#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "application.h"
#include "espnow_manager/espnow_manager.h"

static const char *TAG = "APP_MAIN";

#define LINE_BUFFER_SIZE 256

#define COMMAND_GET_VALUE "get"
#define COMMAND_SET_VALUE "set"

#define APPLICATION_SERIAL_TASK_NAME "serial task"
#define APPLICATION_SERIAL_TASK_STACK_SIZE 1024 * 3
#define APPLICATION_SERIAL_TASK_PRIOR 3

#define APPLICATION_TASK_NAME "application task"
#define APPLICATION_TASK_STACK_SIZE 1024 * 2
#define APPLICATION_TASK_PRIOR 2

#define APP_QUEUE_LEN 5

typedef enum
{
    CMD_GET = 0,
    CMD_SET
} app_command_e;

typedef struct
{
    app_command_e event;
    char dev[16];
    uint16_t num;
} app_queue_t;

static QueueHandle_t app_queue;
static app_data_actuator_t g_app_actuator = {0};

esp_err_t app_serial_comand_process(char *read_serial)
{
    char *command_copy = strdup(read_serial);
    if (command_copy == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    char *saveptr;

    char *command = strtok_r(command_copy, " ", &saveptr);
    if (!command)
    {
        free(command_copy);
        return ESP_ERR_INVALID_ARG;
    }

    char *dev = strtok_r(NULL, ":", &saveptr);
    if (!dev)
    {
        free(command_copy);
        return ESP_ERR_INVALID_ARG;
    }

    char *num = strtok_r(NULL, "", &saveptr);
    if (!num)
    {
        free(command_copy);
        return ESP_ERR_INVALID_ARG;
    }

    // ESP_LOGI(TAG, "Reconhecido command: %s, dev: %s e num: %s", command, dev, num);

    app_queue_t buffer = {0};
    if (!strcmp(command, COMMAND_GET_VALUE))
    {
        buffer.event = CMD_GET;
    }
    else if (!strcmp(command, COMMAND_SET_VALUE))
    {
        buffer.event = CMD_SET;
    }

    strncpy(buffer.dev, dev, sizeof(buffer.dev) - 1);  
    buffer.dev[sizeof(buffer.dev) - 1] = '\0';  
    
    char *endptr;
    long num_val = strtol(num, &endptr, 10);
    
    if (endptr == num || num_val < 0 || num_val > UINT16_MAX)
    {
        free(command_copy);
        return ESP_ERR_INVALID_ARG;
    }
    
    buffer.num = (uint16_t)num_val;

    if (xQueueSend(app_queue, &buffer, 0) != pdPASS)
    {
        free(command_copy);
        return ESP_FAIL;
    }

    free(command_copy);

    return ESP_OK;
}

void app_serial_task(void *args)
{
    uint8_t data[128];
    char line_buffer[LINE_BUFFER_SIZE];
    int line_pos = 0;

    while (1)
    {
        int len = usb_serial_jtag_read_bytes(data, sizeof(data), pdMS_TO_TICKS(50000));

        if (len > 0)
        {
            for (int i = 0; i < len; i++)
            {
                char c = (char)data[i];

                if (c == '\n' || c == '\r')
                {
                    if (line_pos > 0)
                    {
                        line_buffer[line_pos] = '\0';
                        // ESP_LOGI(TAG, "Linha completa recebida: '%s'", line_buffer);
                        app_serial_comand_process(line_buffer);
                    }
                    line_pos = 0;
                }
                else if (line_pos < LINE_BUFFER_SIZE - 1)
                {
                    line_buffer[line_pos++] = c;
                }
            }
        }
    }
}

esp_err_t app_get_routine(char *dev, uint16_t num)
{
    if (num > TEMPERATURE_MAX_SENSOR_COUNT)
    {
        return ESP_ERR_INVALID_ARG;
    }

    app_data_sensors_t buffer = {0};
    esp_err_t ret = espnow_get_data(&buffer);
    if (ret != ESP_OK)
    {
        return ret;
    }

    if (!strcmp(dev, "hum"))
    {
        printf("%.2f\n", buffer.hum);
        fflush(stdout);
    }
    else if (!strcmp(dev, "temp"))
    {
        printf("%.2f\n", buffer.temp[num]);
        fflush(stdout);
    }
    else if(!strcmp(dev, "temp_ext"))
    {
        printf("%.2f\n", buffer.temp_external);
        fflush(stdout);
    }
    else if(!strcmp(dev, "hum_ext"))
    {
        printf("%.2f\n", buffer.hum_external);
        fflush(stdout);
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t app_set_routine(char *dev, uint16_t value)
{
    if (value > 100)
    {
        value = 100;
    }

    if (!strcmp(dev, "hum"))
    {
        g_app_actuator.pwm_hum = value;
    }
    else if (!strcmp(dev, "temp"))
    {
        g_app_actuator.perc_res = value;
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret =espnow_manager_send((uint8_t *)&g_app_actuator, sizeof(g_app_actuator));

    return ret;
}

void app_task(void *args)
{
    app_queue_t action;
    esp_err_t ret = ESP_OK;

    while (true)
    {
        if (xQueueReceive(app_queue, &action, pdMS_TO_TICKS(20)) == pdPASS)
        {
            switch (action.event)
            {
            case CMD_GET:
                ret = app_get_routine(action.dev, action.num);
                break;

            case CMD_SET:
                ret = app_set_routine(action.dev, action.num);
                break;

            default:
                ret = ESP_ERR_INVALID_ARG;
                break;
            }

            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed routine (E: %s)", esp_err_to_name(ret));
            }
        }
    }
}

esp_err_t application_init(void)
{
    usb_serial_jtag_driver_config_t cfg = {
        .tx_buffer_size = 256 * 2,
        .rx_buffer_size = 256 * 2};
    esp_err_t ret = usb_serial_jtag_driver_install(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to config USB JTAG driver (E: %s)", esp_err_to_name(ret));
        return ret;
    }

    app_queue = xQueueCreate(APP_QUEUE_LEN, sizeof(app_queue_t));
    if (!app_queue)
    {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_FAIL;
    }

    BaseType_t Retx = xTaskCreate(app_serial_task,
                                  APPLICATION_SERIAL_TASK_NAME,
                                  APPLICATION_SERIAL_TASK_STACK_SIZE,
                                  NULL,
                                  APPLICATION_SERIAL_TASK_PRIOR,
                                  NULL);
    if (Retx != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to create serial task");
        return ESP_FAIL;
    }

    Retx = xTaskCreate(app_task,
                       APPLICATION_TASK_NAME,
                       APPLICATION_TASK_STACK_SIZE,
                       NULL,
                       APPLICATION_TASK_PRIOR,
                       NULL);
    if (Retx != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to create app task");
        return ESP_FAIL;
    }

    // app_queue_t msg =
    //     {
    //         .dev = "temp",
    //         .event = CMD_GET,
    //         .num = 0};

    // while (true)
    // {
    //     xQueueSend(app_queue, &msg, pdMS_TO_TICKS(5000));
    //     vTaskDelay(pdMS_TO_TICKS(5000));
    // }

    return ESP_OK;
}