#include "tap.h"
#include "shared_data.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

SemaphoreHandle_t gTapSemaphore = NULL;
static volatile TickType_t sLastTapTick = 0;

void TAP_Init(void) {
    SIM->SCGC5 |= TAP_PORT_CLOCK_MASK;

    TAP_PORT->PCR[TAP_PIN] = PORT_PCR_MUX(1)
                           | PORT_PCR_PE_MASK
                           | PORT_PCR_PS_MASK
                           | PORT_PCR_IRQC(0xA);

    TAP_GPIO->PDDR &= ~(1u << TAP_PIN);

    gTapSemaphore = xSemaphoreCreateBinary();
    configASSERT(gTapSemaphore != NULL);

    NVIC_SetPriority(TAP_IRQn, 0);
    NVIC_ClearPendingIRQ(TAP_IRQn);
    NVIC_EnableIRQ(TAP_IRQn);
}

void PORTC_PORTD_IRQHandler(void) {
    if (TAP_PORT->ISFR & (1u << TAP_PIN)) {
        TAP_PORT->ISFR = (1u << TAP_PIN);
        TickType_t now = xTaskGetTickCountFromISR();
        if ((now - sLastTapTick) > pdMS_TO_TICKS(TAP_DEBOUNCE_MS)) {
            sLastTapTick = now;

            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(gTapSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

void vTapTask(void *pvParameters) {
    (void)pvParameters;
    int focusSnapshot = 0;

    while (1) {
        if (xSemaphoreTake(gTapSemaphore, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gSensorData.tap_event = 1;
                gSensorData.focus_mode ^= 1;
                focusSnapshot = gSensorData.focus_mode;
                xSemaphoreGive(gSensorMutex);
            }
        }
    }
}
