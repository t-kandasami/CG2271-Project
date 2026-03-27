#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "api_handler.h"
#include "passwords.h"

ESP32_AI_Connect aiClient("gemini", GEMINI_KEY, GEMINI_MODEL);

/* ── Cooldown ────────────────────────────────────────────────────────────── */
#define GEMINI_COOLDOWN_MS   60000   // 60 seconds minimum between API calls
static unsigned long sLastGeminiCall = 0;

/* ── WiFi ────────────────────────────────────────────────────────────────── */
void connectWiFiGemini(void) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WiFi] Connecting");
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

/* ── Gemini ──────────────────────────────────────────────────────────────── */
String postGemini(const String &prompt) {
    unsigned long now = millis();

    /*
     * Cooldown check — enforce minimum 60s between API calls
     * Prevents 429 rate limit errors on Gemini free tier
     * Skip the call entirely if called too soon and return empty string
     */
    if (sLastGeminiCall != 0 && (now - sLastGeminiCall) < GEMINI_COOLDOWN_MS) {
        unsigned long remaining = (GEMINI_COOLDOWN_MS - (now - sLastGeminiCall)) / 1000;
        Serial.print("[Gemini] Cooldown active — ");
        Serial.print(remaining);
        Serial.println("s remaining before next call");
        return "";
    }

    Serial.println("[Gemini] Sending request...");
    sLastGeminiCall = now;

    String response = aiClient.chat(prompt);

    if (response.isEmpty()) {
        String error = aiClient.getLastError();
        Serial.print("[Gemini] Error: ");
        Serial.println(error);

        if (error.indexOf("403") >= 0) {
            Serial.println("[Gemini] 403 = API key rejected — check aistudio.google.com/apikey");
        } else if (error.indexOf("429") >= 0) {
            Serial.println("[Gemini] 429 = Rate limit hit — cooldown will prevent this next time");
        } else if (error.indexOf("401") >= 0) {
            Serial.println("[Gemini] 401 = Unauthorised — wrong key format");
        }
    } else {
        Serial.println("[Gemini] Response received successfully");
        Serial.print("[Gemini] "); Serial.println(response);
    }
    
    return response;
}