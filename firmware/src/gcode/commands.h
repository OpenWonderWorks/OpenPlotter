/**
 * ============================================================================
 * OpenPlotter — G-code Command Definitions
 * ============================================================================
 *
 * Defines all supported G-code and M-code commands, modal groups,
 * and system commands for the cutting plotter firmware.
 *
 * ============================================================================
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

// ── G-code Command Types ────────────────────────────────────────────────────
enum class GCode : uint8_t {
    G0   = 0,     // Rapid move (pen up travel)
    G1   = 1,     // Linear move (cutting/drawing)
    G2   = 2,     // Arc clockwise
    G3   = 3,     // Arc counter-clockwise
    G4   = 4,     // Dwell (pause)
    G10  = 10,    // Set coordinate offset
    G20  = 20,    // Set units to inches
    G21  = 21,    // Set units to mm
    G28  = 28,    // Home axes
    G90  = 90,    // Absolute positioning
    G91  = 91,    // Relative (incremental) positioning
    G92  = 92,    // Set position (override coordinates)
    NONE = 255    // No G command
};

// ── M-code Command Types ────────────────────────────────────────────────────
enum class MCode : uint8_t {
    M0   = 0,     // Program pause (wait for resume)
    M1   = 1,     // Optional pause
    M2   = 2,     // Program end
    M3   = 3,     // Tool ON / pen DOWN (S parameter = pressure 0-255)
    M5   = 5,     // Tool OFF / pen UP
    M17  = 17,    // Enable stepper motors
    M18  = 18,    // Disable stepper motors
    M30  = 30,    // Program end and reset
    M100 = 100,   // Print help / supported commands
    M106 = 106,   // Fan/Solenoid ON (S parameter = PWM 0-255)
    M107 = 107,   // Fan/Solenoid OFF
    M114 = 114,   // Report current position
    M119 = 119,   // Report endstop/limit switch status
    M120 = 120,   // Enable sensorless homing
    M121 = 121,   // Disable sensorless homing
    M200 = 200,   // Set blade pressure (P parameter = 0-255)
    M201 = 201,   // Set blade offset (tangential knife)
    M500 = 500,   // Save settings to EEPROM/NVS
    M501 = 501,   // Load settings from EEPROM/NVS
    M502 = 502,   // Reset to factory defaults
    NONE = 255    // No M command
};

// ── System Commands ($) ─────────────────────────────────────────────────────
enum class SystemCmd : uint8_t {
    REPORT_SETTINGS,      // $$     — Print all settings
    REPORT_OFFSETS,        // $#     — Print coordinate offsets
    REPORT_INFO,           // $I     — Print firmware info
    REPORT_STARTUP,        // $N     — Print startup blocks
    RUN_HOMING,            // $H     — Run homing cycle
    KILL_ALARM,            // $X     — Kill alarm lock
    FACTORY_RESET,         // $RST=* — Reset all settings to defaults
    SET_SETTING,           // $N=V   — Set setting N to value V
    WIFI_STATUS,           // $WIFI  — Print WiFi status
    SET_WIFI_SSID,         // $SSID=xxx
    SET_WIFI_PASS,         // $PASS=xxx
    NONE
};

// ── Real-Time Commands (single character, no newline needed) ────────────────
// These are processed immediately, bypassing the G-code line buffer.
enum class RealtimeCmd : uint8_t {
    STATUS_QUERY  = '?',   // Request current position/state
    FEED_HOLD     = '!',   // Pause motion (decelerate to stop)
    CYCLE_RESUME  = '~',   // Resume after feed hold
    SOFT_RESET    = 0x18,  // Ctrl+X — soft reset
    NONE          = 0
};

// ── Modal Groups ────────────────────────────────────────────────────────────
// G-codes belong to modal groups. Only one command from each group
// can be active at a time. Setting a new command in a group replaces
// the previous one.
enum class ModalGroup : uint8_t {
    MOTION,         // G0, G1, G2, G3 — movement type
    PLANE_SELECT,   // G17, G18, G19 — work plane (XY default)
    DISTANCE,       // G90, G91 — absolute/relative
    UNITS,          // G20, G21 — inches/mm
    COORD_SYSTEM,   // G54-G59 — work coordinate system
    TOOL,           // M3, M5 — tool on/off
    SPINDLE,        // Not used for plotter (placeholder)
    COOLANT,        // M106, M107 — fan/solenoid on/off
    STOPPING,       // M0, M1, M2, M30 — program control
    NON_MODAL       // G4, G10, G28, G92 — execute once, don't persist
};

// ── Parsed G-code Block ─────────────────────────────────────────────────────
// Represents one fully parsed line of G-code with all parameters extracted.
struct GCodeBlock {
    GCode gcode = GCode::NONE;
    MCode mcode = MCode::NONE;
    SystemCmd systemCmd = SystemCmd::NONE;

    // Axis values (NaN means "not specified in this line")
    float x = __builtin_nanf("");
    float y = __builtin_nanf("");
    float z = __builtin_nanf("");
    float c = __builtin_nanf("");    // C-axis (tangential knife angle)

    // Arc parameters
    float i = 0.0f;    // Arc center X offset (relative to start)
    float j = 0.0f;    // Arc center Y offset (relative to start)
    float r = 0.0f;    // Arc radius (alternative to I/J)

    // Feed rate and other parameters
    float f = __builtin_nanf("");    // Feed rate (mm/min or in/min)
    float s = __builtin_nanf("");    // Spindle/tool parameter (pressure, PWM)
    float p = __builtin_nanf("");    // Dwell time (ms) or parameter value
    float l = __builtin_nanf("");    // Loop count or sub-command

    // Flags
    bool hasX = false;
    bool hasY = false;
    bool hasZ = false;
    bool hasC = false;
    bool hasI = false;
    bool hasJ = false;
    bool hasR = false;
    bool hasF = false;
    bool hasS = false;
    bool hasP = false;
    bool hasL = false;

    // Line number (N word)
    int32_t lineNumber = -1;
    bool hasLineNumber = false;

    // System command value (for $N=V style commands)
    char systemValue[64] = {0};

    // Reset all fields
    void reset() {
        gcode = GCode::NONE;
        mcode = MCode::NONE;
        systemCmd = SystemCmd::NONE;
        x = y = z = c = __builtin_nanf("");
        i = j = r = 0.0f;
        f = s = p = l = __builtin_nanf("");
        hasX = hasY = hasZ = hasC = false;
        hasI = hasJ = hasR = false;
        hasF = hasS = hasP = hasL = false;
        lineNumber = -1;
        hasLineNumber = false;
        memset(systemValue, 0, sizeof(systemValue));
    }
};

// ── Machine State ───────────────────────────────────────────────────────────
// Tracks the current modal state of the machine (persists between lines).
struct ModalState {
    GCode motionMode = GCode::G0;       // Current motion mode (G0/G1/G2/G3)
    bool absoluteMode = true;           // G90 = true, G91 = false
    bool inchMode = false;              // G20 = true, G21 = false (mm default)
    bool toolOn = false;                // M3 = true, M5 = false
    float feedRate = 1000.0f;           // Current feed rate (mm/min)
    float toolPressure = 128.0f;        // Current tool pressure (0-255)
};

// ── Error Codes ─────────────────────────────────────────────────────────────
enum class GCodeError : uint8_t {
    OK = 0,
    EXPECTED_COMMAND_LETTER,       // 1
    BAD_NUMBER_FORMAT,             // 2
    INVALID_GCODE,                 // 3
    INVALID_MCODE,                 // 4
    NEGATIVE_VALUE_NOT_ALLOWED,    // 5
    FEED_RATE_NOT_SET,             // 6
    COMMAND_REQUIRES_POSITION,     // 7
    ARC_RADIUS_ERROR,              // 8
    MODAL_GROUP_VIOLATION,         // 9
    SOFT_LIMIT_EXCEEDED,           // 10
    LINE_TOO_LONG,                 // 11
    SYSTEM_CMD_ERROR,              // 12
    UNSUPPORTED_COMMAND,           // 13
    HOMING_REQUIRED,               // 14
    ALARM_LOCK                     // 15
};

// Convert error code to human-readable string
inline const char* gcodeErrorString(GCodeError err) {
    switch (err) {
        case GCodeError::OK:                         return "ok";
        case GCodeError::EXPECTED_COMMAND_LETTER:     return "Expected command letter";
        case GCodeError::BAD_NUMBER_FORMAT:           return "Bad number format";
        case GCodeError::INVALID_GCODE:               return "Invalid G-code";
        case GCodeError::INVALID_MCODE:               return "Invalid M-code";
        case GCodeError::NEGATIVE_VALUE_NOT_ALLOWED:  return "Negative value not allowed";
        case GCodeError::FEED_RATE_NOT_SET:           return "Feed rate not set";
        case GCodeError::COMMAND_REQUIRES_POSITION:   return "Command requires position";
        case GCodeError::ARC_RADIUS_ERROR:            return "Arc radius error";
        case GCodeError::MODAL_GROUP_VIOLATION:        return "Modal group violation";
        case GCodeError::SOFT_LIMIT_EXCEEDED:         return "Soft limit exceeded";
        case GCodeError::LINE_TOO_LONG:               return "Line too long";
        case GCodeError::SYSTEM_CMD_ERROR:            return "System command error";
        case GCodeError::UNSUPPORTED_COMMAND:         return "Unsupported command";
        case GCodeError::HOMING_REQUIRED:             return "Homing required";
        case GCodeError::ALARM_LOCK:                  return "Alarm lock";
        default:                                      return "Unknown error";
    }
}

#endif // COMMANDS_H
