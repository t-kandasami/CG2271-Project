#include "session_tracker.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <Arduino.h>

/* ── Accumulators ────────────────────────────────────────────────────────── */
static bool     sActive        = false;
static uint32_t sStartMs       = 0;
static float    sSumTemp       = 0.0f;
static float    sSumHumidity   = 0.0f;
static float    sSumLight      = 0.0f;
static float    sSumSound      = 0.0f;
static uint32_t sSoundTriggers = 0;
static uint32_t sSampleCount   = 0;

/* ── Pending report ──────────────────────────────────────────────────────── */
static SemaphoreHandle_t sReportMutex   = NULL;
static volatile bool     sPendingReport = false;
static SessionSummary_t  sPendingSummary;

/* ═════════════════════════════════════════════════════════════════════════ */

void Session_Init(void) {
    sActive        = false;
    sPendingReport = false;
    sReportMutex   = xSemaphoreCreateMutex();
    Serial.println("[Session] Tracker initialised");
}

void Session_Start(void) {
    sSumTemp       = 0.0f;
    sSumHumidity   = 0.0f;
    sSumLight      = 0.0f;
    sSumSound      = 0.0f;
    sSoundTriggers = 0;
    sSampleCount   = 0;
    sStartMs       = millis();
    sActive        = true;
    Serial.println("[Session] Study session started");
}

void Session_AddSample(float temp, float humidity,
                       float light_raw, float sound_raw,
                       uint8_t sound_triggered) {
    if (!sActive) return;

    const float ADC_MAX = 4095.0f;

    float light_pct = (light_raw / ADC_MAX) * 100.0f;
    float sound_pct = (sound_raw / ADC_MAX) * 100.0f;

    /* Clamp */
    if (light_pct > 100.0f) light_pct = 100.0f;
    if (sound_pct > 100.0f) sound_pct = 100.0f;
    if (light_pct < 0.0f)   light_pct = 0.0f;
    if (sound_pct < 0.0f)   sound_pct = 0.0f;

    sSumTemp       += temp;
    sSumHumidity   += humidity;
    sSumLight      += light_pct;
    sSumSound      += sound_pct;
    sSoundTriggers += (sound_triggered ? 1U : 0U);
    sSampleCount++;
}

bool Session_End(SessionSummary_t *out) {
    if (!sActive) return false;
    sActive = false;

    if (sSampleCount == 0) {
        Serial.println("[Session] Ended with 0 samples — discarding");
        return false;
    }

    out->avg_temp       = sSumTemp     / (float)sSampleCount;
    out->avg_humidity   = sSumHumidity / (float)sSampleCount;
    out->avg_light_pct  = sSumLight    / (float)sSampleCount;
    out->avg_sound_pct  = sSumSound    / (float)sSampleCount;
    out->sound_triggers = sSoundTriggers;
    out->sample_count   = sSampleCount;
    out->duration_s     = (millis() - sStartMs) / 1000UL;

    Serial.printf("[Session] Ended: %lu samples, %lu min %lu s\n",
                  (unsigned long)sSampleCount,
                  (unsigned long)(out->duration_s / 60),
                  (unsigned long)(out->duration_s % 60));
    return true;
}

bool Session_IsActive(void) {
    return sActive;
}

void Session_StorePendingReport(const SessionSummary_t *s) {
    if (sReportMutex == NULL) {
        Serial.println("[Session] ERROR: mutex not initialised");
        return;
    }
    if (xSemaphoreTake(sReportMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        sPendingSummary = *s;
        sPendingReport  = true;
        xSemaphoreGive(sReportMutex);
        Serial.println("[Session] Report queued for Gemini");
    }
}

bool Session_ConsumePendingReport(SessionSummary_t *out) {
    if (sReportMutex == NULL) return false;

    bool found = false;
    if (xSemaphoreTake(sReportMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (sPendingReport) {
            *out           = sPendingSummary;
            sPendingReport = false;
            found          = true;
        }
        xSemaphoreGive(sReportMutex);
    }
    return found;
}