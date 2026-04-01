#include <Arduino.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "shared_data.h"
#include "dht_sensor.h"
#include "led_rx.h"
#include "uart_rx.h"
#include "uart_tx.h"
#include "api_handler.h"
#include "telegram_tx.h"
#include "session_tracker.h"

/* ── Shared data ─────────────────────────────────────────────────────────── */
SensorData_t      gSensorData  = {0};

/* ── FreeRTOS primitives ─────────────────────────────────────────────────── */

/*
 * gSensorMutex — Binary Mutex
 * Protects gSensorData from concurrent access by vDHTTask (writer)
 * and vMonitorTask / vGeminiTask / vTelegramTask (readers).
 */
SemaphoreHandle_t gSensorMutex = NULL;

/*
 * gSessionReportQueue — Queue (producer-consumer IPC)
 * Carries completed SessionSummary_t structs from vUartRxTask to vGeminiTask.
 * Depth 3: allows up to 3 reports to queue while Gemini processes one.
 * Replaces the old mutex+bool polling pattern.
 */
QueueHandle_t gSessionReportQueue = NULL;

/*
 * gSystemEvents — Event Group (broadcast signalling)
 * Bits can be waited on by multiple tasks simultaneously — unlike a queue,
 * the bits are not consumed on read, so all waiters unblock together.
 *   WIFI_CONNECTED_BIT : set/cleared by vWiFiKeepAliveTask
 *   SENSOR_READY_BIT   : set by vDHTTask, cleared by vMonitorTask
 */
EventGroupHandle_t gSystemEvents = NULL;

/*
 * gSampleSemaphore — Counting Semaphore
 * Counts DHT samples accumulated during an active session.
 * Max count = SESSION_MAX_SAMPLES. vDHTTask gives; vMonitorTask reads & drains.
 */
SemaphoreHandle_t gSampleSemaphore = NULL;

/*
 * gTaskHandles — Task Handles
 * Saved at xTaskCreate time so vMonitorTask can report stack high-water marks,
 * and so vUartRxTask can notify/resume vDHTTask on session start.
 */
TaskHandles_t gTaskHandles = {NULL};

/* ── Monitor task ────────────────────────────────────────────────────────── */
void vMonitorTask(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        /*
         * Event Group wait — block until vDHTTask sets SENSOR_READY_BIT.
         * pdTRUE: clear the bit on exit so we wait for the next fresh reading.
         * Timeout 10s: print anyway if somehow no reading arrives in time.
         * This replaces the fixed vTaskDelay(5000) — monitor now prints
         * in sync with actual sensor updates, not on an arbitrary timer.
         */
        xEventGroupWaitBits(gSystemEvents, SENSOR_READY_BIT,
                            pdTRUE,   // clear bit on exit
                            pdTRUE,   // wait for ALL listed bits
                            pdMS_TO_TICKS(10000));

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            Serial.println("======= Shared Data Snapshot =======");
            Serial.print("  ESP Temp:       "); Serial.print(gSensorData.esp_temp, 1);     Serial.println(" C");
            Serial.print("  ESP Humidity:   "); Serial.print(gSensorData.esp_humidity, 1); Serial.println(" %");
            Serial.print("  Focus Mode:     "); Serial.println(gSensorData.focus_mode);
            Serial.print("  Light Raw:      "); Serial.println(gSensorData.light_raw);
            Serial.print("  Sound Raw:      "); Serial.println(gSensorData.sound_raw);
            Serial.print("  Sound Trigger:  "); Serial.println(gSensorData.sound_triggered);
            xSemaphoreGive(gSensorMutex);
        }

        /*
         * Counting Semaphore — report accumulated session samples.
         * uxSemaphoreGetCount reads the current count without consuming it.
         * The while loop then drains it so the count resets for the next window.
         */
        UBaseType_t sampleCount = uxSemaphoreGetCount(gSampleSemaphore);
        if (sampleCount > 0) {
            Serial.printf("  Session samples since last snapshot: %u\n",
                          (unsigned)sampleCount);
            while (xSemaphoreTake(gSampleSemaphore, 0) == pdTRUE) {}
        }

        /*
         * Stack High-Water Mark — reports the minimum free stack (in words)
         * ever recorded for each task. A value approaching 0 means stack
         * overflow is imminent and the task's stack size must be increased.
         * This is the most reliable way to catch stack sizing bugs before
         * they cause silent crashes.
         */
        Serial.println("======= Stack High-Water Marks ======");
        if (gTaskHandles.dht      != NULL)
            Serial.printf("  DHT:       %4u words free\n", uxTaskGetStackHighWaterMark(gTaskHandles.dht));
        if (gTaskHandles.uartRx   != NULL)
            Serial.printf("  UartRX:    %4u words free\n", uxTaskGetStackHighWaterMark(gTaskHandles.uartRx));
        if (gTaskHandles.gemini   != NULL)
            Serial.printf("  Gemini:    %4u words free\n", uxTaskGetStackHighWaterMark(gTaskHandles.gemini));
        if (gTaskHandles.wifiKeep != NULL)
            Serial.printf("  WiFiKeep:  %4u words free\n", uxTaskGetStackHighWaterMark(gTaskHandles.wifiKeep));
        if (gTaskHandles.serialRx != NULL)
            Serial.printf("  SerialRX:  %4u words free\n", uxTaskGetStackHighWaterMark(gTaskHandles.serialRx));
        if (gTaskHandles.monitor  != NULL)
            Serial.printf("  Monitor:   %4u words free\n", uxTaskGetStackHighWaterMark(gTaskHandles.monitor));
        Serial.printf("  Free heap: %u bytes\n", (unsigned)esp_get_free_heap_size());
        Serial.println("=====================================");
    }
}

/* ── Setup ───────────────────────────────────────────────────────────────── */
void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("=== Study Coach ESP32 Boot ===");

    /* ── Create FreeRTOS primitives before any task uses them ── */

    /* Binary mutex — protects gSensorData */
    gSensorMutex = xSemaphoreCreateMutex();

    /*
     * Queue — carries SessionSummary_t from vUartRxTask to vGeminiTask.
     * Depth 3, item size = sizeof(SessionSummary_t).
     */
    gSessionReportQueue = xQueueCreate(3, sizeof(SessionSummary_t));

    /*
     * Event Group — WIFI_CONNECTED_BIT and SENSOR_READY_BIT.
     * Broadcast: multiple tasks can wait on the same bits simultaneously.
     */
    gSystemEvents = xEventGroupCreate();

    /*
     * Counting Semaphore — tracks DHT samples during a session.
     * Max count = SESSION_MAX_SAMPLES, initial count = 0.
     */
    gSampleSemaphore = xSemaphoreCreateCounting(SESSION_MAX_SAMPLES, 0);

    /* Hardware init — no network needed */
    DHT_Init();
    LED_RX_Init();
    UART_RX_Init();
    UART_TX_Init();
    Session_Init();

    /*
     * Network init — ORDER MATTERS:
     * 1. connectWiFiGemini() connects WiFi and configures aiClient
     * 2. Set WIFI_CONNECTED_BIT so tasks waiting on it unblock immediately
     * 3. Telegram_Init() uses existing connection — no reconnect needed
     */
    connectWiFiGemini();
    if (WiFi.status() == WL_CONNECTED) {
        xEventGroupSetBits(gSystemEvents, WIFI_CONNECTED_BIT);
        Serial.println("[Boot] WIFI_CONNECTED_BIT set");
    }
    delay(1000);
    Telegram_Init();
    testTelegram();

    /* ── Create tasks — save handles for stack monitoring and notifications ── */
    xTaskCreate(vWiFiKeepAliveTask, "WiFiKeep", 4096,                   NULL, 3, &gTaskHandles.wifiKeep);
    xTaskCreate(vDHTTask,           "DHT",       DHT_TASK_STACK_SIZE,   NULL, 2, &gTaskHandles.dht);
    xTaskCreate(vMonitorTask,       "Monitor",   4096,                   NULL, 1, &gTaskHandles.monitor);
    xTaskCreate(vSerialRxTask,      "SerialRX",  2048,                   NULL, 2, &gTaskHandles.serialRx);
    xTaskCreate(vUartRxTask,        "UartRX",    4096,                   NULL, 4, &gTaskHandles.uartRx);
    xTaskCreate(vGeminiTask,        "Gemini",    GEMINI_TASK_STACK_SIZE, NULL, 2, &gTaskHandles.gemini);
    xTaskCreate(vTelegramTask,      "Telegram",  TELEGRAM_TASK_STACK_SIZE, NULL, TELEGRAM_TASK_PRIORITY, NULL);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
