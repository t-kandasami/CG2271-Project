#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "peripherals.h"
#include "fsl_debug_console.h"
#include "shared_data.h"
#include "FreeRTOS.h"
#include "task.h"

#include "light_sensor.h"
#include "tap.h"
#include "led.h"
#include "sound_sensor.h"
#include "uart_tx.h"
#include "uart_rx.h"

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    gSensorMutex = xSemaphoreCreateMutex();
    gADCMutex = xSemaphoreCreateMutex();
    TAP_Init();
    led_init();
    initUART2_RXTX(MCXC_UART_BAUD);

    xTaskCreate(LIGHT_SENSOR_Task, "Light",  configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vTapTask, "Tap", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
    xTaskCreate(vSoundTask, "Sound", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    xTaskCreate(vLEDTask, "LED", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    // xTaskCreate(vPrintTask, "Print", 256, NULL, 3, NULL);
    xTaskCreate(vTxTask, "TXTask", configMINIMAL_STACK_SIZE+128, NULL, 2, NULL);
    xTaskCreate(vRXTask, "RXTask", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();
    while(1);
}
