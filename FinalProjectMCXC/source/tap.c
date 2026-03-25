#include "tap.h"
#include "shared_data.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

SemaphoreHandle_t gTapSemaphore = NULL;
static volatile TickType_t debounceLastTapTick = 0;
static volatile TickType_t doubleTapLastTapTick = 0;
static volatile TickType_t now = 0;
static volatile int possibleDoubleTap = 0;

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
        now = xTaskGetTickCountFromISR();
        if ((now - debounceLastTapTick) > pdMS_TO_TICKS(TAP_DEBOUNCE_MS)) {
            debounceLastTapTick = now;
            if (!(TAP_GPIO->PDIR & (1u << TAP_PIN))) {
				BaseType_t xHigherPriorityTaskWoken = pdFALSE;
				xSemaphoreGiveFromISR(gTapSemaphore, &xHigherPriorityTaskWoken);
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}

        }
    }
}

void vTapTask(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        // Block until first tap
        if (xSemaphoreTake(gTapSemaphore, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        while (xSemaphoreTake(gTapSemaphore, 0) == pdTRUE);
        // Wait for possible second tap — do NOT hold mutex here (prevents deadlock)
        BaseType_t secondTap = xSemaphoreTake(gTapSemaphore, pdMS_TO_TICKS(TAP_DOUBLE_TAP_MS_UL));

        // Only take mutex briefly to update shared data
        if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (secondTap == pdTRUE) {
                // Double tap: toggle on/off
                gSensorData.on_off ^= 1;
                // Clear paused when turning off
                if (!gSensorData.on_off) {
                    gSensorData.paused = 0;
                }
            } else {
                // Single tap: pause only if on
                if (gSensorData.on_off) {
                    gSensorData.paused ^= 1;
                }
            }
            gSensorData.tap_event = 1;
            xSemaphoreGive(gSensorMutex);
            if (xTxTaskHandle != NULL) {
				xTaskNotifyGive(xTxTaskHandle);
			}
        }
    }
}
