/**
 * ============================================================================
 * OpenPlotter — Master Configuration
 * ============================================================================
 *
 * Edit this file to match YOUR hardware setup before flashing.
 * All values here are defaults that can be overridden at runtime via
 * $$ settings (stored in EEPROM/NVS).
 *
 * ============================================================================
 */

#ifndef OPENPLOTTER_CONFIG_H
#define OPENPLOTTER_CONFIG_H

// ── Board Selection ─────────────────────────────────────────────────────────
/**
 * OpenPlotter hardware selector.
 * Uncomment ONE of the following profiles if compiling manually (e.g., in Arduino IDE).
 * If compiling with PlatformIO, leave them commented out; PlatformIO selects 
 * environments automatically via platformio.ini.
 */
// #define BOARD_MEGA_STANDALONE    // Standalone Arduino Mega 2560 (No ESP32)
#define BOARD_MEGA_RAMPS14       // Arduino Mega 2560 + RAMPS 1.4 Shield (No ESP32) - RECOMMENDED
// #define BOARD_NANO_STANDALONE    // Standalone Arduino Nano (No ESP32)
// #define BOARD_ESP32_STANDALONE   // Standalone ESP32 (No Arduino Mega/Nano)
// #define BOARD_MEGA_ESP32_HYBRID  // Arduino Mega 2560 (Motion) + ESP32 (WiFi bridge)
// #define BOARD_NANO_ESP32_HYBRID  // Arduino Nano (Motion) + ESP32 (WiFi bridge)

// Resolve selection profiles to macros
#if defined(BOARD_MEGA_STANDALONE)
    #define BOARD_MEGA
#elif defined(BOARD_MEGA_RAMPS14)
    #define BOARD_MEGA
    #define SHIELD_RAMPS14
    #define HAS_TMC_UART
#elif defined(BOARD_NANO_STANDALONE)
    #define BOARD_NANO
    #define PLANNER_BUFFER_SIZE 12
    #define SERIAL_RX_BUFFER_SIZE 128
#elif defined(BOARD_ESP32_STANDALONE)
    #define BOARD_ESP32
    #define HAS_WIFI
    #define HAS_BLUETOOTH
    #define HAS_TMC_UART
    #define PLANNER_BUFFER_SIZE 32
#elif defined(BOARD_MEGA_ESP32_HYBRID)
    #define BOARD_MEGA
    #define SHIELD_RAMPS14
    #define HYBRID_MODE
    #define HAS_TMC_UART
#elif defined(BOARD_NANO_ESP32_HYBRID)
    #define BOARD_NANO
    #define HYBRID_MODE
    #define PLANNER_BUFFER_SIZE 12
    #define SERIAL_RX_BUFFER_SIZE 128
#endif

// ── Include pin definitions for selected board ──────────────────────────────
#if defined(SHIELD_RAMPS14)
    #include "src/hal/pins/pins_ramps14.h"
#elif defined(BOARD_MEGA) && defined(HYBRID_MODE)
    #include "src/hal/pins/pins_mega_esp32.h"
#elif defined(BOARD_MEGA)
    #include "src/hal/pins/pins_mega.h"
#elif defined(BOARD_NANO) && defined(HYBRID_MODE)
    #include "src/hal/pins/pins_nano_esp32.h"
#elif defined(BOARD_NANO)
    #include "src/hal/pins/pins_nano.h"
#elif defined(BOARD_ESP32) && !defined(BRIDGE_MODE)
    #include "src/hal/pins/pins_esp32.h"
#endif

// ── Axis Configuration ──────────────────────────────────────────────────────
// Number of axes: 2 = XY plotter, 3 = XYZ (pressure), 4 = XYZC (tangential)
#ifndef NUM_AXES
    #define NUM_AXES                2
#endif

// ── Stepper Driver Configuration ────────────────────────────────────────────
// Driver type per axis: 
// 0 = A4988, 1 = DRV8825, 2 = TMC2208 (UART), 3 = TMC2209 (UART), 4 = TMC2130 (SPI), 5 = TMC5160 (SPI)
#define DEFAULT_DRIVER_X            3   // Default: TMC2209 for X axis
#define DEFAULT_DRIVER_Y            3   // Default: TMC2209 for Y axis
#define DEFAULT_DRIVER_Z            0   // Default: A4988 for Z axis (servo/solenoid)
#define DEFAULT_DRIVER_C            0   // Default: A4988 for C axis (tangential rotation)

// SPI CS (Chip Select) Pin Allocations (for TMC2130 & TMC5160 drivers)
#define X_CS_PIN                    53  // SS pin on Arduino Mega 2560
#define Y_CS_PIN                    49  // AUX-4 pin 4 on RAMPS
#define Z_CS_PIN                    40  // AUX-2 pin 1 on RAMPS
#define C_CS_PIN                    41  // AUX-2 pin 2 on RAMPS

// TMC UART / SPI Default Drive Currents & Modes
#define DEFAULT_TMC_RUN_CURRENT_MA  800   // mA - Running motor current
#define DEFAULT_TMC_HOLD_CURRENT_MA 400   // mA - Idle holding current (reduces heat)
#define DEFAULT_TMC_MICROSTEPS      16    // 16 microsteps (Bresenham sync resolution)
#define DEFAULT_TMC_STEALTHCHOP     true  // true = silent StealthChop, false = high-torque spreadCycle

// Steps per millimeter (adjust for your belt/leadscrew + microstepping)
// Common: 20-tooth GT2 pulley + 1/16 step = 80 steps/mm
#define DEFAULT_STEPS_PER_MM_X      80.0f
#define DEFAULT_STEPS_PER_MM_Y      80.0f
#define DEFAULT_STEPS_PER_MM_Z      400.0f   // If using Z stepper for pressure
#define DEFAULT_STEPS_PER_DEG_C     17.778f  // If using tangential knife C-axis

// ── Speed & Acceleration ────────────────────────────────────────────────────
#define DEFAULT_MAX_RATE_X          5000.0f   // mm/min
#define DEFAULT_MAX_RATE_Y          5000.0f   // mm/min
#define DEFAULT_MAX_RATE_Z          1000.0f   // mm/min
#define DEFAULT_MAX_RATE_C          3600.0f   // deg/min

#define DEFAULT_ACCELERATION        500.0f    // mm/s²
#define DEFAULT_JUNCTION_DEVIATION  0.01f     // mm — cornering tolerance

// ── Travel Limits (soft limits) ─────────────────────────────────────────────
#define DEFAULT_MAX_TRAVEL_X        300.0f    // mm — cutting area width
#define DEFAULT_MAX_TRAVEL_Y        300.0f    // mm — cutting area height
#define DEFAULT_MAX_TRAVEL_Z        10.0f     // mm (if applicable)
#define SOFT_LIMITS_ENABLED         true

// ── Homing ──────────────────────────────────────────────────────────────────
// Method: 0 = Sensored (limit switches), 1 = Sensorless (TMC StallGuard)
#define DEFAULT_HOMING_METHOD       0

// Sensored homing
#define DEFAULT_HOMING_SEEK_RATE    1000.0f   // mm/min — fast approach
#define DEFAULT_HOMING_FEED_RATE    100.0f    // mm/min — slow precision
#define DEFAULT_HOMING_PULLOFF      3.0f      // mm — distance to pull off switch
#define HOMING_DEBOUNCE_MS          10        // ms — switch debounce time

// Limit switch configuration (true = normally closed, recommended)
#define LIMIT_SWITCH_NC             true
#define LIMIT_SWITCH_PULLUP         true      // Enable internal pull-up resistors

// Sensorless homing (TMC2209 StallGuard)
#define DEFAULT_SG_THRESHOLD        50        // 0-255 (higher = less sensitive)
#define DEFAULT_SG_HOMING_CURRENT   500       // mA — reduced current during homing
#define DEFAULT_SG_HOMING_SPEED     40.0f     // mm/s — constant speed for detection
// NOTE: StallGuard requires TMC2209/TMC2130 with UART/SPI. A4988/DRV8825
// users MUST use sensored homing with physical limit switches.

// Homing direction: true = home toward positive end, false = toward negative
#define HOME_DIR_X                  false     // Home to X min
#define HOME_DIR_Y                  false     // Home to Y min
#define HOME_DIR_Z                  false     // Home to Z min (if applicable)

// ── Tool Configuration ──────────────────────────────────────────────────────
// Tool type: 0 = Servo pen/blade, 1 = Solenoid, 2 = Tangential knife
#define DEFAULT_TOOL_TYPE           0

// Servo settings (Tool type 0)
#define DEFAULT_SERVO_UP_ANGLE      30        // degrees — pen/blade UP position
#define DEFAULT_SERVO_DOWN_ANGLE    70        // degrees — pen/blade DOWN position
#define DEFAULT_SERVO_DELAY         150       // ms — wait for servo to reach position
#define SERVO_MIN_PULSE             544       // µs — servo min pulse width
#define SERVO_MAX_PULSE             2400      // µs — servo max pulse width

// Solenoid settings (Tool type 1)
#define SOLENOID_ENGAGE_PWM         255       // 0-255 — initial engage power
#define SOLENOID_HOLD_PWM           100       // 0-255 — holding power (reduces heat)
#define SOLENOID_ENGAGE_DELAY       50        // ms — time at full power before reducing

// Blade pressure (0-255, maps to servo angle range or solenoid PWM)
#define DEFAULT_BLADE_PRESSURE      128

// Tangential knife settings (Tool type 2)
#define TANGENTIAL_LIFT_HEIGHT      2.0f      // mm — lift before rotating
#define TANGENTIAL_SWIVEL_THRESHOLD 15.0f     // degrees — angle TRIG_CHANGE that triggers lift
#define TANGENTIAL_BLADE_OFFSET     0.5f      // mm — distance from rotation center to blade tip

// ── Stepper Driver Configuration ────────────────────────────────────────────
#define STEP_PULSE_WIDTH_US         3         // µs — minimum step pulse width
                                              // A4988: 1µs min, DRV8825: 1.9µs min
                                              // Using 3µs for safety margin
#define STEP_IDLE_DELAY_MS          25        // ms — delay after step before dir TRIG_CHANGE
#define MOTOR_IDLE_TIMEOUT_SEC      120       // seconds — disable motors after idle

// Axis direction inversion (true = inverted)
#define DEFAULT_INVERT_X            false
#define DEFAULT_INVERT_Y            false
#define DEFAULT_INVERT_Z            false
#define DEFAULT_INVERT_C            false

// Enable pin logic (true = active LOW, false = active HIGH)
#define STEPPER_ENABLE_ACTIVE_LOW   true

// ── TMC2209 UART Configuration ──────────────────────────────────────────────
#ifdef HAS_TMC_UART
    #define TMC_BAUD_RATE           115200
    #define TMC_RUN_CURRENT_MA      800       // mA — running current per motor
    #define TMC_HOLD_CURRENT_MA     400       // mA — holding current (50% of run)
    #define TMC_MICROSTEPS          16        // Microstepping: 1, 2, 4, 8, 16, 32, 64, 256
    #define TMC_STEALTHCHOP         true      // true = silent, false = spreadCycle (louder, more torque)
    #define TMC_X_ADDR              0         // UART address for X driver
    #define TMC_Y_ADDR              1         // UART address for Y driver
    #define TMC_Z_ADDR              2         // UART address for Z driver
    #define TMC_C_ADDR              3         // UART address for C driver
#endif

// ── Communication ───────────────────────────────────────────────────────────
#define SERIAL_BAUD_RATE            115200

// Serial RX buffer (Nano needs smaller buffer due to limited RAM)
#ifndef SERIAL_RX_BUFFER_SIZE
    #define SERIAL_RX_BUFFER_SIZE   256
#endif

#define GCODE_LINE_MAX_LENGTH       128       // Max characters per G-code line
#define STATUS_REPORT_INTERVAL_MS   200       // ms between auto status reports

// ESP32 WiFi settings
#ifdef HAS_WIFI
    #define WIFI_AP_SSID_PREFIX     "OpenPlotter"
    #define WIFI_AP_PASSWORD        "openplotter"   // TRIG_CHANGE this!
    #define WIFI_AP_CHANNEL         1
    #define WIFI_HOSTNAME           "openplotter"
    #define WIFI_MDNS_NAME          "openplotter"   // accessible at openplotter.local
    #define WEBSOCKET_PORT          80
    #define WEBSOCKET_PATH          "/ws"
    #define DEFAULT_WIFI_MODE       0               // 0 = AP, 1 = STA (join network)
#endif

// ESP32 Bluetooth settings
#ifdef HAS_BLUETOOTH
    #define BT_DEVICE_NAME          "OpenPlotter"
    #define BT_ENABLED_DEFAULT      false           // Disabled by default (saves power)
#endif

// ── Motion Planner ──────────────────────────────────────────────────────────
// Planner ring buffer size (number of motion blocks)
// Larger = smoother motion, more RAM. Nano needs smaller buffer.
#ifndef PLANNER_BUFFER_SIZE
    #if defined(BOARD_NANO)
        #define PLANNER_BUFFER_SIZE 12
    #elif defined(BOARD_ESP32)
        #define PLANNER_BUFFER_SIZE 32
    #else
        #define PLANNER_BUFFER_SIZE 24
    #endif
#endif

// Segment buffer size (prepared step segments consumed by ISR)
#define SEGMENT_BUFFER_SIZE         8

// Minimum feed rate (mm/min) — prevents division by zero
#define MIN_FEED_RATE               1.0f

// ── Safety ──────────────────────────────────────────────────────────────────
#define HARD_LIMITS_ENABLED         true      // Alarm on limit switch trigger during run
#define ESTOP_PIN_ACTIVE_LOW        true      // E-stop button is normally closed

// ── Debug ───────────────────────────────────────────────────────────────────
// Uncomment to enable verbose debug output on Serial
// WARNING: Debug output can interfere with G-code streaming at high speeds
// #define DEBUG_GCODE_PARSER
// #define DEBUG_MOTION_PLANNER
// #define DEBUG_STEPPER_ISR
// #define DEBUG_HOMING
// #define DEBUG_TMC_UART

#endif // OPENPLOTTER_CONFIG_H

#define OPENPLOTTER_VERSION "1.1.0"

