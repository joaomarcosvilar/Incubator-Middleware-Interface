#include "esp_err.h"
#include "esp_log.h"
#include "stdio.h"

#include "string.h"
#include "driver/uart.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include <string.h>

#include "espnow_manager.h"
#include "files/fs_manager.h"
#include "application/application.h"

#define TAG "ESPNOW"

#define ESPNOW_QUEUE_LEN 5

#define ESPNOW_TASK_NAME "espnow task"
#define ESPNOW_TASK_STACK_SIZE 1024 * 2
#define ESPNOW_TASK_PRIOR 2

#define ESPNOW_SEMAPHORE_TIMEOUT_TICKS 5 * 1000
// D8:3B:DA:A4:06:9C
static uint8_t g_espnow_manager_peer_mac[6] = {0xD8, 0x3B, 0xDA, 0xA4, 0x06, 0x9C};
static QueueHandle_t espnow_queue;
static TaskHandle_t espnow_task_handle;
SemaphoreHandle_t espnow_sempr;

typedef enum
{
    ESPNOW_ROUTINE_SEND = 0,
    ESPNOW_ROUTINE_RECEIVED
} espnow_routine_e;

typedef struct
{
    uint8_t *data;
    uint32_t size;
    espnow_routine_e routine;
} espnow_queue_t;

static app_data_sensors_t g_espnow_data = {0};

esp_err_t espnow_data_lock(void)
{
    if (espnow_sempr == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (xSemaphoreTake(espnow_sempr, ESPNOW_SEMAPHORE_TIMEOUT_TICKS) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t espnow_data_unlock(void)
{
    if (espnow_sempr == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }
    xSemaphoreGive(espnow_sempr);
    return ESP_OK;
}

esp_err_t espnow_data_update(uint8_t *buffer)
{
    esp_err_t ret = espnow_data_lock();
    if(ret != ESP_OK)
    {
        return ret;
    }

    memcpy(&g_espnow_data, buffer, sizeof(g_espnow_data));
    // ESP_LOGI(TAG,"Sensor: \n hum=%.2f", g_espnow_data.hum);
    // for(int i = 0; i < TEMPERATURE_MAX_SENSOR_COUNT; i++)
    // {
    //     ESP_LOGI(TAG,"temp[%d]=%.2f", i, g_espnow_data.temp[i]);
    // }

    ret = espnow_data_unlock();
    return ret;
}

void espnow_manager_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    // printf("Sent to MAC %02X:%02X:%02X:%02X:%02X:%02X - Status: %s\n",
    //        mac_addr[0], mac_addr[1], mac_addr[2],
    //        mac_addr[3], mac_addr[4], mac_addr[5],
    //        status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    ;
}

void espnow_manager_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    uint8_t *mac = recv_info->src_addr;
    // printf("Received from MAC %02X:%02X:%02X:%02X:%02X:%02X: %.*s\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len, (char *)data);

    espnow_queue_t item = {0};
    item.data = malloc(len);
    memcpy(item.data, data, len);
    item.size = len;
    item.routine = ESPNOW_ROUTINE_RECEIVED;

    xQueueSend(espnow_queue, &item, 0);
}

void espnow_task(void *args)
{
    espnow_queue_t buffer = {0};
    for (;;)
    {
        if (xQueueReceive(espnow_queue, &buffer, pdMS_TO_TICKS(100)) == pdPASS)
        {
            if (buffer.routine == ESPNOW_ROUTINE_SEND)
            {
                esp_now_send(g_espnow_manager_peer_mac, buffer.data, buffer.size);
            }
            else if (buffer.routine == ESPNOW_ROUTINE_RECEIVED)
            {
                espnow_data_update(buffer.data);
            }
            free(buffer.data);
        }
    }
}

esp_err_t espnow_manager_init(void)
{
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        res = nvs_flash_init();
    }
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init nvs flash (E: %s)", esp_err_to_name(res));
        return res;
    }

    // WIFI Initialization
    res = esp_netif_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init esp_net (E: %s)", esp_err_to_name(res));
        return res;
    }

    res = esp_event_loop_create_default();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init event loop (E: %s)", esp_err_to_name(res));
        return res;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    res = esp_wifi_init(&cfg);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init wifi (E: %s)", esp_err_to_name(res));
        return res;
    }

    res = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi storage (E: %s)", esp_err_to_name(res));
        return res;
    }

    res = esp_wifi_set_mode(WIFI_MODE_STA);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi mode (E: %s)", esp_err_to_name(res));
        return res;
    }

    res = esp_wifi_start();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start wifi (E: %s)", esp_err_to_name(res));
        return res;
    }

    res = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi channel (E: %s)", esp_err_to_name(res));
        return res;
    }

    // ESPNOW Initialization
    res = esp_now_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init esp now (E: %s)", esp_err_to_name(res));
        return res;
    }

    res = esp_now_register_recv_cb(espnow_manager_recv_cb);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register reviced callback (E: %s)", esp_err_to_name(res));
        return res;
    }

    res = esp_now_register_send_cb(espnow_manager_send_cb);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to to register send callback (E: %s)", esp_err_to_name(res));
        return res;
    }

    // ESPNOW Peer

    // Show mac address in serial
    // uint8_t mac_l[6] = {0};
    // esp_read_mac(mac_l, ESP_MAC_WIFI_STA);
    // printf("Local MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
    //        mac_l[0], mac_l[1], mac_l[2],
    //        mac_l[3], mac_l[4], mac_l[5]);

    // Find mac address saved in file
    if (!(memcmp(g_espnow_manager_peer_mac, (uint8_t[6]){0}, 6)))
    {
        res = fs_search(ROOT_STORAGE_PATH, ESPNOW_FILE);
        if (res != ESP_ERR_NOT_FOUND)
        {
            printf("Mac address not found. Please, write a address MAC (AA:BB:CC:DD:EE:FF):");
            // TODO: stop initialization and wait cmd
            // res = espnow_manager_read_serial();
            // if (res != ESP_OK)
            // {
            //     return res;
            // }
        }
        else if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get from file (E: %s)", esp_err_to_name(res));
            return res;
        }

        res = fs_read(ESPNOW_FULL_PATH, 0, sizeof(g_espnow_manager_peer_mac), (uint8_t *)g_espnow_manager_peer_mac);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read file (E: %s)", esp_err_to_name(res));
            return res;
        }
    }

    // Initialize peer list
    esp_now_peer_info_t peer = {
        .channel = ESPNOW_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, g_espnow_manager_peer_mac, 6);
    res = esp_now_add_peer(&peer);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add peer mac (E: %s)", esp_err_to_name(res));
        return res;
    }

    espnow_queue = xQueueCreate(ESPNOW_QUEUE_LEN, sizeof(espnow_queue_t));
    if (espnow_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_FAIL;
    }

    vSemaphoreCreateBinary(espnow_sempr);
    if (espnow_sempr == NULL)
    {
        ESP_LOGE(TAG, "Failed to init semphore");
        return ESP_FAIL;
    }
    xSemaphoreGive(espnow_sempr);

    BaseType_t xRet = xTaskCreate(espnow_task, ESPNOW_TASK_NAME, ESPNOW_TASK_STACK_SIZE, NULL, ESPNOW_TASK_PRIOR, &espnow_task_handle);
    if (xRet != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espnow_manager_send(uint8_t *data, uint32_t len)
{
    uint8_t *buf_copy = malloc(len);
    if (buf_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for send buffer");
        return ESP_ERR_NO_MEM;
    }
    memcpy(buf_copy, data, len);

    espnow_queue_t buffer = {
        .data = buf_copy,
        .size = len,
        .routine = ESPNOW_ROUTINE_SEND
    };

    if (xQueueSend(espnow_queue, &buffer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to send to queue");
        free(buf_copy);  // liberar se falhar
        return ESP_FAIL;
    }

    return ESP_OK;
}
    

esp_err_t espnow_get_data(app_data_sensors_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = espnow_data_lock();
    if (ret != ESP_OK) {
        return ret;
    }

    memcpy(data, &g_espnow_data, sizeof(app_data_sensors_t));

    ret = espnow_data_unlock();
    return ret;
}