#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "shared_data.h"
#include "passwords.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static bool sReady = false;

/* ── Fresh-connection send ───────────────────────────────────────────────── */
/*
 * Every call creates its own WiFiClientSecure + UniversalTelegramBot.
 * gTelegramMutex ensures only ONE SSL context exists at a time — concurrent
 * callers (vGeminiTask + vTelegramTask) block here instead of both
 * allocating ~30 KB of TLS heap simultaneously, which exhausts the heap.
 */
static bool sendOneFreshConnection(const String &msg) {
    if (gTelegramMutex != NULL) {
        if (xSemaphoreTake(gTelegramMutex, pdMS_TO_TICKS(90000)) != pdTRUE) {
            Serial.println("[Telegram] Mutex timeout — send skipped");
            return false;
        }
    }
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15);
    UniversalTelegramBot bot(BOT_TOKEN, client);
    bool ok = bot.sendMessage(CHAT_ID, msg, "");
    if (gTelegramMutex != NULL) xSemaphoreGive(gTelegramMutex);
    return ok;
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
    Serial.println(sent ? "[Test] SUCCESS" : "[Test] FAILED");
}

/* ── Send (public API) ───────────────────────────────────────────────────── */
void Telegram_SendMessage(const String &msg) {
    if (!sReady) {
        Serial.println("[Telegram] Not ready — skipping");
        return;
    }

    /*
     * Event Group wait — block until WIFI_CONNECTED_BIT is set.
     * If WiFi dropped, this task sleeps here until vWiFiKeepAliveTask
     * reconnects and sets the bit — no busy-waiting.
     */
    if (gSystemEvents != NULL) {
        EventBits_t bits = xEventGroupWaitBits(
            gSystemEvents,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
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

/* ── Periodic heartbeat task ─────────────────────────────────────────────── */
/*
 * Simple periodic task — sends sensor heartbeat every 30s.
 * Uses vTaskDelay (not a software timer) to avoid loading the timer
 * daemon task, which has a small shared stack unsuitable for any
 * work involving mutexes or blocking calls.
 */
void vTelegramTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (!sReady) continue;

        float t = 0.0f, h = 0.0f;
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            t = gSensorData.esp_temp;
            h = gSensorData.esp_humidity;
            xSemaphoreGive(gSensorMutex);
        }

        String msg  = "Heartbeat\nTemp: " + String(t, 1) + " C\n";
        msg        += "Humidity: " + String(h, 1) + " %";
        Telegram_SendMessage(msg);
    }
}
