/**
 * ============================================================================
 * OpenPlotter — Settings Manager
 * ============================================================================
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "../hal/hal.h"
#include "config.h"
#include <stdint.h>

// ── Setting indices (EEPROM/NVS addresses) ─────────────────────────────────
// Each float takes 4 bytes. Booleans use 1 byte.
enum SettingIndex : uint16_t {
    SET_STEPS_PER_MM_X      = 0,    // $100
    SET_STEPS_PER_MM_Y      = 4,    // $101
    SET_STEPS_PER_MM_Z      = 8,    // $102
    SET_STEPS_PER_DEG_C     = 12,   // $103
    SET_MAX_RATE_X           = 16,   // $110
    SET_MAX_RATE_Y           = 20,   // $111
    SET_MAX_RATE_Z           = 24,   // $112
    SET_MAX_RATE_C           = 28,   // $113
    SET_ACCELERATION         = 32,   // $120
    SET_JUNCTION_DEVIATION   = 36,   // $130
    SET_MAX_TRAVEL_X         = 40,   // $140
    SET_MAX_TRAVEL_Y         = 44,   // $141
    SET_MAX_TRAVEL_Z         = 48,   // $142
    SET_HOMING_SEEK_RATE     = 52,   // $150
    SET_HOMING_FEED_RATE     = 56,   // $151
    SET_HOMING_PULLOFF       = 60,   // $152
    SET_HOMING_METHOD        = 64,   // $160 (1 byte)
    SET_SG_THRESHOLD         = 65,   // $161 (1 byte)
    SET_SERVO_UP_ANGLE       = 66,   // $170 (1 byte)
    SET_SERVO_DOWN_ANGLE     = 67,   // $171 (1 byte)
    SET_SERVO_DELAY          = 68,   // $172 (2 bytes)
    SET_BLADE_PRESSURE       = 70,   // $173 (1 byte)
    SET_TOOL_TYPE            = 71,   // $174 (1 byte)
    SET_INVERT_X             = 72,   // $180 (1 byte)
    SET_INVERT_Y             = 73,   // $181 (1 byte)
    SET_INVERT_Z             = 74,   // $182 (1 byte)
    SET_INVERT_C             = 75,   // $183 (1 byte)
    SET_WIFI_MODE            = 76,   // $190 (1 byte)
    SET_TMC_RUN_CURRENT      = 77,   // $162 (2 bytes)
    SET_TMC_HOLD_CURRENT     = 79,   // $163 (2 bytes)
    SET_TMC_MICROSTEPS       = 81,   // $164 (2 bytes)
    SET_TMC_STEALTHCHOP      = 83,   // $165 (1 byte)
    SET_DRIVER_X             = 84,   // $166 (1 byte)
    SET_DRIVER_Y             = 85,   // $167 (1 byte)
    SET_DRIVER_Z             = 86,   // $168 (1 byte)
    SET_DRIVER_C             = 87,   // $169 (1 byte)
    SET_MAGIC                = 100,  // Magic byte to detect first boot
    SETTINGS_SIZE            = 104   // Total bytes used
};

#define SETTINGS_MAGIC_VALUE  0x4F  // 'O' for OpenPlotter

class Settings {
public:
    Settings();

    void init();
    void load();
    void save();
    void resetDefaults();

    // Getters
    float stepsPerMm(uint8_t axis) const;
    float maxRate(uint8_t axis) const;
    float acceleration() const { return _acceleration; }
    float junctionDeviation() const { return _junctionDeviation; }
    float maxTravel(uint8_t axis) const;
    float homingSeekRate() const { return _homingSeekRate; }
    float homingFeedRate() const { return _homingFeedRate; }
    float homingPulloff() const { return _homingPulloff; }
    uint8_t homingMethod() const { return _homingMethod; }
    uint8_t sgThreshold() const { return _sgThreshold; }
    uint8_t servoUpAngle() const { return _servoUpAngle; }
    uint8_t servoDownAngle() const { return _servoDownAngle; }
    uint16_t servoDelay() const { return _servoDelay; }
    uint8_t bladePressure() const { return _bladePressure; }
    uint8_t toolType() const { return _toolType; }
    bool invertAxis(uint8_t axis) const;
    uint8_t wifiMode() const { return _wifiMode; }
    uint16_t tmcRunCurrent() const { return _tmcRunCurrent; }
    uint16_t tmcHoldCurrent() const { return _tmcHoldCurrent; }
    uint16_t tmcMicrosteps() const { return _tmcMicrosteps; }
    bool tmcStealthChop() const { return _tmcStealthChop; }
    uint8_t driverType(uint8_t axis) const;

    // Setters (also persist to storage)
    void setStepsPerMm(uint8_t axis, float value);
    void setMaxRate(uint8_t axis, float value);
    void setAcceleration(float value);
    void setJunctionDeviation(float value);
    void setMaxTravel(uint8_t axis, float value);
    void setHomingMethod(uint8_t method);
    void setSgThreshold(uint8_t value);
    void setServoUpAngle(uint8_t angle);
    void setServoDownAngle(uint8_t angle);
    void setServoDelay(uint16_t ms);
    void setBladePressure(uint8_t pressure);
    void setToolType(uint8_t type);
    void setInvertAxis(uint8_t axis, bool invert);
    void setWifiMode(uint8_t mode);
    void setTmcRunCurrent(uint16_t current);
    void setTmcHoldCurrent(uint16_t current);
    void setTmcMicrosteps(uint16_t microsteps);
    void setTmcStealthChop(bool enable);
    void setDriverType(uint8_t axis, uint8_t type);

    /**
     * Set a setting by its $ number (e.g., $100=80.0).
     * @return true if setting was recognized and applied.
     */
    bool setByNumber(uint16_t number, float value);

    /**
     * Print all settings to serial (for $$ command).
     */
    void printAll();

private:
    float _stepsPerMm[4];
    float _maxRate[4];
    float _acceleration;
    float _junctionDeviation;
    float _maxTravel[4];
    float _homingSeekRate;
    float _homingFeedRate;
    float _homingPulloff;
    uint8_t _homingMethod;
    uint8_t _sgThreshold;
    uint8_t _servoUpAngle;
    uint8_t _servoDownAngle;
    uint16_t _servoDelay;
    uint8_t _bladePressure;
    uint8_t _toolType;
    bool _invertAxis[4];
    uint8_t _wifiMode;
    uint16_t _tmcRunCurrent;
    uint16_t _tmcHoldCurrent;
    uint16_t _tmcMicrosteps;
    bool _tmcStealthChop;
    uint8_t _driverType[4];
};

#endif // SETTINGS_H
