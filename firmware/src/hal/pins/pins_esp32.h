/**
 * ============================================================================
 * OpenPlotter — ESP32 DevKit Standalone Pin Definitions
 * ============================================================================
 *
 * ESP32 standalone mode: The ESP32 handles EVERYTHING — motion control,
 * WiFi, WebSocket server, Bluetooth, and the embedded web UI.
 *
 * PIN SAFETY NOTES:
 * - GPIO 0, 2, 5, 12, 15 are boot-strapping pins. Avoid for inputs.
 * - GPIO 6-11 are connected to internal flash. DO NOT USE.
 * - GPIO 34, 35, 36, 39 are INPUT ONLY (no internal pull-up/down).
 *   Perfect for limit switches with external pull-ups.
 * - ESP32 is 3.3V logic! Most stepper drivers (A4988, DRV8825) accept
 *   3.3V as HIGH, but verify with your specific driver. Use level
 *   shifters if unsure.
 *
 * ============================================================================
 */

#ifndef PINS_ESP32_H
#define PINS_ESP32_H

// ── X Axis ──────────────────────────────────────────────────────────────────
#define X_STEP_PIN          26      // Safe GPIO, no boot conflict
#define X_DIR_PIN           27      // Safe GPIO
#define X_ENABLE_PIN        14      // Has pull-down at boot (OK for enable)

// ── Y Axis ──────────────────────────────────────────────────────────────────
#define Y_STEP_PIN          25      // Safe GPIO
#define Y_DIR_PIN           33      // Safe GPIO
#define Y_ENABLE_PIN        32      // Safe GPIO

// ── Z Axis (optional — blade pressure stepper) ─────────────────────────────
#define Z_STEP_PIN          19      // VSPI MISO (available if not using SPI)
#define Z_DIR_PIN           18      // VSPI SCK (available if not using SPI)
#define Z_ENABLE_PIN        5       // VSPI SS — goes HIGH at boot (OK for enable)

// ── C Axis (optional — tangential knife rotation) ──────────────────────────
#define C_STEP_PIN          4       // Safe GPIO
#define C_DIR_PIN           0       // Boot pin — ensure pull-up. OK for output.
#define C_ENABLE_PIN        15      // Boot pin — OK for output after boot

// ── Endstops / Limit Switches ──────────────────────────────────────────────
// Using input-only GPIOs — perfect for switches, can't accidentally output
#define X_MIN_PIN           34      // Input only — needs external pull-up (10kΩ to 3.3V)
#define X_MAX_PIN           35      // Input only — needs external pull-up
#define Y_MIN_PIN           36      // VP, input only — needs external pull-up
#define Y_MAX_PIN           39      // VN, input only — needs external pull-up

// Z endstops (optional, uses regular GPIOs with internal pull-up)
#define Z_MIN_PIN           23      // Has internal pull-up
// #define Z_MAX_PIN        // Uncomment and assign if needed

// ── Servo ───────────────────────────────────────────────────────────────────
// ESP32 doesn't have native Servo library — uses LEDC PWM channels
#define PEN_SERVO_PIN       13      // LEDC channel 0
#define AUX_SERVO_PIN       12      // LEDC channel 1 (boot strapping — OK after boot)

// Servo LEDC configuration
#define SERVO_LEDC_CHANNEL  0
#define SERVO_LEDC_FREQ     50      // 50Hz standard servo frequency
#define SERVO_LEDC_BITS     16      // 16-bit resolution for precise angles

// ── Solenoid (alternative to servo) ────────────────────────────────────────
#define SOLENOID_PIN        2       // Onboard LED pin — also drives solenoid MOSFET
#define AUX_POWER_PIN       15      // Auxiliary high-current output

// ── TMC2209 UART ────────────────────────────────────────────────────────────
#ifdef HAS_TMC_UART
    #define TMC_UART_TX_PIN     17      // UART2 TX
    #define TMC_UART_RX_PIN     16      // UART2 RX
#endif

// ── Status LED ──────────────────────────────────────────────────────────────
#define STATUS_LED_PIN      2       // Onboard blue LED on most DevKit boards

// ── Emergency Stop ──────────────────────────────────────────────────────────
#define ESTOP_PIN           23      // Use with internal pull-up, NC button to GND

// ── SD Card (VSPI) ─────────────────────────────────────────────────────────
// Only available if not using Z-axis stepper on SPI pins
// #define SD_CS_PIN        5
// #define SD_MOSI_PIN      23
// #define SD_MISO_PIN      19
// #define SD_SCK_PIN       18

// ── I2C ─────────────────────────────────────────────────────────────────────
#define SDA_PIN             21      // Default I2C data
#define SCL_PIN             22      // Default I2C clock

// ── Buzzer ──────────────────────────────────────────────────────────────────
#define BUZZER_PIN          12      // Shares with AUX_SERVO — choose one

// ── WiFi/BT ─────────────────────────────────────────────────────────────────
// WiFi and Bluetooth are built into the ESP32 — no pin assignments needed.
// The antenna is on the PCB module itself.

#endif // PINS_ESP32_H
