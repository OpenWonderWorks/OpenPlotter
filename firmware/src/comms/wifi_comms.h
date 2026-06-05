/**
 * ============================================================================
 * OpenPlotter — WiFi Communication (ESP32)
 * ============================================================================
 */

#ifndef WIFI_COMMS_H
#define WIFI_COMMS_H

#ifdef HAS_WIFI

#include "../hal/hal.h"
#include "config.h"
#include <stdint.h>

class WifiComms {
public:
    WifiComms();
    void init();
    void update();  // Call from main loop

    /**
     * Get the next G-code line received via WebSocket.
     * @return Line string or nullptr.
     */
    char* readLine();

    /**
     * Send text to all connected WebSocket clients.
     */
    void broadcast(const char* msg);

    /**
     * Send status update to all clients (JSON format).
     */
    void sendStatus(const char* state, float x, float y, float z,
                    float feedRate, bool toolOn);

    /**
     * Get WiFi connection status.
     */
    bool isConnected() const;
    const char* getIP() const;
    const char* getSSID() const;

    /**
     * Set WiFi credentials for STA mode.
     */
    void setCredentials(const char* ssid, const char* password);

    /**
     * Switch between AP and STA mode.
     */
    void setMode(uint8_t mode); // 0=AP, 1=STA

private:
    char _lineBuffer[GCODE_LINE_MAX_LENGTH + 1];
    bool _lineReady = false;
    char _ssid[33] = {0};
    char _password[65] = {0};
    uint8_t _mode = DEFAULT_WIFI_MODE;
};

#endif // HAS_WIFI
#endif // WIFI_COMMS_H
