#ifndef APPLICATION_H
#define APPLICATION_H

#define TEMPERATURE_MAX_SENSOR_COUNT 10

esp_err_t application_init(void);

typedef struct
{
    uint16_t perc_res;
    uint16_t pwm_hum;
} app_data_actuator_t;

typedef struct
{
    float temp[TEMPERATURE_MAX_SENSOR_COUNT];
    float hum;
} app_data_sensors_t;


#endif