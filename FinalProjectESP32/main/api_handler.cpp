#include <WiFi.h>
#include <ESP32_AI_Connect.h>
#include "api_handler.h"
#include "passwords.h"
#include "session_tracker.h"
#include "telegram_tx.h"

ESP32_AI_Connect aiClient("gemini", GEMINI_KEY, GEMINI_MODEL);

/* ── Cooldown ────────────────────────────────────────────────────────────── */
#define GEMINI_COOLDOWN_MS 60000  // 60 seconds minimum between API calls
static unsigned long sLastGeminiCall = 0;

/* ── WiFi with Permanent Connection ─────────────────────────────────────── */
void connectWiFiGemini(void) {
  // Force WiFi to stay connected and never sleep
  WiFi.setSleep(false);         // Disable power saving
  WiFi.setAutoReconnect(true);  // Auto reconnect if disconnected
  WiFi.persistent(true);        // Save WiFi config to flash

  // Check if already connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Already connected and forced to stay awake");
    aiClient.setChatMaxTokens(400);
    aiClient.setChatTemperature(0.2);
    return;
  }
  delay(1000);

  // Configure static IP for more stability
  IPAddress staticIP(192, 168, 153, 200);
  IPAddress gateway(192, 168, 153, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(8, 8, 8, 8);
  IPAddress dns2(8, 8, 4, 4);

  if (WiFi.config(staticIP, gateway, subnet, dns, dns2)) {
    Serial.println("[WiFi] Using static IP: 192.168.153.200");
  } else {
    Serial.println("[WiFi] Using DHCP");
  }

  // Start connection
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
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // Keep WiFi radio always on
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Max power for better stability

    // Set TCP keep-alive to prevent disconnections
    WiFi.setAutoReconnect(true);

    aiClient.setChatMaxTokens(400);
    aiClient.setChatTemperature(0.2);
  } else {
    Serial.println("\n[WiFi] Connection failed!");
  }
}

/* ── Keep WiFi Alive Forever Task ───────────────────────────────────────── */
void vWiFiKeepAliveTask(void *pvParameters) {
  (void)pvParameters;

  while (1) {
    // Check if WiFi is still connected
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Connection lost! Reconnecting...");
      connectWiFiGemini();
    } else {
      // Periodically ping to keep connection alive
      // This prevents routers from dropping idle connections
      static unsigned long lastPing = 0;
      if (millis() - lastPing > 30000) {
        IPAddress resolvedIP;  // named variable — required by hostByName()
        WiFi.hostByName("google.com", resolvedIP);
        lastPing = millis();
        Serial.println("[WiFi] Keep-alive ping sent");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 seconds
  }
}

/* ── Gemini with Retry and WiFi Recovery ────────────────────────────────── */
String postGemini(const String &prompt) {
  unsigned long now = millis();

  /* Cooldown check */
  if (sLastGeminiCall != 0 && (now - sLastGeminiCall) < GEMINI_COOLDOWN_MS) {
    unsigned long remaining = (GEMINI_COOLDOWN_MS - (now - sLastGeminiCall)) / 1000;
    Serial.print("[Gemini] Cooldown active — ");
    Serial.print(remaining);
    Serial.println("s remaining before next call");
    return "";
  }

  // Ensure WiFi is connected before attempting
  int wifiRetries = 3;
  while (WiFi.status() != WL_CONNECTED && wifiRetries > 0) {
    Serial.println("[Gemini] WiFi not connected, reconnecting...");
    connectWiFiGemini();
    delay(2000);
    wifiRetries--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Gemini] WiFi connection failed after retries");
    return "";
  }

  Serial.println("[Gemini] Sending request...");
  sLastGeminiCall = now;

  String response = aiClient.chat(prompt);

  if (response.isEmpty()) {
    String error = aiClient.getLastError();
    Serial.print("[Gemini] Error: ");
    Serial.println(error);

    if (error.indexOf("HTTP") >= 0 || error.indexOf("send header") >= 0) {
      Serial.println("[Gemini] Connection error - forcing WiFi reconnect");
      WiFi.disconnect(true);
      delay(1000);
      connectWiFiGemini();
    }
  } else {
    Serial.println("[Gemini] Response received successfully");
    Serial.print("[Gemini] ");
    Serial.println(response);
  }

  return response;
}

String postGeminiWithRetry(const String &prompt, int maxRetries = 3) {
  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.printf("[Gemini] Attempt %d/%d\n", attempt, maxRetries);

    // Ensure WiFi is connected before each attempt
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Gemini] WiFi disconnected, reconnecting...");
      connectWiFiGemini();
      delay(2000);
    }

    String response = postGemini(prompt);

    if (!response.isEmpty()) {
      return response;
    }

    if (attempt < maxRetries) {
      Serial.printf("[Gemini] Attempt %d failed, retrying in 5 seconds...\n", attempt);
      delay(5000);
    }
  }

  Serial.println("[Gemini] All retry attempts failed");
  return "";
}

/* ════════════════════════════════════════════════════════════════════════
 * SESSION REPORT
 * ════════════════════════════════════════════════════════════════════════ */
void postGeminiSessionReport(const SessionSummary_t &s) {
  // Validate data before sending
  if (s.avg_light_pct > 100.0f || s.avg_sound_pct > 100.0f) {
    Serial.println("[Gemini] WARNING: Invalid sensor data detected!");
    Telegram_SendMessage("⚠️ Session report error: Invalid sensor readings");
    return;
  }

  String prompt =
    "Analyse the study session below and classify the environment.\n\n"

    "## Session Data\n"
    "- Duration            : "
    + String(s.duration_s / 60) + " min "
    + String(s.duration_s % 60) + " s\n"
                                  "- Avg Temperature     : "
    + String(s.avg_temp, 1) + " C\n"
                              "- Avg Humidity        : "
    + String(s.avg_humidity, 1) + " %\n"
                                  "- Avg Light Level     : "
    + String(s.avg_light_pct, 1)
    + " %  (0=dark, 100=max brightness)\n"
      "- Avg Sound Level     : "
    + String(s.avg_sound_pct, 1)
    + " %  (0=silent, 100=max)\n"
      "- Sound disturbances  : "
    + String(s.sound_triggers) + " events\n"
                                 "- Samples collected   : "
    + String(s.sample_count) + "\n\n"

                               "## Colour Definitions\n"
                               "GREEN  = comfortable environment, no action needed\n"
                               "YELLOW = borderline, at least one metric approaching its limit\n"
                               "RED    = poor environment, immediate improvement recommended\n\n"

                               "## Classification Rules\n"
                               "Evaluate every metric independently. "
                               "Final classification = WORST tier across ALL metrics.\n\n"

                               "| Metric             | GREEN              | YELLOW                      | RED                  |\n"
                               "|--------------------|--------------------|-----------------------------|----------------------|\n"
                               "| Temperature        | <= 26.0 C          | 26.0 to 28.5 C              | > 28.5 C             |\n"
                               "| Humidity           | <= 65 %            | 65 to 72 %                  | > 72 %               |\n"
                               "| Light level        | 35 to 70 %         | 20-35 % or 70-85 %          | < 20 % or > 85 %     |\n"
                               "| Sound disturbances | 0 to 4 events      | 5 to 10 events              | > 10 events          |\n\n"

                               "## Required Output Format\n"
                               "Use EXACTLY this format — no extra lines, no markdown:\n\n"
                               "CLASSIFICATION: [GREEN or YELLOW or RED]\n"
                               "REASON: [one sentence naming the metric that decided the tier]\n"
                               "SUGGESTIONS:\n"
                               "- [actionable improvement for the worst metric]\n"
                               "- [second improvement]\n"
                               "- [third improvement]\n";

  Serial.println("[Gemini] Sending session report...");
  String response = postGeminiWithRetry(prompt, 3);

  if (response.isEmpty()) {
    Serial.println("[Gemini] No response after retries — session report skipped");
    Telegram_SendMessage("⚠️ Session report failed: Gemini API unreachable");
    return;
  }

  String teleMsg = "Session Report\n\n" + response;
  teleMsg.replace("*", "");
  teleMsg.replace("_", "");
  teleMsg.replace("`", "");

  Telegram_SendMessage(teleMsg);
}

/* ════════════════════════════════════════════════════════════════════════
 * GEMINI TASK
 * ════════════════════════════════════════════════════════════════════════ */
void vGeminiTask(void *pvParameters) {
  (void)pvParameters;
  vTaskDelay(pdMS_TO_TICKS(5000));

  while (1) {
    Serial.println("[GeminiTask] Checking for pending report...");
    SessionSummary_t summary;
    if (Session_ConsumePendingReport(&summary)) {
      Serial.println("[GeminiTask] Found pending report, sending to Gemini...");
      postGeminiSessionReport(summary);
      Serial.println("[GeminiTask] Report processing complete");
    } else {
      Serial.println("[GeminiTask] No pending report");
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

bool WiFi_EnsureConnected(void) {
    if (WiFi.status() == WL_CONNECTED) return true;
    Serial.println("[WiFi] EnsureConnected: reconnecting...");
    connectWiFiGemini();
    return (WiFi.status() == WL_CONNECTED);
}