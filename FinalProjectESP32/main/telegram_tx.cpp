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
static bool                  sReady  = false;   // true only when bot is confirmed working

/* ── Init ────────────────────────────────────────────────────────────────── */
void Telegram_Init(void) {
    Serial.println("[Telegram] Initialising...");

    /* Wait for WiFi — must be connected before creating SSL client */
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

    Serial.println("\n[Telegram] WiFi confirmed — creating client");

    /* Clean up any previous instance */
    if (pBot)    { delete pBot;    pBot    = NULL; }
    if (pClient) { delete pClient; pClient = NULL; }

    pClient = new WiFiClientSecure();
    pClient->setInsecure();
    pClient->setTimeout(15);
    delay(500);

    pBot = new UniversalTelegramBot(BOT_TOKEN, *pClient);
    delay(1000);

    /* Send boot message — retry 3 times */
    for (int i = 0; i < 3; i++) {
        Serial.printf("[Telegram] Boot message attempt %d/3\n", i + 1);
        if (pBot->sendMessage(CHAT_ID, "ESP32 online!", "")) {
            Serial.println("[Telegram] Boot message sent!");
            sReady = true;
            return;
        }
        delay(3000);
    }

    Serial.println("[Telegram] Boot message failed — bot created but unverified");
    /*
     * Still mark as ready — pBot is valid even if boot message failed.
     * The boot message failure could be a transient network issue.
     * Let later sends attempt anyway.
     */
    sReady = true;
}

void testTelegram(void) {
    Serial.println("[Test] Starting Telegram debug...");
    
    // Check 1 — WiFi
    Serial.print("[Test] WiFi status: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
    Serial.print("[Test] IP: ");
    Serial.println(WiFi.localIP());
    
    // Check 2 — pointers
    Serial.print("[Test] pClient: ");
    Serial.println(pClient == NULL ? "NULL" : "OK");
    Serial.print("[Test] pBot: ");
    Serial.println(pBot == NULL ? "NULL" : "OK");
    Serial.print("[Test] sReady: ");
    Serial.println(sReady ? "true" : "false");
    
    // Check 3 — raw TCP connection to Telegram server
    Serial.println("[Test] Attempting raw TCP to api.telegram.org:443...");
    if (pClient->connect("api.telegram.org", 443)) {
        Serial.println("[Test] TCP connection SUCCESS");
        pClient->stop();
    } else {
        Serial.println("[Test] TCP connection FAILED — SSL or network issue");
    }
    
    // Check 4 — direct sendMessage
    Serial.println("[Test] Sending test message...");
    bool sent = pBot->sendMessage(CHAT_ID, "Test message", "");
    Serial.print("[Test] sendMessage result: ");
    Serial.println(sent ? "SUCCESS" : "FAILED");
}

/* ── Send ────────────────────────────────────────────────────────────────── */
void Telegram_SendMessage(const String &msg) {
    //Telegram_Init();
    /* Safety checks before ANY dereference */
    if (pClient == NULL) {
        Serial.println("[Telegram] pClient is NULL — not initialised");
        return;
    }
    if (pBot == NULL) {
        Serial.println("[Telegram] pBot is NULL — not initialised");
        return;
    }
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
        sent = pBot->sendMessage(CHAT_ID, msg, "");
        Serial.println(sent);
        if (!sent && i < 2) {
            Serial.println("[Telegram] Failed — retrying in 3s");
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

        if (!sReady || pBot == NULL || WiFi.status() != WL_CONNECTED) continue;

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