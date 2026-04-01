#include <Arduino.h>
#include "uart_tx.h"
#include "uart_rx.h"       /* MCXC_UART_PORT, MCXC_UART_TXD_PIN */
#include "uart_packet.h"   /* PACKET_START1, PACKET_START2, PACKET_END */

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ═════════════════════════════════════════════════════════════════════════ */
/*  INIT                                                                     */
/* ═════════════════════════════════════════════════════════════════════════ */

/*
 * UART_TX_Init()
 *
 * The UART1 driver is already installed by UART_RX_Init().
 * This call adds GPIO16 as the active TX pin on that same driver instance.
 * Passing UART_PIN_NO_CHANGE for all other pins leaves RX/RTS/CTS alone.
 */
void UART_TX_Init(void) {
    uart_set_pin(MCXC_UART_PORT,
                 MCXC_UART_TXD_PIN,   /* TX — GPIO16 (now active)   */
                 UART_PIN_NO_CHANGE,   /* RX — already set           */
                 UART_PIN_NO_CHANGE,   /* RTS — not used             */
                 UART_PIN_NO_CHANGE);  /* CTS — not used             */

    Serial.println("[UART_TX] TX enabled on GPIO" + String(MCXC_UART_TXD_PIN));
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  SEND                                                                     */
/* ═════════════════════════════════════════════════════════════════════════ */

/*
 * UART_TX_SendCmd()
 *
 * Builds the 5-byte packet expected by MCXC444 vRXTask and writes it
 * to UART1 in a single uart_write_bytes() call (atomic at driver level).
 *
 * Packet layout:
 *   [0] PACKET_START1   0xAA
 *   [1] PACKET_START2   0x55
 *   [2] cmd             TX_CMD_LED_ON (0x01) or TX_CMD_LED_OFF (0x00)
 *   [3] checksum        cmd ^ 0xFF
 *   [4] PACKET_END      0xBB
 */
void UART_TX_SendCmd(uint8_t cmd) {
    uint8_t pkt[TX_PKT_LEN] = {
        PACKET_START1,        /* 0xAA */
        PACKET_START2,        /* 0x55 */
        cmd,
        TX_CHECKSUM(cmd),     /* cmd ^ 0xFF */
        PACKET_END            /* 0xBB */
    };

    int written = uart_write_bytes(MCXC_UART_PORT,
                                   (const char *)pkt,
                                   TX_PKT_LEN);

    if (written == TX_PKT_LEN) {
        // Serial.print("[UART_TX] Sent cmd=0x");
        //Serial.print(cmd, HEX);
        //Serial.println(cmd == TX_CMD_LED_ON ? "  (LED ON)" : "  (LED OFF)");
    } else {
        //Serial.println("[UART_TX] Warning: packet write incomplete");
    }
}
