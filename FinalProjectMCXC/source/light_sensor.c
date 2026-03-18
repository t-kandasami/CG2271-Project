#include "light_sensor.h"
#include "shared_data.h"
#include "MCXC444.h"
#include "fsl_debug_console.h"

#define LIGHT_SAMPLE_PERIOD_MS  500U

QueueHandle_t xLightQueue = NULL;
volatile int lightResult = 0;

void initLightADC(void)
{
    /* Disable & clear interrupt — same as your initADC() */
    NVIC_DisableIRQ(ADC0_IRQn);
    NVIC_ClearPendingIRQ(ADC0_IRQn);

    /* Enable clock gating to ADC0 and PORTE */
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;

    /* Set PTE22 to ALT0 (analog) — MUX=0, same as your PTE20/21 */
    PORTE->PCR[LIGHT_ADC_PIN] &= ~PORT_PCR_MUX_MASK;
    PORTE->PCR[LIGHT_ADC_PIN] |= PORT_PCR_MUX(0);

    /* Enable ADC interrupt */
    ADC0->SC1[0] |= ADC_SC1_AIEN_MASK;

    /* Single-ended mode — DIFF=0 */
    ADC0->SC1[0] &= ~ADC_SC1_DIFF_MASK;
    ADC0->SC1[0] |= ADC_SC1_DIFF(0b0);

    /* 12-bit conversion — MODE=01 */
    ADC0->CFG1 &= ~ADC_CFG1_MODE_MASK;
    ADC0->CFG1 |= ADC_CFG1_MODE(0b01);

    /* Software trigger — ADTRG=0 */
    ADC0->SC2 &= ~ADC_SC2_ADTRG_MASK;

    /* Alternate voltage reference (VDDA) — same as your code */
    ADC0->SC2 &= ~ADC_SC2_REFSEL_MASK;
    ADC0->SC2 |= ADC_SC2_REFSEL(0b01);

    /* No averaging */
    ADC0->SC3 &= ~ADC_SC3_AVGE_MASK;
    ADC0->SC3 |= ADC_SC3_AVGE(0);

    /* No continuous conversion */
    ADC0->SC3 &= ~ADC_SC3_ADCO_MASK;
    ADC0->SC3 |= ADC_SC3_ADCO(0);

    /* Highest priority */
    NVIC_SetPriority(ADC0_IRQn, 1);
    NVIC_EnableIRQ(ADC0_IRQn);
}

void startLightADC(void)
{
    /* Single atomic write — triggers conversion with interrupt */
    ADC0->SC1[0] = ADC_SC1_ADCH(LIGHT_ADC_CHANNEL) | ADC_SC1_AIEN_MASK;
}

void ADC0_IRQHandler(void)
{
    /* Read result — clears COCO automatically */
    lightResult = ADC0->R[0];

    /* Notify FreeRTOS task from ISR */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint16_t sample = (uint16_t)lightResult;
    xQueueOverwriteFromISR(xLightQueue, &sample, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void LIGHT_SENSOR_Task(void *pvParameters)
{
    (void)pvParameters;
    uint16_t sample;

    xLightQueue = xQueueCreate(1, sizeof(uint16_t));
    configASSERT(xLightQueue != NULL);

    initLightADC();

    while(1) {
            xSemaphoreTake(gADCMutex, portMAX_DELAY);
            startLightADC();
            xSemaphoreGive(gADCMutex);

            if (xQueueReceive(xLightQueue, &sample, pdMS_TO_TICKS(1000)) == pdTRUE)
            {
                if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    gSensorData.light_raw = sample;
                    xSemaphoreGive(gSensorMutex);
                }

                vTaskDelay(pdMS_TO_TICKS(LIGHT_SAMPLE_PERIOD_MS));
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

}
