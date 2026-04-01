#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <stdint.h>

/* ── Sensor data ─────────────────────────────────────────────────────────── */
typedef struct {
    /* DHT11 — written by vDHTTask */
    float   esp_temp;
    float   esp_humidity;
    uint8_t led_status;         // 0=green, 1=amber, 2=red

    /* UART — written by vUartRxTask (from MCXC444) */
    uint8_t  tap_event;         // 1 = tap detected
    uint8_t  focus_mode;        // 1 = focus mode active
    uint16_t light_raw;         // ADC 0-4095 (12-bit)
    uint16_t sound_raw;         // ADC 0-4095 (12-bit)
    uint8_t  sound_triggered;   // 1 = sound event this cycle
} SensorData_t;

extern SensorData_t      gSensorData;
extern SemaphoreHandle_t gSensorMutex;   // binary mutex — protects gSensorData
extern SemaphoreHandle_t gTelegramMutex; // binary mutex — serialises ALL SSL sends (Gemini + Telegram, one at a time)

/* ── Inter-task communication ────────────────────────────────────────────── */

/*
 * gSessionReportQueue — Queue (producer-consumer)
 * Producer : vUartRxTask  (via handleFocusTransition on focus 1→0)
 * Consumer : vGeminiTask  (blocks on xQueueReceive, no polling)
 * Depth    : 3 — allows burst of multiple short sessions without dropping
 */
extern QueueHandle_t gSessionReportQueue;

/*
 * gSystemEvents — Event Group (broadcast signalling)
 * Any number of tasks can wait on any combination of bits simultaneously.
 * Unlike a queue, bits are not consumed — all waiters unblock together.
 *
 * WIFI_CONNECTED_BIT  : set/cleared by vWiFiKeepAliveTask
 * SENSOR_READY_BIT    : set by vDHTTask after each valid reading;
 *                       cleared by vMonitorTask after it prints the snapshot
 */
extern EventGroupHandle_t gSystemEvents;
#define WIFI_CONNECTED_BIT   ( 1 << 0 )
#define SENSOR_READY_BIT     ( 1 << 1 )

/*
 * gSampleSemaphore — Counting Semaphore
 * Counts DHT samples accumulated during an active session.
 * vDHTTask gives once per sample; vMonitorTask drains and reports the count.
 * Maximum count = SESSION_MAX_SAMPLES (prevents unbounded accumulation).
 */
#define SESSION_MAX_SAMPLES  30
extern SemaphoreHandle_t gSampleSemaphore;

/* ── Task handles ────────────────────────────────────────────────────────── */
/*
 * Collected in one struct so vMonitorTask can report stack high-water marks
 * for every task, and so uart_rx can notify/resume vDHTTask.
 */
typedef struct {
    TaskHandle_t dht;
    TaskHandle_t uartRx;
    TaskHandle_t gemini;
    TaskHandle_t wifiKeep;
    TaskHandle_t monitor;
    TaskHandle_t serialRx;
} TaskHandles_t;

extern TaskHandles_t gTaskHandles;

#endif // SHARED_DATA_H
