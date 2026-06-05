/**
 * ============================================================================
 * OpenPlotter — Hybrid Nano + ESP32 Pin Definitions
 * ============================================================================
 *
 * HYBRID MODE: Arduino Nano handles basic XY motion control while an ESP32
 * provides WiFi, Bluetooth, and serves the web UI. Communication via
 * SoftwareSerial bridge (Nano lacks a second hardware serial).
 *
 * LIMITATIONS:
 * - Only 2 axes (X, Y) — no Z stepper or tangential knife
 * - SoftwareSerial bridge at 57600 baud (lower than hardware serial)
 * - 12-block planner buffer (limited RAM)
 *
 * SERIAL BRIDGE WIRING:
 *   Nano A5 (SoftSerial TX) ──→ ESP32 GPIO16 (RX2)
 *   Nano A4 (SoftSerial RX) ←── ESP32 GPIO17 (TX2)
 *   Nano GND ─────────────────── ESP32 GND
 *
 * ============================================================================
 */

#ifndef PINS_NANO_ESP32_H
#define PINS_NANO_ESP32_H

// Inherit base Nano pin definitions
#include "pins_nano.h"

// SoftwareSerial bridge pins are already defined in pins_nano.h under
// HYBRID_MODE, but we ensure they're set here:
#ifndef BRIDGE_RX_PIN
    #define BRIDGE_RX_PIN   A4
#endif
#ifndef BRIDGE_TX_PIN
    #define BRIDGE_TX_PIN   A5
#endif
#ifndef BRIDGE_BAUD
    #define BRIDGE_BAUD     57600
#endif

#endif // PINS_NANO_ESP32_H
