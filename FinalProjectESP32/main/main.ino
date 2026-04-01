#include <Arduino.h>
#include <WiFi.h>
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

SensorData_t      gSensorData  = {0};
SemaphoreHandle_t gSensorMutex = NULL;

void vMonitorTask(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            Serial.println("======= Shared Data Snapshot =======");
            Serial.print("  ESP Temp:       "); Serial.print(gSensorData.esp_temp, 1);     Serial.println(" C");
            Serial.print("  ESP Humidity:   "); Serial.print(gSensorData.esp_humidity, 1); Serial.println(" %");
            Serial.print("  Focus Mode:     "); Serial.println(gSensorData.focus_mode);
            Serial.print("  Light Raw:      "); Serial.println(gSensorData.light_raw);
            Serial.print("  Sound Raw:      "); Serial.println(gSensorData.sound_raw);
            Serial.print("  Sound Trigger:  "); Serial.println(gSensorData.sound_triggered);
            Serial.println("=====================================");
            xSemaphoreGive(gSensorMutex);
        }
    }
}



void setup() {
    Serial.begin(115200);
    delay(3000);

    gSensorMutex = xSemaphoreCreateMutex();
    Serial.println("=== Study Coach ESP32 Boot ===");

    /* Hardware init first — no network needed */
    DHT_Init();
    LED_RX_Init();
    UART_RX_Init();
    UART_TX_Init();
    Session_Init();
    
    /*
     * Network init — ORDER MATTERS:
     * 1. connectWiFiGemini() connects WiFi and configures aiClient
     * 2. Telegram_Init() uses existing WiFi connection — no reconnect needed
     */
    connectWiFiGemini();
    delay(1000);
    Telegram_Init();     // WiFi already up — just creates client + bot
    testTelegram();
    
    /* Tasks */
    xTaskCreate(vWiFiKeepAliveTask, "WiFiKeep", 2048,                NULL, 3, NULL);
    xTaskCreate(vDHTTask,           "DHT",       DHT_TASK_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vMonitorTask,       "Monitor",   2048,                NULL, 1, NULL);
    xTaskCreate(vSerialRxTask,      "SerialRX",  2048,                NULL, 2, NULL);
    xTaskCreate(vUartRxTask,        "UartRX",    4096,                NULL, 4, NULL);
    xTaskCreate(vGeminiTask,        "Gemini",    GEMINI_TASK_STACK_SIZE, NULL, 2, NULL);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}

