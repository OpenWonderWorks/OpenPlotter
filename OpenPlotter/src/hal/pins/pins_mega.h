/**
 * ============================================================================
 * OpenPlotter — Arduino Mega 2560 Standalone Pin Definitions
 * ============================================================================
 *
 * For Mega 2560 WITHOUT a RAMPS shield. Use this when wiring stepper
 * drivers directly to Mega pins (e.g., with a custom breakout board
 * or breadboard prototyping).
 *
 * ============================================================================
 */

#ifndef PINS_MEGA_H
#define PINS_MEGA_H

// ── X Axis ──────────────────────────────────────────────────────────────────
#define X_STEP_PIN          2
#define X_DIR_PIN           3
#define X_ENABLE_PIN        4

// ── Y Axis ──────────────────────────────────────────────────────────────────
#define Y_STEP_PIN          5
#define Y_DIR_PIN           6
#define Y_ENABLE_PIN        7

// ── Z Axis (optional) ──────────────────────────────────────────────────────
#define Z_STEP_PIN          8
#define Z_DIR_PIN           9
#define Z_ENABLE_PIN        10

// ── C Axis (optional — tangential knife) ────────────────────────────────────
#define C_STEP_PIN          26
#define C_DIR_PIN           28
#define C_ENABLE_PIN        24

// ── Endstops ────────────────────────────────────────────────────────────────
#define X_MIN_PIN           22
#define X_MAX_PIN           23
#define Y_MIN_PIN           24
#define Y_MAX_PIN           25
#define Z_MIN_PIN           26
#define Z_MAX_PIN           27

// ── Servo ───────────────────────────────────────────────────────────────────
#define PEN_SERVO_PIN       11      // PWM capable
#define AUX_SERVO_PIN       12      // PWM capable

// ── Solenoid ────────────────────────────────────────────────────────────────
#define SOLENOID_PIN        44      // Digital output
#define AUX_POWER_PIN       45

// ── TMC UART ────────────────────────────────────────────────────────────────
#ifdef HAS_TMC_UART
    #define TMC_UART_TX_PIN     40
    #define TMC_UART_RX_PIN     41
#endif

// ── Misc ────────────────────────────────────────────────────────────────────
#define STATUS_LED_PIN      13
#define ESTOP_PIN           30
#define BUZZER_PIN          31
#define PS_ON_PIN           39
#define SD_CS_PIN           53

#endif // PINS_MEGA_H
