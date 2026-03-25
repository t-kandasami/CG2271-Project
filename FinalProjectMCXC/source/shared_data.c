#include "shared_data.h"
#include "led.h"

#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"

SemaphoreHandle_t gSensorMutex = NULL;
SemaphoreHandle_t gADCMutex = NULL;
volatile SensorData_t gSensorData = { 0, 0, 0, 0, 0, 0 };
volatile int led_state = 0;

TaskHandle_t xTxTaskHandle = NULL;

void vPrintTask(void *pvParameters) {
    (void)pvParameters;
    SensorData_t localData;

    while(1) {
        if (xSemaphoreTake(gSensorMutex, portMAX_DELAY) == pdTRUE) {
            localData.light_raw       = gSensorData.light_raw;
            localData.sound_raw       = gSensorData.sound_raw;
            localData.tap_event       = gSensorData.tap_event;
            localData.sound_triggered = gSensorData.sound_triggered;
            localData.on_off          = gSensorData.on_off;
            localData.paused          = gSensorData.paused;

            /* Clear transients */
            gSensorData.tap_event = 0;
            gSensorData.sound_triggered = 0;

            xSemaphoreGive(gSensorMutex);
        }

//        PRINTF("--- Sensor Status ---\r\n");
//        PRINTF("Light (raw) : %u\r\n", localData.light_raw);
//        PRINTF("Sound (raw) : %u\r\n", localData.sound_raw);
//        PRINTF("Tap Event   : %s\r\n", localData.tap_event ? "YES" : "NO");
//        PRINTF("Sound Event : %s\r\n", localData.sound_triggered ? "YES" : "NO");
//        PRINTF("ON / OFF    : %d\r\n", localData.on_off);
//        PRINTF("Pause       : %d\r\n", localData.paused);
//        PRINTF("---------------------\r\n\r\n");

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

