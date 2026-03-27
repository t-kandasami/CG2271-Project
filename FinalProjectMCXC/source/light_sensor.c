#include "light_sensor.h"
#include "shared_data.h"
#include "MCXC444.h"
#include "fsl_debug_console.h"

#define LIGHT_SAMPLE_PERIOD_MS  500U

QueueHandle_t xLightQueue = NULL;
volatile uint32_t lightResult = 0;

void initLightADC(void)
{
    NVIC_DisableIRQ(ADC0_IRQn);
    NVIC_ClearPendingIRQ(ADC0_IRQn);

    /* Disable SLCD — PTB1 is LCD_P1 */
    SIM->SCGC5 |=  SIM_SCGC5_SLCD_MASK;
    LCD->GCR   &= ~LCD_GCR_LCDEN_MASK;
    SIM->SCGC5 &= ~SIM_SCGC5_SLCD_MASK;

    /* Enable clocks */
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;

    /* PTB1 → pure analog, NO pull resistors */
    PORTB->PCR[LIGHT_ADC_PIN] = 0U;

    /* Disable ADC module before configuration */
    ADC0->SC1[0] = ADC_SC1_ADCH(0x1FU);

    ADC0->CFG1 = ADC_CFG1_MODE(0b11);         // 16-bit
    ADC0->CFG2 &= ~ADC_CFG2_MUXSEL_MASK;      // SE9 has no b-side
    ADC0->SC2  = ADC_SC2_REFSEL(0b01);        // VDDA ref, software trigger
    ADC0->SC3  = 0U;                            // no averaging, no continuous

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

    while(1) {
        xSemaphoreTake(gADCMutex, portMAX_DELAY);
        initLightADC();
        startLightADC();

        if (xQueueReceive(xLightQueue, &sample, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            xSemaphoreGive(gADCMutex);

            if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gSensorData.light_raw = sample;
                xSemaphoreGive(gSensorMutex);
            }
        }
        else {
        	xSemaphoreGive(gADCMutex);
        }
        // Delay until the next sample period
        vTaskDelay(pdMS_TO_TICKS(LIGHT_SAMPLE_PERIOD_MS));
    }
}
