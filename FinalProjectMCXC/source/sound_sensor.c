#include <stdio.h>
#include <stdbool.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sound_sensor.h"
#include "shared_data.h"

/* -----------------------------------------------------------------------
 * Internal: ADC Self-Calibration
 * ----------------------------------------------------------------------- */
static void adc_calibrate(void){
	NVIC_DisableIRQ(ADC0_IRQn);
	uint16_t cal_var;

    ADC0->CFG1 &= ~ADC_CFG1_ADIV_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADIV(3);
    ADC0->CFG1 |=  ADC_CFG1_ADLSMP_MASK;
    ADC0->CFG1 &= ~ADC_CFG1_MODE_MASK;
    ADC0->CFG1 |=  ADC_CFG1_MODE(0b11);
    ADC0->CFG1 &= ~ADC_CFG1_ADICLK_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADICLK(0b00);

    ADC0->SC2 &= ~ADC_SC2_ADTRG_MASK;
    ADC0->SC2 &= ~ADC_SC2_REFSEL_MASK;
    ADC0->SC2 |=  ADC_SC2_REFSEL(0b01);

    ADC0->SC3 |=  ADC_SC3_AVGE_MASK;
    ADC0->SC3 &= ~ADC_SC3_AVGS_MASK;
    ADC0->SC3 |=  ADC_SC3_AVGS(0b11);

    ADC0->SC3 |= ADC_SC3_CAL_MASK;

    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0U) {}

    if ((ADC0->SC3 & ADC_SC3_CALF_MASK) != 0U)
    {
        ADC0->SC3 |= ADC_SC3_CALF_MASK;
        // PRINTF("[ADC] Calibration FAILED\r\n");
        return;
    }

    cal_var  = 0U;
    cal_var += (uint16_t)ADC0->CLPD;
    cal_var += (uint16_t)ADC0->CLPS;
    cal_var += (uint16_t)ADC0->CLP4;
    cal_var += (uint16_t)ADC0->CLP3;
    cal_var += (uint16_t)ADC0->CLP2;
    cal_var += (uint16_t)ADC0->CLP1;
    cal_var += (uint16_t)ADC0->CLP0;
    cal_var >>= 1U;
    cal_var  |= 0x8000U;
    ADC0->PG  = (uint32_t)cal_var;

    cal_var  = 0U;
    cal_var += (uint16_t)ADC0->CLMD;
    cal_var += (uint16_t)ADC0->CLMS;
    cal_var += (uint16_t)ADC0->CLM4;
    cal_var += (uint16_t)ADC0->CLM3;
    cal_var += (uint16_t)ADC0->CLM2;
    cal_var += (uint16_t)ADC0->CLM1;
    cal_var += (uint16_t)ADC0->CLM0;
    cal_var >>= 1U;
    cal_var  |= 0x8000U;
    ADC0->MG  = (uint32_t)cal_var;

    // PRINTF("[ADC] Calibration OK\r\n");
    NVIC_EnableIRQ(ADC0_IRQn);
}

/* -----------------------------------------------------------------------
 * Public: sound_sensor_init  (RM 23.6.1.1)
 * ----------------------------------------------------------------------- */
void sound_sensor_init(void){
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;

    PORTB->PCR[0] &= ~PORT_PCR_MUX_MASK;
    PORTB->PCR[0] |=  PORT_PCR_MUX(0);

    adc_calibrate();

    ADC0->CFG1 &= ~ADC_CFG1_ADIV_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADIV(1);
    ADC0->CFG1 |=  ADC_CFG1_ADLSMP_MASK;
    ADC0->CFG1 &= ~ADC_CFG1_MODE_MASK;
    ADC0->CFG1 |=  ADC_CFG1_MODE(0b11);
    ADC0->CFG1 &= ~ADC_CFG1_ADICLK_MASK;
    ADC0->CFG1 |=  ADC_CFG1_ADICLK(0b00);

    ADC0->SC2 &= ~ADC_SC2_ADTRG_MASK;
    ADC0->SC2 &= ~ADC_SC2_REFSEL_MASK;
    ADC0->SC2 |=  ADC_SC2_REFSEL(0b01);

    ADC0->SC3 &= ~ADC_SC3_AVGE_MASK;
    ADC0->SC3 |=  ADC_SC3_AVGE(0);
    ADC0->SC3 &= ~ADC_SC3_ADCO_MASK;
    ADC0->SC3 |=  ADC_SC3_ADCO(0);

    ADC0->SC1[0] &= ~ADC_SC1_ADCH_MASK;
    ADC0->SC1[0] |=  ADC_SC1_ADCH(0x1FU);
}

/* -----------------------------------------------------------------------
 * Public: sound_sensor_read  (RM 23.5.4.1)
 * MUST be called while holding gADCMutex
 * ----------------------------------------------------------------------- */
uint16_t sound_sensor_read(void){
    /* Single atomic write — no interrupt for polling */
    ADC0->SC1[0] = ADC_SC1_ADCH(SOUND_ADC_CHANNEL);

    while ((ADC0->SC1[0] & ADC_SC1_COCO_MASK) == 0U) {}

    return (uint16_t)(ADC0->R[0] & 0xFFFFU);
}


/* -----------------------------------------------------------------------
 * Public: vSoundTask
 * Phase 1 — 5s calibration (holds gADCMutex for full window)
 * Phase 2 — continuous peak-hold monitoring
 * ----------------------------------------------------------------------- */
/* -----------------------------------------------------------------------
 * Public: vSoundTask
 * Phase 1 — 5s calibration (holds gADCMutex for full window)
 * Phase 2 — peak-hold monitoring against baseline
 *            Trigger = (peak − baseline) > TRIGGER_DELTA
 *            sound_raw now stores the per-window MAX, not the last sample
 * ----------------------------------------------------------------------- */
void vSoundTask(void *pvParameters){
    (void)pvParameters;

    uint32_t sum      = 0U;
    uint32_t count    = 0U;
    uint16_t sample   = 0U;
    uint16_t baseline = 0U;

    /* ---- Phase 1: Calibration ---- */
    for (uint32_t t = 0U; t < CAL_TIME_MS; t += SAMPLE_DELAY_MS)
	{
		xSemaphoreTake(gADCMutex, portMAX_DELAY);
		sound_sensor_init();
		sample = sound_sensor_read();
		xSemaphoreGive(gADCMutex);
		sum += sample;
		count++;

		vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
	}

	baseline = (uint16_t)(sum / count);

    // PRINTF("Sound: baseline=%u  trigger_delta=%u\r\n\r\n", baseline, TRIGGER_DELTA);

    /* ---- Phase 2: Peak-to-peak monitoring ---- */
    while (1)
    {
        uint16_t peak      = 0U;
        uint16_t trough    = 0xFFFFU;
        uint8_t  aboveCount = 0U;

        /* Sample as fast as possible — 30 samples × 1ms ≈ 30ms window */
        for (uint32_t i = 0U; i < PEAK_SAMPLE_COUNT; i++)
        {
            xSemaphoreTake(gADCMutex, portMAX_DELAY);
            sample = sound_sensor_read();
            xSemaphoreGive(gADCMutex);

            if (sample > peak)   peak   = sample;
            if (sample < trough) trough = sample;

            /* Count samples deviating from baseline in either direction */
            uint16_t dev = (sample > baseline)
                           ? (sample - baseline)
                           : (baseline - sample);
            if (dev > TRIGGER_DELTA) aboveCount++;

            // vTaskDelay(pdMS_TO_TICKS(PEAK_SAMPLE_DELAY));
        }

        /* Peak-to-peak swing — catches both the compression and rarefaction
         * of the pressure wave, so sensitivity is ~2x vs. one-sided peakDev */
        uint16_t swing = (peak > trough) ? (peak - trough) : 0U;

        /* Construction noise: large swing, at least 1 spike above threshold.
         * No upper bound on aboveCount — impacts can last several ms          */
        bool triggered = (swing > TRIGGER_DELTA) && (aboveCount >= 1U);

        // PRINTF("[Sound] peak=%u trough=%u swing=%u above=%u trig=%d\r\n",
        //         peak, trough, swing, aboveCount, triggered);

        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            gSensorData.sound_raw       = peak;   /* still report max reading */
            gSensorData.sound_triggered = triggered ? 1U : 0U;
            xSemaphoreGive(gSensorMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }
}

