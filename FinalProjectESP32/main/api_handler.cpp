#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "api_handler.h"
#include "passwords.h"
#include "session_tracker.h"
#include "shared_data.h"
#include "telegram_tx.h"

ESP32_AI_Connect aiClient("gemini", GEMINI_KEY, GEMINI_MODEL);

#define GEMINI_COOLDOWN_MS  60000
static unsigned long sLastGeminiCall = 0;

/* ═════════════════════════════════════════════════════════════════════════ */
/*  WIFI                                                                     */
/* ═════════════════════════════════════════════════════════════════════════ */

void connectWiFiGemini(void) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Already connected");
        aiClient.setChatMaxTokens(1500);
        aiClient.setChatTemperature(0.2);
        aiClient.setChatSystemRole("You are a study environment AI assistant.");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
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
        aiClient.setChatMaxTokens(1500);
        aiClient.setChatTemperature(0.2);
        aiClient.setChatSystemRole("You are a study environment AI assistant.");
    } else {
        Serial.println("\n[WiFi] Connection FAILED");
    }
}

bool WiFi_EnsureConnected(void) {
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.println("[WiFi] Reconnecting...");
    WiFi.reconnect();
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
    }
    return (WiFi.status() == WL_CONNECTED);
}

void vWiFiKeepAliveTask(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Lost — reconnecting...");
            /*
             * Event Group — clear the WiFi bit while reconnecting.
             * Any task blocked on xEventGroupWaitBits(WIFI_CONNECTED_BIT)
             * will remain blocked until we set the bit again below.
             */
            if (gSystemEvents != NULL) {
                xEventGroupClearBits(gSystemEvents, WIFI_CONNECTED_BIT);
            }
            connectWiFiGemini();
            if (WiFi.status() == WL_CONNECTED && gSystemEvents != NULL) {
                xEventGroupSetBits(gSystemEvents, WIFI_CONNECTED_BIT);
                Serial.println("[WiFi] WIFI_CONNECTED_BIT set");
            }
        } else {
            /* WiFi is up — nothing to do, setAutoReconnect handles drops */
            Serial.println("[WiFi] Keep-alive check: connected");
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  GEMINI                                                                   */
/* ═════════════════════════════════════════════════════════════════════════ */

String postGemini(const String &prompt) {
    unsigned long now = millis();

    if (sLastGeminiCall != 0 && (now - sLastGeminiCall) < GEMINI_COOLDOWN_MS) {
        unsigned long remaining = (GEMINI_COOLDOWN_MS - (now - sLastGeminiCall)) / 1000;
        Serial.print("[Gemini] Cooldown — "); Serial.print(remaining); Serial.println("s left");
        return "";
    }

    if (!WiFi_EnsureConnected()) {
        Serial.println("[Gemini] No WiFi");
        return "";
    }

    Serial.println("[Gemini] Sending request...");
    sLastGeminiCall = now;

    String response = aiClient.chat(prompt);

    if (response.isEmpty()) {
        String error = aiClient.getLastError();
        Serial.print("[Gemini] Error: "); Serial.println(error);
        if (error.indexOf("400") >= 0) Serial.println("[Gemini] 400 = bad request");
        if (error.indexOf("429") >= 0) Serial.println("[Gemini] 429 = rate limited");
        if (error.indexOf("529") >= 0) Serial.println("[Gemini] 529 = overloaded");
    } else {
        Serial.println("[Gemini] Response OK:");
        Serial.println(response);
    }

    return response;
}

static String postGeminiDirect(const String &prompt) {
    /*
     * Event Group wait — block until WIFI_CONNECTED_BIT is set.
     * Replaces the manual polling loop in WiFi_EnsureConnected().
     * pdFALSE: do not clear the bit (other tasks also need it).
     * pdTRUE:  wait for ALL listed bits (only one bit here).
     * Timeout: 30s — give the keep-alive task time to reconnect.
     */
    if (gSystemEvents != NULL) {
        EventBits_t bits = xEventGroupWaitBits(
            gSystemEvents,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(30000));
        if (!(bits & WIFI_CONNECTED_BIT)) {
            Serial.println("[Gemini] WiFi wait timeout — direct skipped");
            return "";
        }
    } else if (!WiFi_EnsureConnected()) {
        Serial.println("[Gemini] No WiFi — direct skipped");
        return "";
    }

    Serial.println("[Gemini] Direct request...");
    sLastGeminiCall = millis();

    String response = aiClient.chat(prompt);

    if (response.isEmpty()) {
        String error = aiClient.getLastError();
        Serial.print("[Gemini] Direct error: "); Serial.println(error);
        if (error.indexOf("429") >= 0 || error.indexOf("529") >= 0) {
            Serial.println("[Gemini] Rate limited — waiting 30s");
            vTaskDelay(pdMS_TO_TICKS(30000));
        }
    } else {
        Serial.println("[Gemini] Direct response OK:");
        Serial.println(response);
    }

    return response;
}

String postGeminiWithRetry(const String &prompt, int maxRetries) {
    for (int i = 1; i <= maxRetries; i++) {
        Serial.printf("[Gemini] Attempt %d/%d\n", i, maxRetries);
        String r = postGemini(prompt);
        if (!r.isEmpty()) return r;
        if (i < maxRetries) { Serial.println("[Gemini] Retrying in 5s"); delay(5000); }
    }
    Serial.println("[Gemini] All attempts failed");
    return "";
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  SESSION REPORT                                                           */
/* ═════════════════════════════════════════════════════════════════════════ */

void postGeminiSessionReport(const SessionSummary_t &s) {
    if (s.avg_light_pct > 100.0f || s.avg_sound_pct > 100.0f) {
        Serial.println("[Gemini] Invalid sensor data");
        Telegram_SendMessage("Warning: Session report skipped — invalid data");
        return;
    }

    /* Simplified prompt — no markdown tables, shorter, less likely to 400 */
    String prompt =
        "You are a study environment advisor. "
        "Analyse this study session and respond in EXACTLY this format with no extra text:\n"
        "CLASSIFICATION: [GREEN or YELLOW or RED]\n"
        "REASON: [one sentence]\n"
        "SUGGESTIONS:\n"
        "- [tip 1]\n"
        "- [tip 2]\n"
        "- [tip 3]\n\n"
        "Session data:\n"
        "Duration: "     + String(s.duration_s / 60) + " minutes\n"
        "Temperature: "  + String(s.avg_temp, 1)      + " C\n"
        "Humidity: "     + String(s.avg_humidity, 1)  + " percent\n"
        "Light level: "  + String(s.avg_light_pct, 1) + " percent\n"
        "Sound level: "  + String(s.avg_sound_pct, 1) + " percent\n"
        "Noise events: " + String(s.sound_triggers)   + "\n\n"
        "Thresholds:\n"
        "GREEN:  temp<=26, humidity<=65, light 35-70%, noise<=4\n"
        "YELLOW: temp 26-28.5, humidity 65-72, light 20-35 or 70-85%, noise 5-10\n"
        "RED:    temp>28.5, humidity>72, light<20 or >85%, noise>10\n"
        "Use the worst metric as the final classification.";

    Serial.println("[Gemini] Sending session report...");
    String response = postGeminiDirect(prompt);

    if (response.isEmpty()) {
        Serial.println("[Gemini] Retrying in 30s...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        response = postGeminiDirect(prompt);
    }

    if (response.isEmpty()) {
        Serial.println("[Gemini] Report failed");
        Telegram_SendMessage("Session report failed — Gemini unavailable");
        return;
    }

    /* Strip markdown */
    response.replace("*", "");
    response.replace("_", "");
    response.replace("`", "");
    response.replace("#", "");

    String msg  = "Study Session Report\n";
    msg += "Duration: " + String(s.duration_s / 60) + " min\n\n";
    msg += response;

    Serial.println("[Gemini] Sending to Telegram:");
    Serial.println(msg);

    Telegram_SendMessage(msg);
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  GEMINI TASK                                                              */
/* ═════════════════════════════════════════════════════════════════════════ */

void vGeminiTask(void *pvParameters) {
    (void)pvParameters;
    vTaskDelay(pdMS_TO_TICKS(8000));   // wait for Telegram to fully init

    while (1) {
        SessionSummary_t summary;

        /*
         * Queue Receive — block indefinitely until a SessionSummary_t
         * arrives from vUartRxTask (posted on focus mode 1→0 edge).
         *
         * portMAX_DELAY: this task sleeps with zero CPU usage until
         * xQueueSend wakes it. This replaces the old 10-second polling
         * loop which woke up every 10s just to find nothing to do.
         *
         * The queue depth of 3 ensures that if two sessions end while
         * Gemini is still processing, the reports are queued and not lost.
         */
        Serial.println("[GeminiTask] Waiting for session report on queue...");
        if (xQueueReceive(gSessionReportQueue, &summary, portMAX_DELAY) == pdTRUE) {
            Serial.println("[GeminiTask] Report dequeued — processing");
            postGeminiSessionReport(summary);
            Serial.println("[GeminiTask] Done — waiting for next report");
        }
    }
}