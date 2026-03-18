#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define LIGHT_ADC_CHANNEL   3U   /* ADC0_SE3 → PTE22 */
#define LIGHT_ADC_PIN       22   /* PTE22             */

extern QueueHandle_t xLightQueue;
extern volatile int lightResult;

void initLightADC(void);
void startLightADC(void);

void LIGHT_SENSOR_Task(void *pvParameters);

#endif
