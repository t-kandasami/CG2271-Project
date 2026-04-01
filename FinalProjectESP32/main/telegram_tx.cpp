#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "telegram_tx.h"
#include "shared_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart_tx.h"
#include "telegram_tx.h"
#include "passwords.h"

static WiFiClientSecure client;
static UniversalTelegramBot bot(BOT_TOKEN, client);

static void connectWiFiTele() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.println("connecting");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        client.setInsecure();
        Serial.println("connected");
        bot.sendMessage(CHAT_ID, "ESP32 online!", "");
    }
}


void Telegram_Init() {
    connectWiFiTele();
    // connectWiFiTele() already sends "ESP32 online!" on success — no duplicate needed
}

void vTelegramTask(void *pvParameters) {
    (void)pvParameters;

    char lastSentReply[512] = "";   // track last sent reply to avoid duplicates

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // check every 30s
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            float t = gSensorData.esp_temp;
            float h = gSensorData.esp_humidity;
            char reply[512];
            strncpy(reply, gSensorData.gemini_reply, sizeof(reply) - 1);
            reply[sizeof(reply) - 1] = '\0';
            xSemaphoreGive(gSensorMutex);

            String msg = "Temp: " + String(t, 1) + " C\n";
            msg += "Humidity: " + String(h, 1) + " %";
            bot.sendMessage(CHAT_ID, msg, "");

            // Send Gemini reply only if there is one and it's new
            if (reply[0] != '\0' && strcmp(reply, lastSentReply) != 0) {
                bot.sendMessage(CHAT_ID, String(reply), "");
                strncpy(lastSentReply, reply, sizeof(lastSentReply) - 1);
                lastSentReply[sizeof(lastSentReply) - 1] = '\0';
            }
        }
    }
}