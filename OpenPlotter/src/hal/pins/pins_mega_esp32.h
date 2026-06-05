/**
 * ============================================================================
 * OpenPlotter — Hybrid Mega + ESP32 Pin Definitions
 * ============================================================================
 *
 * HYBRID MODE: Arduino Mega handles all motion control (steppers, servos,
 * homing) while an ESP32 provides WiFi, Bluetooth, and serves the web UI.
 * They communicate over a serial bridge.
 *
 * This file defines pins for the MEGA side. The ESP32 side uses the
 * esp32_bridge firmware which only needs the serial bridge pins.
 *
 * SERIAL BRIDGE WIRING:
 *   Mega TX1 (Pin 18) ──→ ESP32 GPIO16 (RX2)
 *   Mega RX1 (Pin 19) ←── ESP32 GPIO17 (TX2)
 *   Mega GND ──────────── ESP32 GND (CRITICAL!)
 *
 * ============================================================================
 */

#ifndef PINS_MEGA_ESP32_H
#define PINS_MEGA_ESP32_H

// Inherit RAMPS 1.4 pin definitions (most common hybrid setup)
#include "pins_ramps14.h"

// Override the bridge serial settings
#undef BRIDGE_SERIAL
#undef BRIDGE_BAUD
#define BRIDGE_SERIAL       Serial1     // Hardware Serial1 on Mega
#define BRIDGE_BAUD         115200      // Match ESP32 Serial2 baud rate

// NOTE: Serial1 uses Pin 18 (TX1) and Pin 19 (RX1) on the Mega.
// These pins overlap with Z_MIN (Pin 18) and Z_MAX (Pin 19) on RAMPS 1.4!
// In hybrid mode, Z endstops are NOT available on the standard RAMPS headers.
//
// WORKAROUND: If you need Z endstops in hybrid mode, reroute them to
// spare AUX pins:
#ifdef HYBRID_MODE
    #undef Z_MIN_PIN
    #undef Z_MAX_PIN
    #define Z_MIN_PIN       42      // Rerouted to AUX-2
    #define Z_MAX_PIN       44      // Rerouted to AUX-2
#endif

#endif // PINS_MEGA_ESP32_H
