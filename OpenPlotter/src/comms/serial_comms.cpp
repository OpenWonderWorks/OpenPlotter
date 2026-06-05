/**
 * ============================================================================
 * OpenPlotter — Serial Communication Implementation
 * ============================================================================
 */

#include "serial_comms.h"
#include "../gcode/parser.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

SerialComms::SerialComms() {
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
    memset(_lineBuffer, 0, sizeof(_lineBuffer));
}

void SerialComms::init() {
    hal->serialInit(SERIAL_BAUD_RATE);
}

char* SerialComms::readLine() {
    _realtimeChar = 0;

    while (hal->serialAvailable() > 0) {
        char c = (char)hal->serialRead();

        // Check for real-time commands (processed immediately, any time)
        if (GCodeParser::isRealtimeCommand(c)) {
            _realtimeChar = c;
            return nullptr;
        }

        // End of line
        if (c == '\n' || c == '\r') {
            if (_rxIndex > 0) {
                _rxBuffer[_rxIndex] = '\0';
                memcpy(_lineBuffer, _rxBuffer, _rxIndex + 1);
                _rxIndex = 0;
                return _lineBuffer;
            }
            continue; // Skip empty lines
        }

        // Accumulate characters
        if (_rxIndex < GCODE_LINE_MAX_LENGTH) {
            _rxBuffer[_rxIndex++] = c;
        }
        // If buffer overflow, silently truncate (will report error on parse)
    }

    return nullptr;
}

char SerialComms::checkRealtimeCommand() {
    char c = _realtimeChar;
    _realtimeChar = 0;
    return c;
}

void SerialComms::sendOk() {
    hal->serialPrintln("ok");
}

void SerialComms::sendError(uint8_t errorCode, const char* message) {
    char buf[64];
    if (message) {
        snprintf(buf, sizeof(buf), "error:%d (%s)", errorCode, message);
    } else {
        snprintf(buf, sizeof(buf), "error:%d", errorCode);
    }
    hal->serialPrintln(buf);
}

void SerialComms::sendAlarm(uint8_t alarmCode) {
    char buf[32];
    snprintf(buf, sizeof(buf), "ALARM:%d", alarmCode);
    hal->serialPrintln(buf);
}

void SerialComms::sendMessage(const char* msg) {
    hal->serialPrint("[MSG: ");
    hal->serialPrint(msg);
    hal->serialPrintln("]");
}

void SerialComms::sendStatus(const char* status) {
    hal->serialPrint("<");
    hal->serialPrint(status);
    hal->serialPrintln(">");
}

void SerialComms::sendLine(const char* line) {
    hal->serialPrintln(line);
}

void SerialComms::sendf(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    hal->serialPrintln(buf);
}
