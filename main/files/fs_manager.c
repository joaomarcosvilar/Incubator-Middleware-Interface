#include <string.h>
#include <esp_spiffs.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "fs_manager.h"

#define SPIFFS_SEMAPHORE_TIMEOUT_TICKS 5000
#define FS_OBJ_NAME_LEN 300
#define SPIFFS_DIR_PERMISSION 0777
#define TAG  "FS_MANAGER"

SemaphoreHandle_t file_semaphore;

static esp_err_t fs_lock(void)
{
    if (file_semaphore == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (xSemaphoreTake(file_semaphore, SPIFFS_SEMAPHORE_TIMEOUT_TICKS) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t fs_unlock(void)
{
    if (file_semaphore == NULL)
    {
        return ESP_ERR_NOT_FOUND;
    }
    xSemaphoreGive(file_semaphore);
    return ESP_OK;
}

static FILE *fs_file_open(char *path_file, fs_manager_operation_e operation)
{
    switch (operation)
    {
    case SPIFFS_WR:
        return fopen(path_file, "r+");
    case SPIFFS_NW_WR:
        return fopen(path_file, "w+");
    case SPIFFS_NW_W:
        return fopen(path_file, "w");
    case SPIFFS_APPEND_W:
        return fopen(path_file, "a");
    case SPIFFS_READ:
        return fopen(path_file, "r");
    default:
        break;
    }
    return NULL;
}

esp_err_t fs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = ROOT_STORAGE_PATH,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Performing SPIFFS_check().");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(TAG, "SPIFFS_check() successful");
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    if (used > total)
    {
        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
 
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return ESP_FAIL;
        }
        else
        {
            ESP_LOGI(TAG, "SPIFFS_check() successful");
        }
    }

    file_semaphore = xSemaphoreCreateBinary();
    if (file_semaphore == NULL)
    {
        ESP_LOGE(TAG, "Failed init semaphore");
        return ESP_FAIL;
    }

    xSemaphoreGive(file_semaphore);

    ESP_LOGI(TAG, "SPIFFS initialized");
    return ESP_OK;
}

esp_err_t fs_write(char *path_file, uint32_t offset, uint32_t len, uint8_t *buffer, fs_manager_operation_e operation)
{
    esp_err_t res = ESP_FAIL;
    int ret = 0;

    if (path_file == NULL || buffer == NULL)
    {
        return res;
    }

    res = fs_lock();
    if (res != ESP_OK)
    {
        return res;
    }

    FILE *fd = fs_file_open(path_file, operation);
    if (fd == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s", path_file);
        fs_unlock();
        return ESP_FAIL;
    }

    if (offset != 0)
    {
        ret = fseek(fd, offset, SEEK_SET);
        if (ret < 0)
        {
            fclose(fd);
            fs_unlock();
            return ESP_FAIL;
        }
    }

    ret = fwrite((const void *)buffer, sizeof(uint8_t), len, fd);
    if (ret != len)
    {
        ESP_LOGE(TAG, "Written less than expected %d bytes ", ret);
        res = ESP_ERR_INVALID_SIZE;
    }

    ret = fclose(fd);
    if (ret < 0)
    {
        ESP_LOGD(TAG, "Failed to close the file: %s", path_file);
    }

    fs_unlock();

    return ESP_OK;
}

esp_err_t fs_read(char *path_file, uint32_t offset, uint32_t len, uint8_t *buffer)
{
    esp_err_t res = ESP_FAIL;
    int ret = 0;

    if (path_file == NULL || buffer == NULL)
    {
        return res;
    }

    res = fs_lock();
    if (res != ESP_OK)
    {
        return res;
    }

    FILE *fd = fs_file_open(path_file, SPIFFS_READ);
    if (fd == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s", path_file);
        fs_unlock();
        return ESP_FAIL;
    }

    if (offset != 0)
    {
        ret = fseek(fd, offset, SEEK_SET);
        if (ret < 0)
        {
            fclose(fd);
            fs_unlock();
            return ESP_FAIL;
        }
    }

    ret = fread((void *)buffer, sizeof(uint8_t), len, fd);
    if (ret != len)
    {
        ESP_LOGE(TAG, "Read less than expected %d bytes ", ret);
        res = ESP_ERR_INVALID_SIZE;
    }

    ret = fclose(fd);
    if (ret < 0)
    {
        ESP_LOGD(TAG, "Failed to close the file: %s", path_file);
    }

    fs_unlock();
    return res;
}

esp_err_t fs_search(char *path_file, char *target_file)
{
    DIR *dir = NULL;
    struct dirent *ent;
    esp_err_t res = ESP_FAIL;
    char full_path[FS_OBJ_NAME_LEN] = {0};

    if (path_file == NULL || target_file == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return res;
    }

    res = fs_lock();
    if (res != ESP_OK)
    {
        return res;
    }

    dir = opendir(path_file);
    if (dir ==  NULL)
    {
        ESP_LOGE(TAG, "Failed to open the dir %s", path_file);
        fs_unlock();
        return ESP_FAIL;
    }

    res = ESP_ERR_NOT_FOUND;
    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(full_path, FS_OBJ_NAME_LEN, "%s/%s", path_file, ent->d_name);
        if (strcmp(target_file, ent->d_name) == 0)
        {
            struct stat st;
            if (stat(full_path, &st) == 0)
            {
                if (S_ISREG(st.st_mode))
                {
                    ESP_LOGI(TAG, "File found :%s", full_path);
                    res = ESP_OK;
                    break;
                }
            }
        }
    }

    closedir(dir);
    fs_unlock();

    return res;
}

esp_err_t fs_get_file_size(char *path_file, uint32_t *file_size)
{
    int ret = 0;
    if (path_file == NULL || file_size == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t res = fs_lock();
    if (res != ESP_OK)
    {
        return res;
    }

    FILE *fd = fs_file_open(path_file, SPIFFS_READ);
    if (fd == NULL)
    {
        return ESP_FAIL;
    }
    ret = fseek(fd, 0, SEEK_END);
    if (ret < 0)
    {
        *file_size = 0;
        fclose(fd);
        fs_unlock();
        return ESP_FAIL;
    }

    long int size = ftell(fd);
    if (ret >= 0)
    {
        *file_size = (uint32_t)size;
    }
    else
    {
        *file_size = 0;
        res = ESP_ERR_INVALID_SIZE;
    }

    ret = fclose(fd);
    if (ret < 0)
    {
        ESP_LOGD(TAG, "Failed to close the file: %s", path_file);
    }

    fs_unlock();
    return res;
}

esp_err_t spiffs_files_format(void)
{
    esp_err_t ret = esp_spiffs_format(NULL);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS formatted successfully.");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to format SPIFFS (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }
}