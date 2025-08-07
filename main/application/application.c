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

#include "application.h"
#include "espnow_manager/espnow_manager.h"
#include "files/fs_manager.h"

#define TAG "APP"

#define PROMPT_STR "incubadora"

#define APPLICATION_TASK_NAME "application task"
#define APPLICATION_TASK_STACK_SIZE 1024 * 4
#define APPLICATION_TASK_PRIOR 2

static TaskHandle_t application_task_handle;

void application_task(void *args)
{
    ;
}

esp_err_t application_init(void)
{
    // Initialize console
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = PROMPT_STR ">";
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
