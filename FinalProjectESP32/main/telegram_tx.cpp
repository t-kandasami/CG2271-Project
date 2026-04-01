#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "api_handler.h"
#include "shared_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_tx.h"
#include "passwords.h"

static WiFiClientSecure client;
static UniversalTelegramBot bot(BOT_TOKEN, client);

/*
static bool connectWiFiTele() {
    // If already connected, verify it's working
    if (WiFi.status() == WL_CONNECTED) {
        if (WiFi.localIP() != INADDR_NONE) {
            return true;
        }
    }
    
    Serial.println("[Telegram] WiFi not connected, connecting...");
    WiFi.mode(WIFI_STA);
    
    // Disable power saving
    WiFi.setSleep(false);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[Telegram] WiFi connected");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        client.setInsecure();
        return true;
    } else {
        Serial.println("\n[Telegram] WiFi connection failed");
        return false;
    }
}
*/

void Telegram_Init() {
    //connectWiFiTele();
    client.setInsecure();
    Serial.println("[Telegram] Initialised (using shared WiFi)");
}

void Telegram_SendMessage(const String &msg) {
    if (!WiFi_EnsureConnected()) {
        Serial.println("[Telegram] Cannot send - no WiFi connection");
        return;
    }
    Serial.println("[Telegram] Sending message:");
    Serial.println(msg);
    int retries = 3;
    bool sent = false;
    while (retries > 0 && !sent) {
        Serial.printf("[Telegram] Attempt %d/3...\n", 4 - retries);
        if (bot.sendMessage(CHAT_ID, msg, "")) {
            Serial.println("[Telegram] Message sent successfully");
            sent = true;
        } else {
            Serial.println("[Telegram] Send failed, retrying...");
            retries--;
            if (retries > 0) vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    if (!sent) {
        Serial.println("[Telegram] Failed to send message after all retries");
    }
}
