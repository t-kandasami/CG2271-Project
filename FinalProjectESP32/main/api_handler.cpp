#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "api_handler.h"

ESP32_AI_Connect aiClient("gemini", GEMINI_KEY, GEMINI_MODEL);

void connectWiFi(void) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }
    Serial.print("\nConnected! IP: ");
    Serial.println(WiFi.localIP());

    aiClient.setChatMaxTokens(300);
    aiClient.setChatTemperature(0.7);
    aiClient.setChatSystemRole("You are a study environment AI assistant.");
}

String postGemini(const String &prompt) {
    String response = aiClient.chat(prompt);
    if (response.isEmpty()) {
        Serial.print("[Gemini] Error: ");
        Serial.println(aiClient.getLastError());
    }
    return response;
}