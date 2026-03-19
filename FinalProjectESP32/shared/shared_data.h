#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>

typedef struct {
    float   esp_temp;
    float   esp_humidity;
    uint8_t led_status;      // 0 = green, 1 = amber, 2 = red
    uint16_t light;          // from MCXC444 via UART (coming soon)
    uint16_t distance_cm;    // from MCXC444 via UART (coming soon)
    uint8_t  focus_mode;     // from MCXC444 via UART (coming soon)
    uint8_t  tap_event;      // from MCXC444 via UART (coming soon)
} SensorData_t;

extern SensorData_t      gSensorData;
extern SemaphoreHandle_t gSensorMutex;

#endif // SHARED_DATA_H