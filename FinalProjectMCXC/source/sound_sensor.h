/*
 * Copyright 2016-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file    sound_sensor.h
 * @brief   Sound Sensor driver for FRDM-MCXC444.
 *          Pin: PTB0 = ADC0_SE8 = Arduino A0 (J4 Pin 1)
 */

#ifndef SOUND_SENSOR_H
#define SOUND_SENSOR_H

#include <stdint.h>
#include "MCXC444.h"
#include "FreeRTOS.h"
#include "task.h"

/* -----------------------------------------------------------------------
 * ADC and Sensor Configuration
 * PTB0 = ADC0_SE8, ADCH = 01000 (RM Table 23-2)
 * ----------------------------------------------------------------------- */
#define SOUND_ADC_CHANNEL   8U      // PTB0 = ADC0_SE8
#define CAL_TIME_MS         5000U   // Calibration window (ms)
#define SAMPLE_DELAY_MS     25U     // Must exceed PEAK_SAMPLE_COUNT * PEAK_SAMPLE_DELAY

/* TRIGGER_DELTA is the ONLY sensitivity knob.
 * Must be ABOVE idle noise floor (~5-15 counts) but BELOW a clap (~300-500).
 * Lower = more sensitive. Start at 80 and tune from there.
 * Rule: TRIGGER_DELTA must always be greater than idle deviation. */
#define TRIGGER_DELTA       80U     // Tune this value only

/* Peak-hold: 5 samples x 4 ms apart = 20 ms window per cycle */
#define PEAK_SAMPLE_COUNT   5U
#define PEAK_SAMPLE_DELAY   4U      // ms between each peak-hold sample

/* -----------------------------------------------------------------------
 * FreeRTOS Task Configuration
 * ----------------------------------------------------------------------- */
#define SOUND_TASK_STACK_SIZE    256U
#define SOUND_TASK_PRIORITY      (tskIDLE_PRIORITY + 1U)

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */
void     sound_sensor_init(void);
uint16_t sound_sensor_read(void);
void     vSoundTask(void *pvParameters);

#endif /* SOUND_SENSOR_H */
