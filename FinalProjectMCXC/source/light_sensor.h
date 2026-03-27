#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define LIGHT_ADC_CHANNEL   12U
#define LIGHT_ADC_PIN       2

extern QueueHandle_t xLightQueue;
extern volatile uint32_t lightResult;

void initLightADC(void);
void startLightADC(void);

void LIGHT_SENSOR_Task(void *pvParameters);

#endif
