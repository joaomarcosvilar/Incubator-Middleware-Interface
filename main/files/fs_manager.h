#ifndef FS_MANAGER_H
#define FS_MANAGER_H

#define ROOT_STORAGE_PATH "/storage"

typedef enum
{
    SPIFFS_WR = 0,      // Abre um arquivo para leitura e escrita. O arquivo deve existir antes de ser modificado.
    SPIFFS_NW_WR,       // Cria um arquivo texto para leitura e escrita.
    SPIFFS_NW_W,        // Abre um arquivo texto para gravar. Se o arquivo nao existe vai ser criado, caso contrario o conteudo anterior sera destruido.
    SPIFFS_APPEND_W,    // Abre um arquivo para armazenar.
    SPIFFS_READ,        // Abre um arquivo para leitura.
}fs_manager_operation_e;

esp_err_t fs_init(void);
esp_err_t fs_read(char *path_file, uint32_t offset, uint32_t len, uint8_t *buffer);
esp_err_t fs_write(char *path_file, uint32_t offset, uint32_t len, uint8_t *buffer, fs_manager_operation_e operation);
esp_err_t fs_search(char *path_file, char *target_file);
esp_err_t fs_get_file_size(char *path_file, uint32_t *file_size);
esp_err_t spiffs_files_format(void);

#endif