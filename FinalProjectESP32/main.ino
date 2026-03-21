#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "shared_data.h"
#include "dht_sensor.h"

/* ── Shared handles ──────────────────────────────────────────────────────── */
SensorData_t      gSensorData  = {0};
SemaphoreHandle_t gSensorMutex = NULL;

/* ── Monitor task ────────────────────────────────────────────────────────── */
void vMonitorTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            Serial.println("======= Shared Data Snapshot =======");
            Serial.print("  ESP Temp:     "); Serial.print(gSensorData.esp_temp, 1);     Serial.println(" C");
            Serial.print("  ESP Humidity: "); Serial.print(gSensorData.esp_humidity, 1); Serial.println(" %");
            Serial.print("  LED Status:   ");
            if      (gSensorData.led_status == 2) Serial.println("RED");
            else if (gSensorData.led_status == 1) Serial.println("AMBER");
            else                                  Serial.println("GREEN");
            Serial.println("=====================================");
            xSemaphoreGive(gSensorMutex);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Study Coach ESP32 Boot ===");

    /* Step 1: Create shared handles */
    gSensorMutex = xSemaphoreCreateMutex();

    /* Step 2: Init modules */
    DHT_Init();

    /* Step 3: Create tasks */
    xTaskCreate(vDHTTask,     "DHT",     DHT_TASK_STACK_SIZE, NULL, DHT_TASK_PRIORITY, NULL);
    xTaskCreate(vMonitorTask, "Monitor", 2048,                NULL, 1,                 NULL);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}