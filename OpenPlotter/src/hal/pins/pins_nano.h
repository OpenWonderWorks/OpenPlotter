/**
 * ============================================================================
 * OpenPlotter — Arduino Nano Pin Definitions
 * ============================================================================
 *
 * The Nano (ATmega328P) has very limited pins. This map supports a
 * basic 2-axis (XY) plotter with servo pen control. No Z stepper
 * or tangential knife is possible due to pin constraints.
 *
 * WARNING: The Nano has only 2KB RAM. The planner buffer is reduced
 * to 12 blocks and serial buffer to 128 bytes.
 *
 * ============================================================================
 */

#ifndef PINS_NANO_H
#define PINS_NANO_H

// ── X Axis ──────────────────────────────────────────────────────────────────
#define X_STEP_PIN          2       // D2
#define X_DIR_PIN           5       // D5
#define X_ENABLE_PIN        8       // D8

// ── Y Axis ──────────────────────────────────────────────────────────────────
#define Y_STEP_PIN          3       // D3
#define Y_DIR_PIN           6       // D6
#define Y_ENABLE_PIN        8       // Shared enable with X (pin constrained)

// ── Z Axis — NOT AVAILABLE on Nano standalone ──────────────────────────────
// #define Z_STEP_PIN
// #define Z_DIR_PIN
// #define Z_ENABLE_PIN

// ── C Axis — NOT AVAILABLE on Nano standalone ──────────────────────────────
// #define C_STEP_PIN
// #define C_DIR_PIN
// #define C_ENABLE_PIN

// ── Endstops ────────────────────────────────────────────────────────────────
#define X_MIN_PIN           9       // D9
#define Y_MIN_PIN           10      // D10
// No max endstops — insufficient pins

// ── Servo ───────────────────────────────────────────────────────────────────
#define PEN_SERVO_PIN       11      // D11 — Timer2 PWM

// ── Solenoid (alternative to servo) ────────────────────────────────────────
#define SOLENOID_PIN        12      // D12

// ── Misc ────────────────────────────────────────────────────────────────────
#define STATUS_LED_PIN      13      // D13 — onboard LED
#define ESTOP_PIN           A0      // Analog pin used as digital input

// ── Spindle / Direction (CNC Shield v3 compatible) ─────────────────────────
// If using Arduino CNC Shield v3 on Nano:
// #define X_STEP_PIN       2
// #define X_DIR_PIN        5
// #define Y_STEP_PIN       3
// #define Y_DIR_PIN        6
// #define Z_STEP_PIN       4
// #define Z_DIR_PIN        7
// #define ENABLE_PIN       8   (shared for all axes)

// ── Serial Bridge (hybrid mode) ────────────────────────────────────────────
#ifdef HYBRID_MODE
    // Nano only has one hardware serial (Serial), which is used for USB.
    // In hybrid mode, we use SoftwareSerial for the ESP32 bridge.
    #define BRIDGE_RX_PIN   A4      // SoftwareSerial RX ← ESP32 TX
    #define BRIDGE_TX_PIN   A5      // SoftwareSerial TX → ESP32 RX
    #define BRIDGE_BAUD     57600   // Lower baud for SoftwareSerial reliability
#endif

#endif // PINS_NANO_H
