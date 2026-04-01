#include <Arduino.h>
#include <WiFi.h>  // Add this line
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "shared_data.h"
#include "dht_sensor.h"
#include "led_rx.h"
#include "uart_rx.h"
#include "uart_tx.h"
#include "api_handler.h"
#include "telegram_tx.h"
#include "session_tracker.h"

/* ── Shared handles ──────────────────────────────────────────────────────── */
SensorData_t      gSensorData  = {0};
SemaphoreHandle_t gSensorMutex = NULL;

/* ── Monitor task ────────────────────────────────────────────────────────── */
void vMonitorTask(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ── Declare the keep-alive task from api_handler ───────────────────────── */
void vWiFiKeepAliveTask(void *pvParameters);  // Forward declaration

void setup() {
    Serial.begin(115200);
    delay(3000);

    gSensorMutex = xSemaphoreCreateMutex();
    Serial.println("System Starting...");
    DHT_Init();
    LED_RX_Init();
    UART_RX_Init();
    UART_TX_Init();  
    Telegram_Init();
    Session_Init();
    
    // Connect WiFi with permanent settings
    connectWiFiGemini();
    
    // Wait for WiFi to stabilize
    delay(3000);
    
    // Start WiFi keep-alive task to maintain connection
    xTaskCreate(vWiFiKeepAliveTask, "WiFiKeep", 2048, NULL, 3, NULL);

    // Create other tasks
    xTaskCreate(vDHTTask,      "DHT",      DHT_TASK_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vMonitorTask,  "Monitor",  2048,                NULL, 1, NULL);
    xTaskCreate(vSerialRxTask, "SerialRX", 2048,                NULL, 2, NULL);
    xTaskCreate(vUartRxTask,   "UartRX",   4096,                NULL, 4, NULL);
    xTaskCreate(vGeminiTask,   "gemini",   GEMINI_TASK_STACK_SIZE, NULL, 2, NULL);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}