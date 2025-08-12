#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"

#include <string.h>
#include <fcntl.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_cdcacm.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "application.h"
#include "espnow_manager/espnow_manager.h"
#include "files/fs_manager.h"

#define TAG "APP"

#define PROMPT_STR "incubadora"

#define APPLICATION_TASK_NAME "application task"
#define APPLICATION_TASK_STACK_SIZE 1024 * 1
#define APPLICATION_TASK_PRIOR 2

#define APP_SEMAPHORE_TIMEOUT_TICKS 5000

static TaskHandle_t application_task_handle;
SemaphoreHandle_t app_data_semphr;

static app_data_actuator_t g_app_data = {0};

esp_err_t app_data_lock(void)
{
    if (app_data_semphr == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (xSemaphoreTake(app_data_semphr, APP_SEMAPHORE_TIMEOUT_TICKS) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t app_data_unlock(void)
{
    if (app_data_semphr == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }
    xSemaphoreGive(app_data_semphr);
    return ESP_OK;
}

int cmd_set(int arg, char **argv)
{
    if (argv < 2)
    {
        // printf("Usage: set key:value\n");
        return 1;
    }

    char *input = argv[1];
    char *sep = strchr(input, ':');
    if (!sep)
    {
        // printf("Invalid format! Use key:value\n");
        return 1;
    }

    *sep = '\0'; // separa key e value
    const char *key = input;
    const char *value = sep + 1;

    app_data_lock();

    if (!strcmp(key, "perc_res"))
    {
        g_app_data.perc_res = (uint16_t)strtoul(value, NULL, 10);
    }
    else if (!strcmp(key, "pwm_hum"))
    {
        g_app_data.pwm_hum = (uint16_t)strtoul(value, NULL, 10);
    }
    app_data_unlock();

    // ESP_LOGI(TAG, "Changed:\nperc_res = %d\npwm_hum = %d", g_app_data.perc_res, g_app_data.pwm_hum);
    espnow_manager_send((uint8_t *)&g_app_data, sizeof(g_app_data));

    return 0;
}

int cmd_get(int argc, char **argv)
{
    if (argc < 2)
    {
        return 1;
    }

    char *input = argv[1];
    char *sep = strchr(input, ':');
    if (!sep)
    {
        return 1;
    }

    *sep = '\0';
    const char *key = input;
    int index = atoi(sep + 1);

    app_data_sensors_t data;
    esp_err_t ret = espnow_get_data(&data);
    if (ret != ESP_OK)
    {
        return 1;
    }

    if (!strcmp(key, "hum"))
    {
        if (index >= 0 && index < TEMPERATURE_MAX_SENSOR_COUNT)
        {
            printf("%.2f\n", data.hum);
        }
    }
    else if (!strcmp(key, "temp"))
    {
        if (index >= 0 && index < TEMPERATURE_MAX_SENSOR_COUNT)
        {
            printf("%.2f\n", data.temp[index]);
        }
    }

    return 0;
}

esp_err_t app_register_cmd(void)
{
    const esp_console_cmd_t set = {
        .command = "set",
        .help = "Set actuators (set key_actuator:value)\n\tperc_res -> resistor control \n \t pwm_hum -> humidifier control",
        .hint = NULL,
        .func = &cmd_set};

    esp_err_t ret = esp_console_cmd_register(&set);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed register get cmd");
        return ret;
    }

    const esp_console_cmd_t get = {
        .command = "get",
        .help = "Get values sensors ( set key_sensor:index)\n\ttemp -> array sensor temperature\n\thum -> array sensor humidity",
        .hint = NULL,
        .func = &cmd_get};

    ret = esp_console_cmd_register(&get);

    return ret;
}

esp_err_t application_init(void)
{
    app_data_semphr = xSemaphoreCreateBinary();
    if (app_data_semphr == NULL)
    {
        ESP_LOGE(TAG, "Failed to init semphore");
        return ESP_FAIL;
    }
    xSemaphoreGive(app_data_semphr);

    // Initialize console
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    // repl_config.prompt = PROMPT_STR ">";
    repl_config.prompt = NULL;
    repl_config.max_cmdline_length = 508;

    // Initialize nvs flash
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init nvs flash (E: %s)", esp_err_to_name(err));
        return err;
    }

    esp_console_register_help_command();
    app_register_cmd();

    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init usb cdc (E: %s)", esp_err_to_name(err));
        return err;
    }

    err = esp_console_start_repl(repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start console (E: %s)", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}
