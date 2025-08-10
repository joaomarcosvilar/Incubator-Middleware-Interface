#ifndef ESPNOW_manager_H
#define ESPNOW_manager_H

#define ESPNOW_CHANNEL 1

#define ESPNOW_FILE "espnow_addr.dat"
#define ESPNOW_FULL_PATH "/storage/espnow_addr.dat"

#include "application/application.h"

// /* Parameters of sending ESPNOW data. */
// typedef struct {
//     bool unicast;                         //Send unicast ESPNOW data.
//     bool broadcast;                       //Send broadcast ESPNOW data.
//     uint8_t state;                        //Indicate that if has received broadcast ESPNOW data or not.
//     uint32_t magic;                       //Magic number which is used to determine which device to send unicast ESPNOW data.
//     uint16_t count;                       //Total count of unicast ESPNOW data to be sent.
//     uint16_t delay;                       //Delay between sending two ESPNOW data, unit: ms.
//     int len;                              //Length of ESPNOW data to be sent, unit: byte.
//     uint8_t *buffer;                      //Buffer pointing to ESPNOW data.
//     uint8_t dest_mac[ESP_NOW_ETH_ALEN];   //MAC address of destination device.
// } espnow_send_param_t;

esp_err_t espnow_manager_init(void);
esp_err_t espnow_manager_send(uint8_t *data, uint32_t len);
esp_err_t espnow_get_data(app_data_sensors_t *data);

#endif