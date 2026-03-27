#include "uart_rx.h"
#include "shared_data.h"
#include "fsl_debug_console.h"
#include "MCXC444.h"


/* ---------------------------------------------------------------------------
 * Low-level blocking read — yields to FreeRTOS while waiting so vTxTask
 * and other tasks are not starved during idle gaps between packets.
 * -------------------------------------------------------------------------*/
static uint8_t uart2_read_byte(void)
{
	while (!(UART2->S1 & UART_S1_RDRF_MASK));
    return (uint8_t)UART2->D;   /* reading D clears RDRF */
}

/* ---------------------------------------------------------------------------
 * vRXTask
 *
 * Receives 5-byte command packets from the ESP32 and updates
 * gSensorData.on_off, which vLEDTask already polls every LED_POLL_MS.
 *
 * Frame-sync strategy: discard bytes until START1 is found, then
 * validate each subsequent field — restart sync on any mismatch.
 * -------------------------------------------------------------------------*/
void vRXTask(void *pvParameters)
{
    (void)pvParameters;
    uint8_t b;

    while(1){
        /* 1. Sync: wait for START1 */
        do { b = uart2_read_byte(); } while (b != PACKET_START1);

        /* 2. START2 */
        if (uart2_read_byte() != PACKET_START2) continue;

        /* 3. CMD */
        uint8_t cmd = uart2_read_byte();

        /* 4. Checksum */
        uint8_t chk = uart2_read_byte();
        if (chk != RX_CHECKSUM(cmd))
        {
//            PRINTF("[RX] Checksum fail: cmd=0x%02X got=0x%02X exp=0x%02X\r\n",
//                   cmd, chk, RX_CHECKSUM(cmd));
            continue;
        }

        /* 5. END marker */
        if (uart2_read_byte() != PACKET_END) continue;

        led_state = (cmd == RX_CMD_LED_ON) ? 1 : 0;
//        PRINTF("[RX] LED %s\r\n", led_state ? "ON" : "OFF");
    }
}
