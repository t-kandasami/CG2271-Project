#include "led.h"
#include "shared_data.h"
#include "fsl_debug_console.h"

static void led_on(void)
{
    LED_TPM->CONTROLS[LED_TPM_CHANNEL].CnV = LED_CV;
    LED_TPM->CNT = 0U;
    LED_TPM->SC |= TPM_SC_CMOD(1U);
}

static void led_off(void)
{
	LED_TPM->SC &= ~TPM_SC_CMOD_MASK;
	LED_TPM->CONTROLS[LED_TPM_CHANNEL].CnV = 0U;

}

void led_init(void)
{
    SIM->SCGC5 |= LED_PORT_CLK;
    SIM->SCGC6 |= LED_TPM_CLK;

    /* IRC48M as TPM clock source */
    SIM->SOPT2 &= ~SIM_SOPT2_TPMSRC_MASK;
    SIM->SOPT2 |=  SIM_SOPT2_TPMSRC(1U);

    /* PTB1 = ALT3 = TPM1_CH1 */
    LED_PORT->PCR[LED_PIN] &= ~PORT_PCR_MUX_MASK;
    LED_PORT->PCR[LED_PIN] |=  PORT_PCR_MUX(LED_MUX);

    /* TPM1: edge-aligned PWM, prescaler /8 → 6 MHz */
    LED_TPM->SC  = 0U;
    LED_TPM->CNT = 0U;
    LED_TPM->MOD = LED_MOD;
    LED_TPM->SC  = TPM_SC_PS(TPM1_PS);

    LED_TPM->CONTROLS[LED_TPM_CHANNEL].CnSC =
        TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK;   // high-true EPWM
    LED_TPM->CONTROLS[LED_TPM_CHANNEL].CnV  = 0U;   /* off at start */
}

void vLEDTask(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        if (led_state) {
            led_on();
//            PRINTF("[LED] ON\r\n");
        } else {
        	led_off();
//        	PRINTF("[LED] OFF\r\n");
        }
        vTaskDelay(pdMS_TO_TICKS(LED_POLL_MS));
    }
}
