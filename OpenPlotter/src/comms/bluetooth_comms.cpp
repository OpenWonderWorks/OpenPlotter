/**
 * ============================================================================
 * OpenPlotter — Bluetooth Communication Implementation (ESP32)
 * ============================================================================
 */

#ifdef ESP32
#ifdef HAS_BLUETOOTH
#ifndef BRIDGE_MODE

#include "bluetooth_comms.h"
#include "../utils/logger.h"

// Hide from Arduino dependency scanner
#define BT_HDR "BluetoothSerial.h"
#include BT_HDR

static BluetoothSerial* _btSerial = nullptr;

void BluetoothComms::init() {
    #if BT_ENABLED_DEFAULT
    _btSerial = new BluetoothSerial();
    _btSerial->begin(BT_DEVICE_NAME);
    LOG_INFO("Bluetooth: Started as \"%s\"", BT_DEVICE_NAME);
    #else
    LOG_INFO("Bluetooth: Disabled (set BT_ENABLED_DEFAULT=true to enable)");
    #endif
}

char* BluetoothComms::readLine() {
    if (!_btSerial) return nullptr;

    while (_btSerial->available() > 0) {
        char c = (char)_btSerial->read();

        if (c == '\n' || c == '\r') {
            if (_rxIndex > 0) {
                _rxBuffer[_rxIndex] = '\0';
                memcpy(_lineBuffer, _rxBuffer, _rxIndex + 1);
                _rxIndex = 0;
                return _lineBuffer;
            }
            continue;
        }

        if (_rxIndex < GCODE_LINE_MAX_LENGTH) {
            _rxBuffer[_rxIndex++] = c;
        }
    }

    return nullptr;
}

void BluetoothComms::send(const char* msg) {
    if (_btSerial) {
        _btSerial->println(msg);
    }
}

bool BluetoothComms::isConnected() const {
    return _btSerial && _btSerial->hasClient();
}

#endif // !BRIDGE_MODE
#endif // HAS_BLUETOOTH
#endif // ESP32
