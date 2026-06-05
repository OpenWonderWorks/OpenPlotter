/**
 * ============================================================================
 * OpenPlotter — RAMPS 1.4 Shield Pin Definitions (Arduino Mega 2560)
 * ============================================================================
 *
 * This is the PRIMARY pin map for the most common setup:
 *   Arduino Mega 2560 + RAMPS 1.4 shield
 *
 * The RAMPS 1.4 board sits directly on top of the Mega 2560. All pin
 * assignments below correspond to the Mega's digital/analog pin numbers
 * as mapped through the RAMPS 1.4 PCB traces.
 *
 * STEPPER DRIVER SLOTS:
 *   The RAMPS 1.4 has 5 stepper driver sockets: X, Y, Z, E0, E1
 *   Each accepts a Pololu-style driver: A4988, DRV8825, or TMC2209
 *
 *   ┌──────────────────────────────────────────────────────┐
 *   │  [X]     [Y]     [Z]     [E0]     [E1]              │
 *   │   ↑       ↑       ↑       ↑        ↑                │
 *   │  X-axis  Y-axis  Blade   Tang.   (spare)            │
 *   │                  pressure knife                      │
 *   │                  (opt.)  (opt.)                      │
 *   │                                                      │
 *   │  Install drivers with potentiometer facing AWAY      │
 *   │  from the power input terminals (D8/D9/D10 side)    │
 *   └──────────────────────────────────────────────────────┘
 *
 * ============================================================================
 */

#ifndef PINS_RAMPS14_H
#define PINS_RAMPS14_H

// ── X Axis ──────────────────────────────────────────────────────────────────
#define X_STEP_PIN          54      // A0 — X stepper step pulse
#define X_DIR_PIN           55      // A1 — X stepper direction
#define X_ENABLE_PIN        38      //    — X stepper enable (active LOW)

// ── Y Axis ──────────────────────────────────────────────────────────────────
#define Y_STEP_PIN          60      // A6 — Y stepper step pulse
#define Y_DIR_PIN           61      // A7 — Y stepper direction
#define Y_ENABLE_PIN        56      // A2 — Y stepper enable (active LOW)

// ── Z Axis (optional — blade pressure stepper) ─────────────────────────────
#define Z_STEP_PIN          46      //    — Z stepper step pulse
#define Z_DIR_PIN           48      //    — Z stepper direction
#define Z_ENABLE_PIN        62      // A8 — Z stepper enable (active LOW)

// ── E0 Axis (optional — tangential knife C-axis rotation) ──────────────────
#define C_STEP_PIN          26      //    — E0 stepper step pulse
#define C_DIR_PIN           28      //    — E0 stepper direction
#define C_ENABLE_PIN        24      //    — E0 stepper enable (active LOW)

// ── E1 Axis (spare — available for future use) ─────────────────────────────
#define E1_STEP_PIN         36      //    — E1 stepper step pulse
#define E1_DIR_PIN          34      //    — E1 stepper direction
#define E1_ENABLE_PIN       30      //    — E1 stepper enable (active LOW)

// ── Endstop / Limit Switch Pins ─────────────────────────────────────────────
// RAMPS 1.4 has 6 endstop connectors: X_MIN, X_MAX, Y_MIN, Y_MAX, Z_MIN, Z_MAX
// Each connector has 3 pins: Signal, GND, VCC(5V)
// For sensorless homing, the TMC2209 DIAG pin connects to these same pins.
#define X_MIN_PIN           3       // INT5 — X min limit / TMC DIAG X
#define X_MAX_PIN           2       // INT4 — X max limit (optional)
#define Y_MIN_PIN           14      // PCINT10 — Y min limit / TMC DIAG Y
#define Y_MAX_PIN           15      // PCINT9 — Y max limit (optional)
#define Z_MIN_PIN           18      // INT3 — Z min limit (optional)
#define Z_MAX_PIN           19      // INT2 — Z max limit (optional)

// ── Servo Pins ──────────────────────────────────────────────────────────────
// RAMPS 1.4 has 4 servo headers near the reset button:
//   SERVO0, SERVO1, SERVO2, SERVO3
// Each has Signal, VCC(5V), GND
// NOTE: Servo VCC is powered from the Mega's 5V regulator or an external BEC.
//       High-torque servos may need an external 5V BEC supply.
#define SERVO_0_PIN         11      // PWM — Primary pen/blade servo
#define SERVO_1_PIN         6       // PWM — Secondary servo (spare)
#define SERVO_2_PIN         5       // PWM — Servo 2 (spare)
#define SERVO_3_PIN         4       // PWM — Servo 3 (spare)

// Aliases for clarity
#define PEN_SERVO_PIN       SERVO_0_PIN
#define AUX_SERVO_PIN       SERVO_1_PIN

// ── MOSFET / Heater / Fan Outputs ──────────────────────────────────────────
// These are the 3 power MOSFET outputs on RAMPS 1.4.
// Originally for heaters/fan, we repurpose them for plotter use.
#define HEATER_0_PIN        10      // D10 — MOSFET output (solenoid / aux power)
#define HEATER_1_PIN        8       // D8  — MOSFET output (high-power, for heated bed)
#define FAN_PIN             9       // D9  — MOSFET output (fan / solenoid)

// Plotter-specific aliases
#define SOLENOID_PIN        FAN_PIN         // Solenoid blade engagement
#define AUX_POWER_PIN       HEATER_0_PIN    // Auxiliary power output

// ── Power Supply Control ────────────────────────────────────────────────────
#define PS_ON_PIN           12      // ATX power supply control (active LOW)
                                    // Connect to ATX PS_ON (green wire)
                                    // Pull LOW to turn on PSU, HIGH to standby

// ── Status LED ──────────────────────────────────────────────────────────────
#define STATUS_LED_PIN      13      // Onboard LED on Mega

// ── I2C (AUX-1 header) ─────────────────────────────────────────────────────
// For LCD displays, I2C sensors, I/O expanders
#define SDA_PIN             20      // I2C data
#define SCL_PIN             21      // I2C clock

// ── SPI (ICSP header) ──────────────────────────────────────────────────────
// For SD card, SPI-based TMC drivers (TMC2130), etc.
#define SPI_MOSI_PIN        51
#define SPI_MISO_PIN        50
#define SPI_SCK_PIN         52
#define SPI_SS_PIN          53      // SD card chip select

// ── SD Card ─────────────────────────────────────────────────────────────────
#define SD_CS_PIN           53      // Directly on Mega's SS pin
#define SD_DETECT_PIN       49      // Card detect (active LOW, optional)

// ── TMC2209 UART ────────────────────────────────────────────────────────────
// For sensorless homing (StallGuard) and advanced driver configuration.
// The TMC2209 uses single-wire UART. Multiple drivers share a bus with
// unique addresses set via MS1/MS2 hardware pins.
//
// WIRING: Connect each TMC2209's PDN_UART pin through a 1kΩ resistor
// to the shared UART TX pin. Connect UART RX directly.
//
// TMC2209 addressing (set via MS1/MS2 pins on the driver):
//   Address 0: MS1=LOW,  MS2=LOW   — X driver
//   Address 1: MS1=HIGH, MS2=LOW   — Y driver
//   Address 2: MS1=LOW,  MS2=HIGH  — Z driver
//   Address 3: MS1=HIGH, MS2=HIGH  — C driver (E0 slot)
#ifdef HAS_TMC_UART
    #define TMC_UART_TX_PIN     40      // AUX-2 header, pin 40
    #define TMC_UART_RX_PIN     63      // AUX-2 header, A9
    // Alternatively, for single-wire UART, TX and RX can be the same pin
    // with appropriate resistor network. See WIRING_GUIDE.md for details.
#endif

// ── Hybrid Mode: Serial Bridge to ESP32 ────────────────────────────────────
// In hybrid mode, the Mega communicates with an ESP32 over Serial1.
// The ESP32 provides WiFi/BT and serves the web UI.
#ifdef HYBRID_MODE
    // Mega TX1 → ESP32 RX2 (GPIO16)
    // Mega RX1 → ESP32 TX2 (GPIO17)
    // MUST share common GND
    #define BRIDGE_SERIAL       Serial1
    #define BRIDGE_BAUD         115200
    // Note: Pin 18 (TX1) and Pin 19 (RX1) are hardware Serial1 on Mega.
    // These overlap with Z_MIN (18) and Z_MAX (19) endstop pins!
    // If using hybrid mode, Z endstops must be relocated or disabled.
#endif

// ── AUX Headers (spare I/O for user expansion) ─────────────────────────────
// AUX-2 header pins not used by TMC UART:
#define AUX2_PIN_1          63      // A9 (may conflict with TMC RX)
#define AUX2_PIN_2          40      // (may conflict with TMC TX)
#define AUX2_PIN_3          42
#define AUX2_PIN_4          44
#define AUX2_PIN_5          64      // A10
#define AUX2_PIN_6          65      // A11
#define AUX2_PIN_7          66      // A12

// AUX-3 header:
#define AUX3_PIN_1          49
#define AUX3_PIN_2          50      // SPI MISO
#define AUX3_PIN_3          51      // SPI MOSI
#define AUX3_PIN_4          52      // SPI SCK

// AUX-4 header (commonly used for LCD connections):
#define AUX4_PIN_1          16      // TX2
#define AUX4_PIN_2          17      // RX2
#define AUX4_PIN_3          23
#define AUX4_PIN_4          25
#define AUX4_PIN_5          27
#define AUX4_PIN_6          29
#define AUX4_PIN_7          31
#define AUX4_PIN_8          33
#define AUX4_PIN_9          35
#define AUX4_PIN_10         37
#define AUX4_PIN_11         39
#define AUX4_PIN_12         41
#define AUX4_PIN_13         43
#define AUX4_PIN_14         45
#define AUX4_PIN_15         47

// ── Emergency Stop ──────────────────────────────────────────────────────────
// Dedicated E-stop pin. Connect a normally-closed button between this pin
// and GND. When the button is pressed (circuit opens), the firmware halts.
#define ESTOP_PIN           41      // AUX-4 — dedicated emergency stop
                                    // Change to any unused digital pin

// ── Buzzer (optional) ───────────────────────────────────────────────────────
#define BUZZER_PIN          37      // AUX-4 — piezo buzzer for alerts

#endif // PINS_RAMPS14_H
