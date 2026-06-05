/**
 * ============================================================================
 * OpenPlotter — Bluetooth Communication (ESP32)
 * ============================================================================
 */

#ifndef BLUETOOTH_COMMS_H
#define BLUETOOTH_COMMS_H

#ifdef HAS_BLUETOOTH

#include "../hal/hal.h"
#include "config.h"

class BluetoothComms {
public:
    void init();
    char* readLine();
    void send(const char* msg);
    bool isConnected() const;

private:
    char _rxBuffer[GCODE_LINE_MAX_LENGTH + 1];
    char _lineBuffer[GCODE_LINE_MAX_LENGTH + 1];
    uint8_t _rxIndex = 0;
};

#endif // HAS_BLUETOOTH
#endif // BLUETOOTH_COMMS_H
