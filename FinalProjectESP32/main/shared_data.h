#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>

typedef struct {
    /* DHT11 — written by vDHTTask */
    float   esp_temp;
    float   esp_humidity;
    uint8_t led_status;         // 0=green, 1=amber, 2=red

    /* UART — written by vUartRxTask (from MCXC444) */
    uint8_t  tap_event;         // 1 = tap detected
    uint8_t  focus_mode;        // 1 = focus mode active
    uint16_t light_raw;         // ADC 0-1023
    uint16_t sound_raw;         // ADC 0-1023
    uint8_t  sound_triggered;   // 1 = sound event this cycle

    /* Gemini — written by vGeminiTask, read by vTelegramTask */
    char     gemini_reply[512]; // last Gemini response, null-terminated

} SensorData_t;

extern SensorData_t      gSensorData;
extern SemaphoreHandle_t gSensorMutex;

#endif // SHARED_DATA_H