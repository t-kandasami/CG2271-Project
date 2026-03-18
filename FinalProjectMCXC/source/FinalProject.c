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
#include "sound_sensor.h"

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    BOARD_InitDebugConsole();

    gSensorMutex = xSemaphoreCreateMutex();
    gADCMutex = xSemaphoreCreateMutex();
    TAP_Init();

    xTaskCreate(LIGHT_SENSOR_Task, "Light",  256, NULL, 2, NULL);
    xTaskCreate(vTapTask, "Tap", 256, NULL, 3, NULL);
    xTaskCreate(vSoundTask, "Sound", 512, NULL, 1, NULL);

    xTaskCreate(vPrintTask, "Print", 256, NULL, 3, NULL);

    vTaskStartScheduler();
    while(1);
}
