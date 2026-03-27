#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "MCXC444.h"
#include "FreeRTOS.h"
#include "task.h"

/* PTB1 = TPM1_CH1 (ALT3) */
#define LED_TPM             TPM1
#define LED_TPM_CHANNEL     1U
#define LED_PORT            PORTE
#define LED_PIN             21U
#define LED_PORT_CLK        SIM_SCGC5_PORTE_MASK
#define LED_TPM_CLK         SIM_SCGC6_TPM1_MASK
#define LED_MUX             3U

#define LED_PWM_FREQ_HZ     1000U
#define LED_DUTY_PCT        50U
#define LED_POLL_MS         50U
#define LED_TASK_STACK      128U

#define TPM1_CLK_HZ         48000000U
#define TPM1_PS             3U
#define TPM1_FCLK           (TPM1_CLK_HZ / 8U)
#define LED_MOD             ((TPM1_FCLK / LED_PWM_FREQ_HZ) - 1U)
#define LED_CV              (LED_MOD * LED_DUTY_PCT / 100U)

void led_init(void);
void vLEDTask(void *pvParameters);

#endif /* LED_H */
