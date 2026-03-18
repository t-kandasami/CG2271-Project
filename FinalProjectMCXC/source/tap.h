#ifndef TAP_H
#define TAP_H

#include <stdint.h>
#include "MCXC444.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define TAP_PIN              2
#define TAP_PORT             PORTD
#define TAP_GPIO             PTD
#define TAP_PORT_CLOCK_MASK  SIM_SCGC5_PORTD_MASK
#define TAP_IRQn             PORTC_PORTD_IRQn
#define TAP_DEBOUNCE_MS      300U

extern SemaphoreHandle_t gTapSemaphore;
extern volatile uint32_t g_sysTick_ms;

void TAP_Init(void);
void vTapTask(void *pvParameters);

#endif
