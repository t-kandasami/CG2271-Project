#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "shared_data.h"
#include "passwords.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static WiFiClientSecure     *pClient = NULL;
static UniversalTelegramBot *pBot    = NULL;

/* ── Init ────────────────────────────────────────────────────────────────── */
void Telegram_Init(void) {
    Serial.println("[Telegram] Initialising...");

    // Simple WiFi connect — no static IP, no power settings
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[Telegram] Connecting to WiFi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[Telegram] WiFi failed");
        return;
    }

    Serial.println("\n[Telegram] WiFi connected");
    Serial.print("[Telegram] IP: ");
    Serial.println(WiFi.localIP());

    // Create client — setInsecure skips SSL cert verification
    pClient = new WiFiClientSecure();
    pClient->setInsecure();
    pClient->setTimeout(15);   // 15s timeout for slow connections

    delay(1000);

    // Create bot after client is fully ready
    pBot = new UniversalTelegramBot(BOT_TOKEN, *pClient);

    delay(1000);

    Serial.println("[Telegram] Bot created — sending test message");

    // Retry boot message 3 times
    bool sent = false;
    for (int i = 0; i < 3 && !sent; i++) {
        Serial.printf("[Telegram] Attempt %d/3\n", i + 1);
        sent = pBot->sendMessage(CHAT_ID, "ESP32 online!", "");
        if (!sent) {
            Serial.println("[Telegram] Failed — waiting 3s");
            delay(3000);
        }
    }

    if (sent) {
        Serial.println("[Telegram] Boot message sent!");
    } else {
        Serial.println("[Telegram] Boot message failed — check token and chat ID");
    }
}

/* ── Send ────────────────────────────────────────────────────────────────── */
void Telegram_SendMessage(const String &msg) {
    if (pBot == NULL) {
        Serial.println("[Telegram] Not initialised");
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Telegram] No WiFi");
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
        sent = pBot->sendMessage(CHAT_ID, msg, "");
        if (!sent && i < 2) {
            Serial.printf("[Telegram] Attempt %d failed — retrying\n", i + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    Serial.println(sent ? "[Telegram] Sent!" : "[Telegram] Failed");
}

/* ── Periodic task ───────────────────────────────────────────────────────── */
void vTelegramTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (pBot == NULL || WiFi.status() != WL_CONNECTED) continue;

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