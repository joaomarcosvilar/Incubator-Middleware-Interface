#include "esp_err.h"
#include "esp_log.h"
#include "stdio.h"

#include "driver/uart.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include <string.h>

#include "espnow_manager.h"
#include "files/fs_manager.h"

#define TAG "ESPNOW"

#define ESPNOW_QUEUE_LEN 5

// D8:3B:DA:A4:06:9C
static uint8_t g_espnow_manager_peer_mac[6] = {0xD8, 0x3B, 0xDA, 0xA4, 0x06, 0x9C};
static QueueHandle_t espnow_queue;

esp_err_t espnow_manager_read_serial(void)
{
    uint8_t buffer[128] = {0};
    int len = 0;
    while (1)
    {
        len += uart_read_bytes(UART_NUM_0, buffer + len, 128 - len - 1, pdMS_TO_TICKS(1000));
        buffer[len] = '\0';

        char *newline = strchr((char *)buffer, '\n');
        if (newline)
        {
            *newline = '\0';
            break;
        }
    }

    ESP_LOGI(TAG, "MAC recebido: %s", buffer);

    if (sscanf((char *)buffer, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &g_espnow_manager_peer_mac[0], &g_espnow_manager_peer_mac[1], &g_espnow_manager_peer_mac[2],
               &g_espnow_manager_peer_mac[3], &g_espnow_manager_peer_mac[4], &g_espnow_manager_peer_mac[5]) != 6)

    {
        ESP_LOGE(TAG, "Invalid MAC address");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

void espnow_manager_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    printf("Sent to MAC %02X:%02X:%02X:%02X:%02X:%02X - Status: %s\n",
           mac_addr[0], mac_addr[1], mac_addr[2],
           mac_addr[3], mac_addr[4], mac_addr[5],
           status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void espnow_manager_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    uint8_t *mac = recv_info->src_addr;
    printf("Received from MAC %02X:%02X:%02X:%02X:%02X:%02X: %.*s\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len, (char *)data);
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
    uint8_t mac_l[6] = {0};
    esp_read_mac(mac_l, ESP_MAC_WIFI_STA);
    printf("Local MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac_l[0], mac_l[1], mac_l[2],
           mac_l[3], mac_l[4], mac_l[5]);

    // Find mac address saved in file
    if (!(memcmp(g_espnow_manager_peer_mac, (uint8_t[6]){0}, 6)))
    {
        res = fs_search(ROOT_STORAGE_PATH, ESPNOW_manager_FILE);
        if (res != ESP_ERR_NOT_FOUND)
        {
            printf("Mac address not found. Please, write a address MAC (AA:BB:CC:DD:EE:FF):");
            res = espnow_manager_read_serial();
            if (res != ESP_OK)
            {
                return res;
            }
        }
        else if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get from file (E: %s)", esp_err_to_name(res));
            return res;
        }

        res = fs_read(ESPNOW_manager_FULL_PATH, 0, sizeof(g_espnow_manager_peer_mac), (uint8_t *)g_espnow_manager_peer_mac);
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

    // while (1)
    // {
    //     esp_now_send(g_espnow_manager_peer_mac, (uint8_t *)("Hello S3!"), 10);
    //     vTaskDelay(pdMS_TO_TICKS(5000));
    // }
    // espnow_queue = xQueueCreate(ESPNOW_QUEUE_LEN, sizeof());

    return ESP_OK;
}

esp_err_t espnow_manager_send(uint8_t *data, uint32_t len)
{
    return ESP_OK;
}