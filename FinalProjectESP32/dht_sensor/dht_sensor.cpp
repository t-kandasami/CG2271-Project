#include "dht_sensor.h"
#include "shared_data.h"

#include <DHT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ── Internal state ──────────────────────────────────────────────────────── */
static DHT   sDHT(DHT_PIN, DHT11);

/*
 * These mirror your lastTemp and lastHumidity from the working sketch.
 * Used to calculate change between readings — same logic you had in loop()
 */
static float sLastTemp     = 0.0f;
static float sLastHumidity = 0.0f;
static float sPrevTemp     = 0.0f;   // previous reading for change calculation
static float sPrevHumidity = 0.0f;   // previous reading for change calculation

/* ── Private helpers ─────────────────────────────────────────────────────── */

/*
 * Same bad reading check as your isnan() check in loop()
 */
static bool isValidReading(float temp, float humidity) {
    if (isnan(temp) || isnan(humidity)) return false;
    return true;
}

/*
 * Same status logic as your working sketch:
 *   temp > 29   → TOO HOT
 *   humidity > 75 → TOO HUMID
 *   else        → OK
 * Returns 0 = green (OK), 1 = amber (warn), 2 = red (alert)
 */
static uint8_t getLEDStatus(float temp, float humidity) {
    if (temp     > 29.0f) return 2;   // TOO HOT  → red
    if (humidity > 75.0f) return 2;   // TOO HUMID → red
    return 0;                          // OK        → green
}

/*
 * Prints exactly what your working sketch printed in loop()
 */
static void printReading(float temp, float humidity) {
    float tempChange = temp     - sPrevTemp;
    float humChange  = humidity - sPrevHumidity;

    Serial.println("─────────────────────────────");
    Serial.print("Temp: ");     Serial.print(temp);     Serial.println(" C");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");

    Serial.print("Temp change: ");
    Serial.print(tempChange > 0 ? "+" : "");
    Serial.print(tempChange);
    Serial.println(" C");

    Serial.print("Humidity change: ");
    Serial.print(humChange > 0 ? "+" : "");
    Serial.print(humChange);
    Serial.println(" %");

    Serial.print("Status: ");
    if      (temp     > 29.0f) Serial.println("TOO HOT");
    else if (humidity > 75.0f) Serial.println("TOO HUMID");
    else                       Serial.println("OK");
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  PUBLIC FUNCTIONS                                                         */
/* ═════════════════════════════════════════════════════════════════════════ */

void DHT_Init(void) {
    sDHT.begin();

    /*
     * Same as your setup() — wait 2 seconds to stabilise
     * then print Ready! before starting reads
     */
    Serial.println("Waiting for sensor to stabilise...");
    delay(2000);
    Serial.println("Ready!");
    Serial.println("─────────────────────────────");
}

float DHT_GetTemp(void)     { return sLastTemp; }
float DHT_GetHumidity(void) { return sLastHumidity; }

/* ═════════════════════════════════════════════════════════════════════════ */
/*  TASK — mirrors your loop() exactly                                       */
/* ═════════════════════════════════════════════════════════════════════════ */

void vDHTTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        /*
         * Same as your loop() — read humidity and temperature
         */
        float humidity    = sDHT.readHumidity();
        float temperature = sDHT.readTemperature();

        /*
         * Same as your isnan() check — skip bad readings
         */
        if (!isValidReading(temperature, humidity)) {
            Serial.println("Bad reading — retrying...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        /* Print exactly what your sketch printed */
        printReading(temperature, humidity);

        /*
         * Same as your lastTemp / lastHumidity update at end of loop()
         */
        sPrevTemp     = sLastTemp;
        sPrevHumidity = sLastHumidity;
        sLastTemp     = temperature;
        sLastHumidity = humidity;

        // Write to shared data under mutex
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            gSensorData.esp_temp     = temperature;
            gSensorData.esp_humidity = humidity;
            gSensorData.led_status   = getLEDStatus(temperature, humidity);
            xSemaphoreGive(gSensorMutex);
        }

        // Same as your delay(2000) — FreeRTOS friendly version
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}