#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "shared_data.h"
#include "passwords.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

static bool sReady = false;

/*
 * gTelegramQueue — Queue (timer callback → vTelegramTask)
 *
 * Software timer callbacks run inside the FreeRTOS timer daemon task.
 * The daemon's stack is small and shared — it must NEVER block or do
 * slow work like TLS HTTPS calls.
 *
 * Pattern: the timer callback posts a message string into this queue
 * (fast, non-blocking), then vTelegramTask (dedicated stack 8192)
 * drains the queue and does the actual HTTPS send.
 *
 * This correctly decouples the periodic event source (timer) from the
 * slow handler (TLS), which is the canonical FreeRTOS timer design.
 */
#define TELEGRAM_MSG_LEN   512
typedef struct {
    char text[TELEGRAM_MSG_LEN];
} TelegramMsg_t;

static QueueHandle_t sTelegramQueue = NULL;

/* ── Fresh-connection send ───────────────────────────────────────────────── */
/*
 * Every call creates its own WiFiClientSecure + UniversalTelegramBot on
 * the stack — completely isolated from Gemini's HTTPS connection.
 * No shared socket state can bleed between the two pipelines.
 */
static bool sendOneFreshConnection(const String &msg) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);
    UniversalTelegramBot bot(BOT_TOKEN, client);
    return bot.sendMessage(CHAT_ID, msg, "");
}

/* ── Init ────────────────────────────────────────────────────────────────── */
void Telegram_Init(void) {
    Serial.println("[Telegram] Initialising...");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[Telegram] WiFi not ready — init aborted");
        return;
    }

    Serial.println("\n[Telegram] WiFi confirmed");

    /* Create the internal message queue — depth 5 */
    sTelegramQueue = xQueueCreate(5, sizeof(TelegramMsg_t));
    if (sTelegramQueue == NULL) {
        Serial.println("[Telegram] Queue creation failed!");
        return;
    }

    /* Send boot message */
    for (int i = 0; i < 3; i++) {
        Serial.printf("[Telegram] Boot message attempt %d/3\n", i + 1);
        if (sendOneFreshConnection("ESP32 online!")) {
            Serial.println("[Telegram] Boot message sent!");
            sReady = true;
            return;
        }
        delay(3000);
    }

    Serial.println("[Telegram] Boot message failed — will retry on first send");
    sReady = true;
}

/* ── Test ────────────────────────────────────────────────────────────────── */
void testTelegram(void) {
    Serial.println("[Test] Starting Telegram debug...");
    Serial.print("[Test] WiFi status: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
    Serial.print("[Test] IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("[Test] Sending test message via fresh connection...");
    bool sent = sendOneFreshConnection("Test message");
    Serial.print("[Test] sendMessage result: ");
    Serial.println(sent ? "SUCCESS" : "FAILED");
}

/* ── Send (public API) ───────────────────────────────────────────────────── */
void Telegram_SendMessage(const String &msg) {
    if (!sReady) {
        Serial.println("[Telegram] Not ready — skipping");
        return;
    }

    /*
     * Event Group wait — block until WIFI_CONNECTED_BIT is set.
     * Replaces the inline WiFi.status() poll with a proper blocking wait.
     * If WiFi dropped, this task sleeps here until vWiFiKeepAliveTask
     * reconnects and sets the bit — no busy-waiting, no dropped messages.
     */
    if (gSystemEvents != NULL) {
        EventBits_t bits = xEventGroupWaitBits(
            gSystemEvents,
            WIFI_CONNECTED_BIT,
            pdFALSE,              // do not clear the bit
            pdTRUE,               // wait for all bits listed
            pdMS_TO_TICKS(20000));
        if (!(bits & WIFI_CONNECTED_BIT)) {
            Serial.println("[Telegram] WiFi wait timeout — message dropped");
            return;
        }
    } else if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Telegram] No WiFi — message dropped");
        return;
    }

    if (msg.isEmpty()) {
        Serial.println("[Telegram] Empty message — skipped");
        return;
    }

    Serial.println("[Telegram] Sending:");
    Serial.println(msg);

    bool sent = false;
    for (int i = 0; i < 3 && !sent; i++) {
        Serial.printf("[Telegram] Attempt %d/3\n", i + 1);
        sent = sendOneFreshConnection(msg);
        Serial.println(sent ? "[Telegram] OK" : "[Telegram] Failed");
        if (!sent && i < 2) {
            Serial.println("[Telegram] Retrying in 3s");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }

    Serial.println(sent ? "[Telegram] Sent!" : "[Telegram] All attempts failed");
}

/* ── Software Timer heartbeat ────────────────────────────────────────────── */
/*
 * Software Timer callback — runs inside the FreeRTOS timer daemon task.
 *
 * RULE: timer callbacks must not block and must not call slow APIs.
 * This callback only reads sensor data (mutex, fast) and posts to a queue
 * (xQueueSend with timeout 0, non-blocking).
 * The actual HTTPS send happens in vTelegramTask below.
 */
static TimerHandle_t sTelegramTimer = NULL;

static void telegramTimerCallback(TimerHandle_t xTimer) {
    if (sTelegramQueue == NULL) return;

    float t = 0.0f, h = 0.0f;
    if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        t = gSensorData.esp_temp;
        h = gSensorData.esp_humidity;
        xSemaphoreGive(gSensorMutex);
    }

    TelegramMsg_t m;
    snprintf(m.text, sizeof(m.text),
             "Heartbeat\nTemp: %.1f C\nHumidity: %.1f %%", t, h);

    /* Non-blocking send — drop silently if queue is full (daemon task must not block) */
    if (xQueueSend(sTelegramQueue, &m, 0) != pdTRUE) {
        Serial.println("[Telegram] Heartbeat queue full — dropped");
    }
}

void Telegram_StartPeriodicHeartbeat(uint32_t period_ms) {
    if (sTelegramQueue == NULL) {
        Serial.println("[Telegram] Queue not ready — heartbeat not started");
        return;
    }

    sTelegramTimer = xTimerCreate(
        "TelegramHB",
        pdMS_TO_TICKS(period_ms),
        pdTRUE,             // auto-reload: repeating timer
        NULL,
        telegramTimerCallback);

    if (sTelegramTimer != NULL) {
        xTimerStart(sTelegramTimer, 0);
        Serial.printf("[Telegram] Heartbeat timer started — period %lums\n",
                      (unsigned long)period_ms);
    } else {
        Serial.println("[Telegram] Timer creation failed");
    }
}

/* ── Telegram task — drains the queue and does the actual HTTPS send ─────── */
/*
 * vTelegramTask blocks on xQueueReceive(portMAX_DELAY) — zero CPU usage
 * when idle. It wakes only when the software timer (or any other caller)
 * posts to sTelegramQueue. The 8192-byte stack is needed for TLS.
 */
void vTelegramTask(void *pvParameters) {
    (void)pvParameters;

    /* Wait for sTelegramQueue to be created by Telegram_Init */
    while (sTelegramQueue == NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    TelegramMsg_t m;
    while (1) {
        /*
         * Queue Receive — block until the timer callback (or anyone else)
         * posts a message. portMAX_DELAY = sleep forever until data arrives.
         */
        if (xQueueReceive(sTelegramQueue, &m, portMAX_DELAY) == pdTRUE) {
            Telegram_SendMessage(String(m.text));
        }
    }
}
