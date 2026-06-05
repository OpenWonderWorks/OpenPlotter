/**
 * ============================================================================
 * OpenPlotter — Serial Communication
 * ============================================================================
 */

#ifndef SERIAL_COMMS_H
#define SERIAL_COMMS_H

#include "../hal/hal.h"
#include "config.h"
#include <stdint.h>

class SerialComms {
public:
    SerialComms();
    void init();

    /**
     * Check for incoming data and process complete lines.
     * Call from main loop.
     * @return Pointer to a complete line if available, nullptr otherwise.
     *         The returned buffer is only valid until the next call.
     */
    char* readLine();

    /**
     * Check for real-time commands (single-char, no newline).
     * @return The real-time command character, or 0 if none.
     */
    char checkRealtimeCommand();

    /**
     * Send response strings.
     */
    void sendOk();
    void sendError(uint8_t errorCode, const char* message = nullptr);
    void sendAlarm(uint8_t alarmCode);
    void sendMessage(const char* msg);
    void sendStatus(const char* status);
    void sendLine(const char* line);

    /**
     * Send formatted response.
     */
    void sendf(const char* fmt, ...);

private:
    char _rxBuffer[GCODE_LINE_MAX_LENGTH + 1];
    uint8_t _rxIndex = 0;
    char _lineBuffer[GCODE_LINE_MAX_LENGTH + 1];
    bool _lineReady = false;
    char _realtimeChar = 0;
};

#endif // SERIAL_COMMS_H
