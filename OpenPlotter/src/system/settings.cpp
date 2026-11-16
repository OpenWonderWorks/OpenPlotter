/**
 * ============================================================================
 * OpenPlotter — Settings Manager Implementation
 * ============================================================================
 */

#include "settings.h"
#include "../utils/logger.h"
#include "../motion/tmc_driver.h"
#include <string.h>
#include <stdio.h>

Settings::Settings() {
    resetDefaults();
}

void Settings::init() {
    hal->storageInit();

    // Check magic byte — if not present, this is first boot
    uint8_t magic = hal->storageReadByte(SET_MAGIC);
    if (magic != SETTINGS_MAGIC_VALUE) {
        LOG_INFO("Settings: First boot detected — writing defaults");
        resetDefaults();
        save();
    } else {
        load();
        LOG_INFO("Settings: Loaded from storage");
    }
}

void Settings::resetDefaults() {
    _stepsPerMm[0] = DEFAULT_STEPS_PER_MM_X;
    _stepsPerMm[1] = DEFAULT_STEPS_PER_MM_Y;
    _stepsPerMm[2] = DEFAULT_STEPS_PER_MM_Z;
    _stepsPerMm[3] = DEFAULT_STEPS_PER_DEG_C;
    _maxRate[0] = DEFAULT_MAX_RATE_X;
    _maxRate[1] = DEFAULT_MAX_RATE_Y;
    _maxRate[2] = DEFAULT_MAX_RATE_Z;
    _maxRate[3] = DEFAULT_MAX_RATE_C;
    _acceleration = DEFAULT_ACCELERATION;
    _junctionDeviation = DEFAULT_JUNCTION_DEVIATION;
    _maxTravel[0] = DEFAULT_MAX_TRAVEL_X;
    _maxTravel[1] = DEFAULT_MAX_TRAVEL_Y;
    _maxTravel[2] = DEFAULT_MAX_TRAVEL_Z;
    _maxTravel[3] = 360.0f;
    _homingSeekRate = DEFAULT_HOMING_SEEK_RATE;
    _homingFeedRate = DEFAULT_HOMING_FEED_RATE;
    _homingPulloff = DEFAULT_HOMING_PULLOFF;
    _homingMethod = DEFAULT_HOMING_METHOD;
    _sgThreshold = DEFAULT_SG_THRESHOLD;
    _servoUpAngle = DEFAULT_SERVO_UP_ANGLE;
    _servoDownAngle = DEFAULT_SERVO_DOWN_ANGLE;
    _servoDelay = DEFAULT_SERVO_DELAY;
    _bladePressure = DEFAULT_BLADE_PRESSURE;
    _toolType = DEFAULT_TOOL_TYPE;
    _invertAxis[0] = DEFAULT_INVERT_X;
    _invertAxis[1] = DEFAULT_INVERT_Y;
    _invertAxis[2] = DEFAULT_INVERT_Z;
    _invertAxis[3] = DEFAULT_INVERT_C;
    _wifiMode = 0;
    _tmcRunCurrent = DEFAULT_TMC_RUN_CURRENT_MA;
    _tmcHoldCurrent = DEFAULT_TMC_HOLD_CURRENT_MA;
    _tmcMicrosteps = DEFAULT_TMC_MICROSTEPS;
    _tmcStealthChop = DEFAULT_TMC_STEALTHCHOP;
    _driverType[0] = DEFAULT_DRIVER_X;
    _driverType[1] = DEFAULT_DRIVER_Y;
    _driverType[2] = DEFAULT_DRIVER_Z;
    _driverType[3] = DEFAULT_DRIVER_C;
}

void Settings::load() {
    _stepsPerMm[0] = hal->storageReadFloat(SET_STEPS_PER_MM_X);
    _stepsPerMm[1] = hal->storageReadFloat(SET_STEPS_PER_MM_Y);
    _stepsPerMm[2] = hal->storageReadFloat(SET_STEPS_PER_MM_Z);
    _stepsPerMm[3] = hal->storageReadFloat(SET_STEPS_PER_DEG_C);
    _maxRate[0] = hal->storageReadFloat(SET_MAX_RATE_X);
    _maxRate[1] = hal->storageReadFloat(SET_MAX_RATE_Y);
    _maxRate[2] = hal->storageReadFloat(SET_MAX_RATE_Z);
    _maxRate[3] = hal->storageReadFloat(SET_MAX_RATE_C);
    _acceleration = hal->storageReadFloat(SET_ACCELERATION);
    _junctionDeviation = hal->storageReadFloat(SET_JUNCTION_DEVIATION);
    _maxTravel[0] = hal->storageReadFloat(SET_MAX_TRAVEL_X);
    _maxTravel[1] = hal->storageReadFloat(SET_MAX_TRAVEL_Y);
    _maxTravel[2] = hal->storageReadFloat(SET_MAX_TRAVEL_Z);
    _homingSeekRate = hal->storageReadFloat(SET_HOMING_SEEK_RATE);
    _homingFeedRate = hal->storageReadFloat(SET_HOMING_FEED_RATE);
    _homingPulloff = hal->storageReadFloat(SET_HOMING_PULLOFF);
    _homingMethod = hal->storageReadByte(SET_HOMING_METHOD);
    _sgThreshold = hal->storageReadByte(SET_SG_THRESHOLD);
    _servoUpAngle = hal->storageReadByte(SET_SERVO_UP_ANGLE);
    _servoDownAngle = hal->storageReadByte(SET_SERVO_DOWN_ANGLE);
    _servoDelay = (uint16_t)hal->storageReadByte(SET_SERVO_DELAY) |
                  ((uint16_t)hal->storageReadByte(SET_SERVO_DELAY + 1) << 8);
    _bladePressure = hal->storageReadByte(SET_BLADE_PRESSURE);
    _toolType = hal->storageReadByte(SET_TOOL_TYPE);
    _invertAxis[0] = hal->storageReadByte(SET_INVERT_X);
    _invertAxis[1] = hal->storageReadByte(SET_INVERT_Y);
    _invertAxis[2] = hal->storageReadByte(SET_INVERT_Z);
    _invertAxis[3] = hal->storageReadByte(SET_INVERT_C);
    _wifiMode = hal->storageReadByte(SET_WIFI_MODE);
    
    // Load TMC settings
    _tmcRunCurrent = (uint16_t)hal->storageReadByte(SET_TMC_RUN_CURRENT) |
                     ((uint16_t)hal->storageReadByte(SET_TMC_RUN_CURRENT + 1) << 8);
    _tmcHoldCurrent = (uint16_t)hal->storageReadByte(SET_TMC_HOLD_CURRENT) |
                      ((uint16_t)hal->storageReadByte(SET_TMC_HOLD_CURRENT + 1) << 8);
    _tmcMicrosteps = (uint16_t)hal->storageReadByte(SET_TMC_MICROSTEPS) |
                     ((uint16_t)hal->storageReadByte(SET_TMC_MICROSTEPS + 1) << 8);
    _tmcStealthChop = hal->storageReadByte(SET_TMC_STEALTHCHOP) > 0;

    _driverType[0] = hal->storageReadByte(SET_DRIVER_X);
    _driverType[1] = hal->storageReadByte(SET_DRIVER_Y);
    _driverType[2] = hal->storageReadByte(SET_DRIVER_Z);
    _driverType[3] = hal->storageReadByte(SET_DRIVER_C);

    // EEPROM/NVS contents can be stale or corrupted. Never apply invalid
    // kinematics, motion, travel, or actuator values after a reboot.
    for (uint8_t axis = 0; axis < 4; ++axis) {
        const uint16_t stepsSetting = (uint16_t)(100 + axis);
        const uint16_t rateSetting = (uint16_t)(110 + axis);
        const uint16_t travelSetting = (uint16_t)(140 + axis);
        if (!isValidSettingValue(stepsSetting, _stepsPerMm[axis])) {
            const float defaults[4] = {DEFAULT_STEPS_PER_MM_X, DEFAULT_STEPS_PER_MM_Y,
                                       DEFAULT_STEPS_PER_MM_Z, DEFAULT_STEPS_PER_DEG_C};
            _stepsPerMm[axis] = defaults[axis];
        }
        if (!isValidSettingValue(rateSetting, _maxRate[axis])) {
            const float defaults[4] = {DEFAULT_MAX_RATE_X, DEFAULT_MAX_RATE_Y,
                                       DEFAULT_MAX_RATE_Z, DEFAULT_MAX_RATE_C};
            _maxRate[axis] = defaults[axis];
        }
        if (axis < 3 && !isValidSettingValue(travelSetting, _maxTravel[axis])) {
            const float defaults[3] = {DEFAULT_MAX_TRAVEL_X, DEFAULT_MAX_TRAVEL_Y,
                                       DEFAULT_MAX_TRAVEL_Z};
            _maxTravel[axis] = defaults[axis];
        }
        if (!isValidSettingValue((uint16_t)(166 + axis), _driverType[axis])) {
            const uint8_t defaults[4] = {DEFAULT_DRIVER_X, DEFAULT_DRIVER_Y,
                                         DEFAULT_DRIVER_Z, DEFAULT_DRIVER_C};
            _driverType[axis] = defaults[axis];
        }
    }
    if (!isValidSettingValue(120, _acceleration)) _acceleration = DEFAULT_ACCELERATION;
    if (!isValidSettingValue(130, _junctionDeviation)) _junctionDeviation = DEFAULT_JUNCTION_DEVIATION;
    if (!isValidSettingValue(150, _homingSeekRate)) _homingSeekRate = DEFAULT_HOMING_SEEK_RATE;
    if (!isValidSettingValue(151, _homingFeedRate)) _homingFeedRate = DEFAULT_HOMING_FEED_RATE;
    if (!isValidSettingValue(152, _homingPulloff)) _homingPulloff = DEFAULT_HOMING_PULLOFF;
    if (!isValidSettingValue(160, _homingMethod)) _homingMethod = DEFAULT_HOMING_METHOD;
    if (!isValidSettingValue(161, _sgThreshold)) _sgThreshold = DEFAULT_SG_THRESHOLD;
    if (!isValidSettingValue(170, _servoUpAngle)) _servoUpAngle = DEFAULT_SERVO_UP_ANGLE;
    if (!isValidSettingValue(171, _servoDownAngle)) _servoDownAngle = DEFAULT_SERVO_DOWN_ANGLE;
    if (!isValidSettingValue(172, _servoDelay)) _servoDelay = DEFAULT_SERVO_DELAY;
    if (!isValidSettingValue(173, _bladePressure)) _bladePressure = DEFAULT_BLADE_PRESSURE;
    if (!isValidSettingValue(174, _toolType)) _toolType = DEFAULT_TOOL_TYPE;
    if (!isValidSettingValue(190, _wifiMode)) _wifiMode = 0;
    if (!isValidSettingValue(162, _tmcRunCurrent)) _tmcRunCurrent = DEFAULT_TMC_RUN_CURRENT_MA;
    if (!isValidSettingValue(163, _tmcHoldCurrent)) _tmcHoldCurrent = DEFAULT_TMC_HOLD_CURRENT_MA;
    if (!isValidSettingValue(164, _tmcMicrosteps)) _tmcMicrosteps = DEFAULT_TMC_MICROSTEPS;
    if (!isValidSettingValue(165, _tmcStealthChop ? 1.0f : 0.0f)) _tmcStealthChop = DEFAULT_TMC_STEALTHCHOP;
    for (uint8_t axis = 0; axis < 4; ++axis) {
        if (!isValidSettingValue((uint16_t)(180 + axis), _invertAxis[axis] ? 1.0f : 0.0f)) {
            const bool defaults[4] = {DEFAULT_INVERT_X, DEFAULT_INVERT_Y,
                                      DEFAULT_INVERT_Z, DEFAULT_INVERT_C};
            _invertAxis[axis] = defaults[axis];
        }
    }
}

void Settings::save() {
    hal->storageWriteFloat(SET_STEPS_PER_MM_X, _stepsPerMm[0]);
    hal->storageWriteFloat(SET_STEPS_PER_MM_Y, _stepsPerMm[1]);
    hal->storageWriteFloat(SET_STEPS_PER_MM_Z, _stepsPerMm[2]);
    hal->storageWriteFloat(SET_STEPS_PER_DEG_C, _stepsPerMm[3]);
    hal->storageWriteFloat(SET_MAX_RATE_X, _maxRate[0]);
    hal->storageWriteFloat(SET_MAX_RATE_Y, _maxRate[1]);
    hal->storageWriteFloat(SET_MAX_RATE_Z, _maxRate[2]);
    hal->storageWriteFloat(SET_MAX_RATE_C, _maxRate[3]);
    hal->storageWriteFloat(SET_ACCELERATION, _acceleration);
    hal->storageWriteFloat(SET_JUNCTION_DEVIATION, _junctionDeviation);
    hal->storageWriteFloat(SET_MAX_TRAVEL_X, _maxTravel[0]);
    hal->storageWriteFloat(SET_MAX_TRAVEL_Y, _maxTravel[1]);
    hal->storageWriteFloat(SET_MAX_TRAVEL_Z, _maxTravel[2]);
    hal->storageWriteFloat(SET_HOMING_SEEK_RATE, _homingSeekRate);
    hal->storageWriteFloat(SET_HOMING_FEED_RATE, _homingFeedRate);
    hal->storageWriteFloat(SET_HOMING_PULLOFF, _homingPulloff);
    hal->storageWriteByte(SET_HOMING_METHOD, _homingMethod);
    hal->storageWriteByte(SET_SG_THRESHOLD, _sgThreshold);
    hal->storageWriteByte(SET_SERVO_UP_ANGLE, _servoUpAngle);
    hal->storageWriteByte(SET_SERVO_DOWN_ANGLE, _servoDownAngle);
    hal->storageWriteByte(SET_SERVO_DELAY, (uint8_t)(_servoDelay & 0xFF));
    hal->storageWriteByte(SET_SERVO_DELAY + 1, (uint8_t)((_servoDelay >> 8) & 0xFF));
    hal->storageWriteByte(SET_BLADE_PRESSURE, _bladePressure);
    hal->storageWriteByte(SET_TOOL_TYPE, _toolType);
    hal->storageWriteByte(SET_INVERT_X, _invertAxis[0]);
    hal->storageWriteByte(SET_INVERT_Y, _invertAxis[1]);
    hal->storageWriteByte(SET_INVERT_Z, _invertAxis[2]);
    hal->storageWriteByte(SET_INVERT_C, _invertAxis[3]);
    hal->storageWriteByte(SET_WIFI_MODE, _wifiMode);
    
    // Save TMC settings
    hal->storageWriteByte(SET_TMC_RUN_CURRENT, (uint8_t)(_tmcRunCurrent & 0xFF));
    hal->storageWriteByte(SET_TMC_RUN_CURRENT + 1, (uint8_t)((_tmcRunCurrent >> 8) & 0xFF));
    hal->storageWriteByte(SET_TMC_HOLD_CURRENT, (uint8_t)(_tmcHoldCurrent & 0xFF));
    hal->storageWriteByte(SET_TMC_HOLD_CURRENT + 1, (uint8_t)((_tmcHoldCurrent >> 8) & 0xFF));
    hal->storageWriteByte(SET_TMC_MICROSTEPS, (uint8_t)(_tmcMicrosteps & 0xFF));
    hal->storageWriteByte(SET_TMC_MICROSTEPS + 1, (uint8_t)((_tmcMicrosteps >> 8) & 0xFF));
    hal->storageWriteByte(SET_TMC_STEALTHCHOP, _tmcStealthChop ? 1 : 0);
    
    hal->storageWriteByte(SET_DRIVER_X, _driverType[0]);
    hal->storageWriteByte(SET_DRIVER_Y, _driverType[1]);
    hal->storageWriteByte(SET_DRIVER_Z, _driverType[2]);
    hal->storageWriteByte(SET_DRIVER_C, _driverType[3]);
    
    hal->storageWriteByte(SET_MAGIC, SETTINGS_MAGIC_VALUE);
    hal->storageCommit();

    LOG_INFO("Settings: Saved to storage");
}

// Getters
float Settings::stepsPerMm(uint8_t axis) const {
    return (axis < 4) ? _stepsPerMm[axis] : 0.0f;
}

float Settings::maxRate(uint8_t axis) const {
    return (axis < 4) ? _maxRate[axis] : 0.0f;
}

float Settings::maxTravel(uint8_t axis) const {
    return (axis < 4) ? _maxTravel[axis] : 0.0f;
}

bool Settings::invertAxis(uint8_t axis) const {
    return (axis < 4) ? _invertAxis[axis] : false;
}

uint8_t Settings::driverType(uint8_t axis) const {
    return (axis < 4) ? _driverType[axis] : 0;
}

// Setters
void Settings::setStepsPerMm(uint8_t axis, float value) {
    if (axis < 4) _stepsPerMm[axis] = value;
}

void Settings::setMaxRate(uint8_t axis, float value) {
    if (axis < 4) _maxRate[axis] = value;
}

void Settings::setAcceleration(float value) { _acceleration = value; }
void Settings::setJunctionDeviation(float value) { _junctionDeviation = value; }

void Settings::setMaxTravel(uint8_t axis, float value) {
    if (axis < 4) _maxTravel[axis] = value;
}

void Settings::setHomingMethod(uint8_t method) { _homingMethod = method; }
void Settings::setSgThreshold(uint8_t value) { _sgThreshold = value; }
void Settings::setServoUpAngle(uint8_t angle) { _servoUpAngle = angle; }
void Settings::setServoDownAngle(uint8_t angle) { _servoDownAngle = angle; }
void Settings::setServoDelay(uint16_t ms) { _servoDelay = ms; }
void Settings::setBladePressure(uint8_t pressure) { _bladePressure = pressure; }
void Settings::setToolType(uint8_t type) { _toolType = type; }

void Settings::setInvertAxis(uint8_t axis, bool invert) {
    if (axis < 4) _invertAxis[axis] = invert;
}

void Settings::setWifiMode(uint8_t mode) { _wifiMode = mode; }

void Settings::setTmcRunCurrent(uint16_t current) { 
    _tmcRunCurrent = current; 
    updateTmcCurrents(_tmcRunCurrent, _tmcHoldCurrent);
}

void Settings::setTmcHoldCurrent(uint16_t current) { 
    _tmcHoldCurrent = current; 
    updateTmcCurrents(_tmcRunCurrent, _tmcHoldCurrent);
}

void Settings::setTmcMicrosteps(uint16_t microsteps) { 
    _tmcMicrosteps = microsteps; 
    updateTmcMicrosteps(_tmcMicrosteps);
}

void Settings::setTmcStealthChop(bool enable) { 
    _tmcStealthChop = enable; 
    updateTmcStealthChop(_tmcStealthChop);
}

void Settings::setDriverType(uint8_t axis, uint8_t type) {
    if (axis < 4) _driverType[axis] = type;
}

// Set by $ number
bool Settings::setByNumber(uint16_t number, float value) {
    if (!isValidSettingValue(number, value)) return false;

    switch (number) {
        case 100: setStepsPerMm(0, value); return true;
        case 101: setStepsPerMm(1, value); return true;
        case 102: setStepsPerMm(2, value); return true;
        case 103: setStepsPerMm(3, value); return true;
        case 110: setMaxRate(0, value); return true;
        case 111: setMaxRate(1, value); return true;
        case 112: setMaxRate(2, value); return true;
        case 113: setMaxRate(3, value); return true;
        case 120: setAcceleration(value); return true;
        case 130: setJunctionDeviation(value); return true;
        case 140: setMaxTravel(0, value); return true;
        case 141: setMaxTravel(1, value); return true;
        case 142: setMaxTravel(2, value); return true;
        case 150: _homingSeekRate = value; return true;
        case 151: _homingFeedRate = value; return true;
        case 152: _homingPulloff = value; return true;
        case 160: setHomingMethod((uint8_t)value); return true;
        case 161: setSgThreshold((uint8_t)value); return true;
        case 170: setServoUpAngle((uint8_t)value); return true;
        case 171: setServoDownAngle((uint8_t)value); return true;
        case 172: setServoDelay((uint16_t)value); return true;
        case 173: setBladePressure((uint8_t)value); return true;
        case 174: setToolType((uint8_t)value); return true;
        case 180: setInvertAxis(0, value > 0); return true;
        case 181: setInvertAxis(1, value > 0); return true;
        case 182: setInvertAxis(2, value > 0); return true;
        case 183: setInvertAxis(3, value > 0); return true;
        case 190: setWifiMode((uint8_t)value); return true;
        case 162: setTmcRunCurrent((uint16_t)value); return true;
        case 163: setTmcHoldCurrent((uint16_t)value); return true;
        case 164: setTmcMicrosteps((uint16_t)value); return true;
        case 165: setTmcStealthChop(value > 0); return true;
        case 166: setDriverType(0, (uint8_t)value); return true;
        case 167: setDriverType(1, (uint8_t)value); return true;
        case 168: setDriverType(2, (uint8_t)value); return true;
        case 169: setDriverType(3, (uint8_t)value); return true;
        default: return false;
    }
}

// Print all settings
void Settings::printAll(void (*lineSink)(const char*)) {
    char buf[64];
    #define OUTPUT_SETTING_LINE() do { \
        if (lineSink) lineSink(buf); \
        else hal->serialPrintln(buf); \
    } while (0)
    snprintf(buf, sizeof(buf), "$100=%.3f (steps/mm X)", _stepsPerMm[0]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$101=%.3f (steps/mm Y)", _stepsPerMm[1]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$102=%.3f (steps/mm Z)", _stepsPerMm[2]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$103=%.3f (steps/deg C)", _stepsPerMm[3]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$110=%.0f (max rate X mm/min)", _maxRate[0]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$111=%.0f (max rate Y mm/min)", _maxRate[1]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$112=%.0f (max rate Z mm/min)", _maxRate[2]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$113=%.0f (max rate C deg/min)", _maxRate[3]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$120=%.0f (acceleration mm/s^2)", _acceleration); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$130=%.4f (junction deviation mm)", _junctionDeviation); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$140=%.0f (max travel X mm)", _maxTravel[0]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$141=%.0f (max travel Y mm)", _maxTravel[1]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$142=%.0f (max travel Z mm)", _maxTravel[2]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$150=%.0f (homing seek rate mm/min)", _homingSeekRate); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$151=%.0f (homing feed rate mm/min)", _homingFeedRate); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$152=%.1f (homing pull-off mm)", _homingPulloff); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$160=%d (homing method: 0=sensored 1=sensorless)", _homingMethod); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$161=%d (StallGuard threshold 0-255)", _sgThreshold); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$170=%d (servo up angle deg)", _servoUpAngle); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$171=%d (servo down angle deg)", _servoDownAngle); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$172=%d (servo delay ms)", _servoDelay); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$173=%d (blade pressure 0-255)", _bladePressure); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$174=%d (tool: 0=servo 1=solenoid 2=tangential)", _toolType); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$180=%d (invert X)", _invertAxis[0]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$181=%d (invert Y)", _invertAxis[1]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$182=%d (invert Z)", _invertAxis[2]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$183=%d (invert C)", _invertAxis[3]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$190=%d (WiFi mode: 0=AP 1=STA)", _wifiMode); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$162=%d (TMC run current mA)", _tmcRunCurrent); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$163=%d (TMC hold current mA)", _tmcHoldCurrent); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$164=%d (TMC microsteps)", _tmcMicrosteps); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$165=%d (TMC StealthChop: 0=spreadCycle 1=stealthChop)", _tmcStealthChop ? 1 : 0); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$166=%d (Driver X: 0=A4988 1=DRV8825 2=TMC2208 3=TMC2209 4=TMC2130 5=TMC5160)", _driverType[0]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$167=%d (Driver Y)", _driverType[1]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$168=%d (Driver Z)", _driverType[2]); OUTPUT_SETTING_LINE();
    snprintf(buf, sizeof(buf), "$169=%d (Driver C)", _driverType[3]); OUTPUT_SETTING_LINE();
    #undef OUTPUT_SETTING_LINE
}
