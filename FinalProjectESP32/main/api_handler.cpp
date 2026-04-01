#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "api_handler.h"
#include "passwords.h"
#include "session_tracker.h"
#include "telegram_tx.h"

ESP32_AI_Connect aiClient("gemini", GEMINI_KEY, GEMINI_MODEL);

/* ── Cooldown ────────────────────────────────────────────────────────────── */
#define GEMINI_COOLDOWN_MS      60000
static unsigned long sLastGeminiCall = 0;

/* ═════════════════════════════════════════════════════════════════════════ */
/*  WIFI                                                                     */
/* ═════════════════════════════════════════════════════════════════════════ */

void connectWiFiGemini(void) {
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Already connected");
        aiClient.setChatMaxTokens(400);
        aiClient.setChatTemperature(0.2);
        aiClient.setChatSystemRole("You are a study environment AI assistant.");
        return;
    }

    delay(1000);

    IPAddress staticIP(192, 168, 153, 200);
    IPAddress gateway(192, 168, 153, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(8, 8, 8, 8);
    IPAddress dns2(8, 8, 4, 4);

    if (!WiFi.config(staticIP, gateway, subnet, dns, dns2)) {
        Serial.println("[WiFi] Using DHCP");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WiFi] Connecting");

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP: ");   Serial.println(WiFi.localIP());
        Serial.print("[WiFi] RSSI: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");

        WiFi.setTxPower(WIFI_POWER_19_5dBm);

        aiClient.setChatMaxTokens(400);
        aiClient.setChatTemperature(0.2);
        aiClient.setChatSystemRole("You are a study environment AI assistant.");
    } else {
        Serial.println("\n[WiFi] Connection FAILED");
    }
}

bool WiFi_EnsureConnected(void) {
    if (WiFi.status() == WL_CONNECTED) return true;

    // Simple reconnect attempt
    Serial.println("[WiFi] Reconnecting...");
    WiFi.reconnect();

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }

    return (WiFi.status() == WL_CONNECTED);
}

/* ── Keep-alive task ─────────────────────────────────────────────────────── */
void vWiFiKeepAliveTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Connection lost — reconnecting...");
            connectWiFiGemini();
        } else {
            static unsigned long lastPing = 0;
            if (millis() - lastPing > 30000) {
                IPAddress resolvedIP;
                WiFi.hostByName("google.com", resolvedIP);
                lastPing = millis();
                Serial.println("[WiFi] Keep-alive ping sent");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  GEMINI                                                                   */
/* ═════════════════════════════════════════════════════════════════════════ */

/*
 * postGemini()
 * Normal call with 60s cooldown — for periodic ambient advice
 */
String postGemini(const String &prompt) {
    unsigned long now = millis();

    if (sLastGeminiCall != 0 && (now - sLastGeminiCall) < GEMINI_COOLDOWN_MS) {
        unsigned long remaining = (GEMINI_COOLDOWN_MS - (now - sLastGeminiCall)) / 1000;
        Serial.print("[Gemini] Cooldown — ");
        Serial.print(remaining);
        Serial.println("s remaining");
        return "";
    }

    if (!WiFi_EnsureConnected()) {
        Serial.println("[Gemini] No WiFi — skipping");
        return "";
    }

    Serial.println("[Gemini] Sending request...");
    sLastGeminiCall = now;

    String response = aiClient.chat(prompt);

    if (response.isEmpty()) {
        String error = aiClient.getLastError();
        Serial.print("[Gemini] Error: "); Serial.println(error);
        if (error.indexOf("403") >= 0) Serial.println("[Gemini] 403 = key rejected");
        if (error.indexOf("429") >= 0) Serial.println("[Gemini] 429 = rate limited");
    } else {
        Serial.println("[Gemini] Response OK");
        Serial.println(response);
    }

    return response;
}

/*
 * postGeminiDirect()
 * Bypasses cooldown — used for important one-off session reports
 * Updates the cooldown timer after sending so normal calls are still throttled
 */
static String postGeminiDirect(const String &prompt) {
    if (!WiFi_EnsureConnected()) {
        Serial.println("[Gemini] No WiFi — direct call skipped");
        return "";
    }

    Serial.println("[Gemini] Sending direct request...");
    sLastGeminiCall = millis();   // update cooldown timer

    String response = aiClient.chat(prompt);

    if (response.isEmpty()) {
        String error = aiClient.getLastError();
        Serial.print("[Gemini] Direct error: "); Serial.println(error);
    } else {
        Serial.println("[Gemini] Direct response OK:");
        Serial.println(response);
    }

    return response;
}

/*
 * postGeminiWithRetry()
 * Retries postGemini() up to maxRetries times with 5s between attempts
 * Uses normal cooldown — for periodic calls
 */
String postGeminiWithRetry(const String &prompt, int maxRetries) {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        Serial.printf("[Gemini] Attempt %d/%d\n", attempt, maxRetries);

        String response = postGemini(prompt);
        if (!response.isEmpty()) return response;

        if (attempt < maxRetries) {
            Serial.printf("[Gemini] Attempt %d failed — retrying in 5s\n", attempt);
            delay(5000);
        }
    }
    Serial.println("[Gemini] All attempts failed");
    return "";
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  SESSION REPORT                                                           */
/* ═════════════════════════════════════════════════════════════════════════ */

void postGeminiSessionReport(const SessionSummary_t &s) {
    /* Sanity check */
    if (s.avg_light_pct > 100.0f || s.avg_sound_pct > 100.0f) {
        Serial.println("[Gemini] Invalid sensor data — aborting report");
        Telegram_SendMessage("Warning: Session report skipped — invalid sensor data");
        return;
    }

    String prompt =
        "Analyse the study session below and classify the environment.\n\n"
        "## Session Data\n"
        "- Duration            : " + String(s.duration_s / 60) + " min " + String(s.duration_s % 60) + " s\n"
        "- Avg Temperature     : " + String(s.avg_temp, 1)         + " C\n"
        "- Avg Humidity        : " + String(s.avg_humidity, 1)     + " %\n"
        "- Avg Light Level     : " + String(s.avg_light_pct, 1)    + " % (0=dark, 100=max)\n"
        "- Avg Sound Level     : " + String(s.avg_sound_pct, 1)    + " % (0=silent, 100=max)\n"
        "- Sound disturbances  : " + String(s.sound_triggers)      + " events\n"
        "- Samples collected   : " + String(s.sample_count)        + "\n\n"

        "## Colour Definitions\n"
        "GREEN  = comfortable, no action needed\n"
        "YELLOW = borderline, at least one metric approaching limit\n"
        "RED    = poor, immediate improvement recommended\n\n"

        "## Classification Rules\n"
        "Final classification = WORST tier across ALL metrics.\n\n"
        "| Metric             | GREEN         | YELLOW              | RED                |\n"
        "|--------------------|---------------|---------------------|--------------------|\n"
        "| Temperature        | <= 26.0 C     | 26.0 to 28.5 C      | > 28.5 C           |\n"
        "| Humidity           | <= 65 %       | 65 to 72 %          | > 72 %             |\n"
        "| Light level        | 35 to 70 %    | 20-35 or 70-85 %    | < 20 or > 85 %     |\n"
        "| Sound disturbances | 0 to 4 events | 5 to 10 events      | > 10 events        |\n\n"

        "## Required Output Format\n"
        "Use EXACTLY this format — no extra lines, no markdown:\n\n"
        "CLASSIFICATION: [GREEN or YELLOW or RED]\n"
        "REASON: [one sentence naming the deciding metric]\n"
        "SUGGESTIONS:\n"
        "- [improvement for worst metric]\n"
        "- [second improvement]\n"
        "- [third improvement]\n";

    Serial.println("[Gemini] Sending session report...");

    /* Use direct call — bypasses cooldown for important session report */
    String response = postGeminiDirect(prompt);

    /* Retry once if first attempt fails */
    if (response.isEmpty()) {
        Serial.println("[Gemini] First attempt failed — retrying in 10s");
        delay(10000);
        response = postGeminiDirect(prompt);
    }

    if (response.isEmpty()) {
        Serial.println("[Gemini] Session report failed after retry");
        Telegram_SendMessage("Session report failed — Gemini API unreachable");
        return;
    }

    /* Strip markdown characters Telegram plain text doesn't need */
    response.replace("*", "");
    response.replace("_", "");
    response.replace("`", "");
    response.replace("#", "");

    /* Build final message */
    String teleMsg = "Study Session Report\n";
    teleMsg += "Duration: " + String(s.duration_s / 60) + " min\n\n";
    teleMsg += response;

    /* Debug — log exactly what gets sent */
    Serial.println("[Gemini] Message to Telegram:");
    Serial.println(teleMsg);
    Serial.print("[Gemini] Message length: ");
    Serial.println(teleMsg.length());

    Telegram_SendMessage(teleMsg);
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  GEMINI TASK                                                              */
/* ═════════════════════════════════════════════════════════════════════════ */

void vGeminiTask(void *pvParameters) {
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(5000));   // wait for system to stabilise

    while (1) {
        Serial.println("[GeminiTask] Checking for pending report...");

        SessionSummary_t summary;
        if (Session_ConsumePendingReport(&summary)) {
            Serial.println("[GeminiTask] Pending report found — sending to Gemini");
            postGeminiSessionReport(summary);
            Serial.println("[GeminiTask] Report complete");
        } else {
            Serial.println("[GeminiTask] No pending report");
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}