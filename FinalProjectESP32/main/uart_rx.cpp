#include <Arduino.h>
#include "uart_rx.h"
#include "uart_tx.h" /* UART_TX_SendCmd, TX_CMD_LED_ON/OFF */
#include "shared_data.h"
#include "uart_packet.h"
#include "session_tracker.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ── Private helpers ─────────────────────────────────────────────────────── */

/*
 * handleFocusTransition()
 * Detects edges on the focus_mode field from each incoming packet:
 *   0 → 1 : focus activated  → start a new session
 *   1 → 0 : focus deactivated → end session and queue a Gemini report
 * sPrevFocus = 0xFF on first call so no spurious event fires.
 */
static void handleFocusTransition(uint8_t focus) {
  static uint8_t sPrevFocus = 0xFF;

  // Serial.printf("[Focus] Current: %d, Previous: %d\n", focus, sPrevFocus);

  if (sPrevFocus == 0xFF) {
    sPrevFocus = focus;
    return;
  }

  if (sPrevFocus == 0 && focus == 1) {
    Serial.println("[Focus] Session START triggered");
    Session_Start();
  } else if (sPrevFocus == 1 && focus == 0) {
    Serial.println("[Focus] Session END triggered");
    SessionSummary_t summary;
    if (Session_End(&summary)) {
      Serial.println("[Focus] Session ended, storing report");
      Session_StorePendingReport(&summary);
    } else {
      Serial.println("[Focus] Session ended with no samples");
    }
  }

  sPrevFocus = focus;
}

/*
 * validateChecksum()
 * XOR of bytes [2..8] must equal byte [9]
 * Matches build_packet() on MCXC444 side exactly
 */
static bool validateChecksum(uint8_t *pkt) {
  uint8_t chk = 0;
  for (uint8_t i = 2; i <= 8; i++) {
    chk ^= pkt[i];
  }
  return chk == pkt[9];
}

/*
 * parseAndStore()
 * Reconstructs uint16 from H and L bytes
 * Flashes LED on valid packet
 * Writes to gSensorData under mutex
 */
static void parseAndStore(uint8_t *pkt) {
  uint8_t tap = pkt[2];
  uint8_t focus = pkt[3];
  uint16_t light = ((uint16_t)pkt[4] << 8) | pkt[5];
  uint16_t sound = ((uint16_t)pkt[6] << 8) | pkt[7];
  uint8_t triggered = pkt[8];

  handleFocusTransition(focus);
  /*
     * Send LED command back to MCXC444 on every valid packet.
     * focus_mode == 1 → LED ON (0x01)
     * focus_mode == 0 → LED OFF (0x00)
     * MCXC444 vRXTask will pick this up and update gSensorData.on_off.
     */
  UART_TX_SendCmd(focus == 1 ? TX_CMD_LED_ON : TX_CMD_LED_OFF);
  if (focus == 1) {
    Serial.println("[UART] Valid packet received:");
    Serial.print("  tap_event:       ");
    Serial.println(tap);
    Serial.print("  focus_mode:      ");
    Serial.println(focus);
    Serial.print("  light_raw:       ");
    Serial.println(light);
    Serial.print("  sound_raw:       ");
    Serial.println(sound);
    Serial.print("  sound_triggered: ");
    Serial.println(triggered);
  }

  if (xSemaphoreTake(gSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    gSensorData.tap_event = tap;
    gSensorData.focus_mode = focus;
    gSensorData.light_raw = light;
    gSensorData.sound_raw = sound;
    gSensorData.sound_triggered = triggered;
    xSemaphoreGive(gSensorMutex);
  } else {
    Serial.println("[UART] Warning: could not take mutex — skipping write");
  }
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  PUBLIC FUNCTIONS                                                         */
/* ═════════════════════════════════════════════════════════════════════════ */

void UART_RX_Init(void) {
  /* LED setup */
  pinMode(MCXC_LED_PIN, OUTPUT);
  digitalWrite(MCXC_LED_PIN, LOW);  // start OFF

  /*
     * Configure UART1 — receive only
     * TX pin set to UART_PIN_NO_CHANGE — ESP32 does not transmit
     */
  uart_config_t uart_config = {
    .baud_rate = MCXC_UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  /* RX buffer 256 bytes, no TX buffer needed */
  uart_driver_install(MCXC_UART_PORT, 256, 0, 0, NULL, 0);
  uart_param_config(MCXC_UART_PORT, &uart_config);

  /*
     * Only set RX pin — TX is UART_PIN_NO_CHANGE
     * This puts ESP32 in receive-only mode
     */
  uart_set_pin(MCXC_UART_PORT,
               UART_PIN_NO_CHANGE,   // TX — not used
               MCXC_UART_RXD_PIN,    // RX — GPIO17
               UART_PIN_NO_CHANGE,   // RTS — not used
               UART_PIN_NO_CHANGE);  // CTS — not used

  Serial.println("[UART] Receiver initialised — RX only mode");
  Serial.print("[UART] Listening on GPIO");
  Serial.print(MCXC_UART_RXD_PIN);
  Serial.print(" at ");
  Serial.print(MCXC_UART_BAUD);
  Serial.println(" baud");
  Serial.print("[UART] LED indicator on GPIO");
  Serial.println(MCXC_LED_PIN);
}

/* ═════════════════════════════════════════════════════════════════════════ */
/*  TASK                                                                     */
/* ═════════════════════════════════════════════════════════════════════════ */

void vUartRxTask(void *pvParameters) {
  (void)pvParameters;

  uint8_t buf[PACKET_LEN];
  uint8_t idx = 0;
  bool synced = false;
  uint8_t byte = 0;

  Serial.println("[UART] Receive task started — waiting for packets...");

  while (1) {
    /*
         * Block up to 10ms waiting for a byte.
         * Returns number of bytes read — 0 means timeout, try again.
         */
    int received = uart_read_bytes(MCXC_UART_PORT,
                                   &byte, 1,
                                   pdMS_TO_TICKS(10));
    if (received <= 0) {
      continue;
    }

    /* ── Sync on start markers ──────────────────────────────────────── */
    if (!synced) {
      if (idx == 0 && byte == PACKET_START1) {
        buf[idx++] = byte;
      } else if (idx == 1 && byte == PACKET_START2) {
        buf[idx++] = byte;
        synced = true;
      } else if (idx == 1 && byte == PACKET_START1) {
        /* Previous 0xAA was not a real start — keep this one */
        idx = 1;
      } else {
        idx = 0;
      }
      continue;
    }

    /* ── Collect remaining bytes ────────────────────────────────────── */
    buf[idx++] = byte;

    /* ── Full packet ────────────────────────────────────────────────── */
    if (idx == PACKET_LEN) {
      idx = 0;
      synced = false;

      /* Validate end marker */
      if (buf[PACKET_LEN - 1] != PACKET_END) {
        Serial.println("[UART] Error: bad end marker — dropped");
        continue;
      }

      /* Validate checksum */
      if (!validateChecksum(buf)) {
        Serial.println("[UART] Error: bad checksum — dropped");
        continue;
      }

      /* Valid — parse and store */
      parseAndStore(buf);
    }
  }
}