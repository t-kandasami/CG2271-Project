#ifndef SESSION_TRACKER_H
#define SESSION_TRACKER_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * COLOUR THRESHOLDS — these same values are written into the Gemini prompt.
 * If you change a number here, update the table in api_handler.cpp too.
 *
 *  GREEN  = comfortable, no action needed
 *  YELLOW = borderline, at least one metric approaching its limit
 *  RED    = poor environment, immediate attention recommended
 *
 *  Rule: final classification = WORST tier across ALL metrics
 *
 *  Temperature (°C)   GREEN ≤26.0  |  YELLOW 26.0–28.5  |  RED >28.5
 *  Humidity (%)       GREEN ≤65.0  |  YELLOW 65.0–72.0  |  RED >72.0
 *  Light (% ADC)      GREEN 35–70  |  YELLOW 20–35/70–85 | RED <20/>85
 *  Sound events       GREEN 0–4    |  YELLOW 5–10        |  RED >10
 * ═══════════════════════════════════════════════════════════════════════════ */

#define SESSION_TEMP_GREEN_MAX    26.0f
#define SESSION_TEMP_YELLOW_MAX   28.5f
#define SESSION_HUM_GREEN_MAX     65.0f
#define SESSION_HUM_YELLOW_MAX    72.0f
#define SESSION_LIGHT_GREEN_MIN   35.0f
#define SESSION_LIGHT_GREEN_MAX   70.0f
#define SESSION_LIGHT_YELLOW_MIN  20.0f
#define SESSION_LIGHT_YELLOW_MAX  85.0f
#define SESSION_SOUND_GREEN_MAX   4U
#define SESSION_SOUND_YELLOW_MAX  10U

typedef struct {
    float    avg_temp;        /* °C                              */
    float    avg_humidity;    /* %                               */
    float    avg_light_pct;   /* 0–100 % (normalised from ADC)  */
    float    avg_sound_pct;   /* 0–100 % (normalised from ADC)  */
    uint32_t sound_triggers;  /* total triggered events          */
    uint32_t sample_count;
    uint32_t duration_s;
} SessionSummary_t;

void Session_Init(void);
void Session_Start(void);
void Session_AddSample(float temp, float humidity,
                       float light_raw, float sound_raw,
                       uint8_t sound_triggered);
bool Session_End(SessionSummary_t *out);   /* true if ≥1 valid sample */
bool Session_IsActive(void);

/* Pending-report handoff between uart_rx task and vGeminiTask */
void Session_StorePendingReport(const SessionSummary_t *s);
bool Session_ConsumePendingReport(SessionSummary_t *out);

#endif /* SESSION_TRACKER_H */