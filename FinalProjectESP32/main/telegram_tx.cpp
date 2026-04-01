#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "shared_data.h"
#include "passwords.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool sReady = false;

/*
 * sendOneFreshConnection()
 * Creates a brand-new WiFiClientSecure + UniversalTelegramBot on the stack
 * for every call — completely isolated from any other HTTPS connection
 * (e.g. Gemini). No shared socket state can bleed in.
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

    /* Send boot message — retry 3 times */
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

/* ── Send ────────────────────────────────────────────────────────────────── */
void Telegram_SendMessage(const String &msg) {
    if (!sReady) {
        Serial.println("[Telegram] Not ready — skipping");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
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

/* ── Periodic task ───────────────────────────────────────────────────────── */
void vTelegramTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (!sReady || WiFi.status() != WL_CONNECTED) continue;

        float t = 0.0f;
        float h = 0.0f;

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            t = gSensorData.esp_temp;
            h = gSensorData.esp_humidity;
            xSemaphoreGive(gSensorMutex);
        }

        String msg  = "Temp: "     + String(t, 1) + " C\n";
        msg        += "Humidity: " + String(h, 1) + " %";
        Telegram_SendMessage(msg);
    }
}