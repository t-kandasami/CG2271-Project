#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>

typedef struct {
    int tap_event;
    int focus_mode;
    uint16_t light_raw;
    uint16_t sound_raw;
    uint8_t  sound_triggered;
} SensorData_t;

extern SemaphoreHandle_t gSensorMutex;
extern SemaphoreHandle_t gADCMutex;
extern volatile SensorData_t gSensorData;

void vPrintTask(void *pvParameters);

#endif
