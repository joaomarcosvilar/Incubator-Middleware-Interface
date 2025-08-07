#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"

#include "application/application.h"
#include "espnow_manager/espnow_manager.h"
#include "files/fs_manager.h"

void app_main(void)
{
    esp_err_t res = fs_init();
    if (res != ESP_OK)
    {
        ESP_LOGE("MAIN", "Failed to init files manager (E: %s)", esp_err_to_name(res));
        return;
    }

    res = espnow_manager_init();
    if (res != ESP_OK)
    {
        ESP_LOGE("MAIN", "Failed to init esp now manager (E: %s)", esp_err_to_name(res));
        return;
    }

    res = application_init();
    if (res != ESP_OK)
    {
        ESP_LOGE("MAIN", "Failed to init application (E: %s)", esp_err_to_name(res));
        return;
    }
}
