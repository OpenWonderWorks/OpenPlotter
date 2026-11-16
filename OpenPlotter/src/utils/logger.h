/**
 * ============================================================================
 * OpenPlotter — Debug Logger
 * ============================================================================
 *
 * Conditional logging macros. Only compile in when DEBUG_* flags are set
 * in config.h. In release builds, these compile to nothing (zero overhead).
 *
 * Usage:
 *   LOG_INFO("Homing complete X=%.2f Y=%.2f", posX, posY);
 *   LOG_ERROR("G-code parse error at line %d", lineNum);
 *   LOG_DEBUG_GCODE("Parsed G%d X%.2f Y%.2f", code, x, y);
 *
 * ============================================================================
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "../hal/hal.h"
#include <stdio.h>
#include <stdarg.h>

// Forward declaration of global HAL
extern HAL* hal;

// ── Build-time log level ─────────────────────────────────────────────────────
// 0 = silent, 1 = errors, 2 = warnings/errors, 3 = info plus warnings/errors.
// Error/warning defaults keep serial output quieter while G-code is streamed.
#ifndef OPENPLOTTER_LOG_LEVEL
    #define OPENPLOTTER_LOG_LEVEL 2
#endif

// ── Logging Macros ──────────────────────────────────────────────────────────

// Buffer for formatted log messages
#ifndef LOG_BUFFER_SIZE
    #if defined(BOARD_NANO)
        #define LOG_BUFFER_SIZE 64
    #else
        #define LOG_BUFFER_SIZE 128
    #endif
#endif

// Core log function — prints to serial if level is enabled
inline void _logPrint(const char* prefix, const char* fmt, ...) {
    if (!hal) return;
    char buf[LOG_BUFFER_SIZE];

    // Print prefix
    hal->serialPrint(prefix);

    // Format and print message
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    hal->serialPrintln(buf);
}

#if OPENPLOTTER_LOG_LEVEL >= 1
    #define LOG_ERROR(fmt, ...) _logPrint("[ERR] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if OPENPLOTTER_LOG_LEVEL >= 2
    #define LOG_WARN(fmt, ...) _logPrint("[WRN] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...) ((void)0)
#endif

#if OPENPLOTTER_LOG_LEVEL >= 3
    #define LOG_INFO(fmt, ...) _logPrint("[INF] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...) ((void)0)
#endif

// Conditional debug macros — compile to nothing when not enabled
#ifdef DEBUG_GCODE_PARSER
    #define LOG_DEBUG_GCODE(fmt, ...) _logPrint("[GCD] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG_GCODE(fmt, ...) ((void)0)
#endif

#ifdef DEBUG_MOTION_PLANNER
    #define LOG_DEBUG_PLANNER(fmt, ...) _logPrint("[PLN] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG_PLANNER(fmt, ...) ((void)0)
#endif

#ifdef DEBUG_STEPPER_ISR
    #define LOG_DEBUG_STEPPER(fmt, ...) _logPrint("[STP] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG_STEPPER(fmt, ...) ((void)0)
#endif

#ifdef DEBUG_HOMING
    #define LOG_DEBUG_HOMING(fmt, ...) _logPrint("[HOM] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG_HOMING(fmt, ...) ((void)0)
#endif

#ifdef DEBUG_TMC_UART
    #define LOG_DEBUG_TMC(fmt, ...) _logPrint("[TMC] ", fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG_TMC(fmt, ...) ((void)0)
#endif

#endif // LOGGER_H
