/**
 * ============================================================================
 * OpenPlotter — Safety System Implementation
 * ============================================================================
 */

#include "safety.h"
#include "safety_checks.h"
#include "../utils/logger.h"

Safety* Safety::_instance = nullptr;

static uint8_t enabledHardLimitMask() {
    uint8_t mask = 0;
    #ifdef X_MIN_PIN
    if (HARD_LIMIT_X_MIN_ENABLED) mask |= 0x01;
    #endif
    #ifdef X_MAX_PIN
    if (HARD_LIMIT_X_MAX_ENABLED) mask |= 0x02;
    #endif
    #ifdef Y_MIN_PIN
    if (HARD_LIMIT_Y_MIN_ENABLED) mask |= 0x04;
    #endif
    #ifdef Y_MAX_PIN
    if (HARD_LIMIT_Y_MAX_ENABLED) mask |= 0x08;
    #endif
    #ifdef Z_MIN_PIN
    if (HARD_LIMIT_Z_MIN_ENABLED) mask |= 0x10;
    #endif
    #ifdef Z_MAX_PIN
    if (HARD_LIMIT_Z_MAX_ENABLED) mask |= 0x20;
    #endif
    return mask;
}

void Safety::init(Stepper* stepper, Planner* planner, Status* status,
                  Settings* settings, HomingManager* homing, ToolController* tool) {
    _stepper = stepper;
    _planner = planner;
    _status = status;
    _settings = settings;
    _homing = homing;
    _tool = tool;
    _instance = this;

    // Configure E-stop pin
    #ifdef ESTOP_PIN
    hal->pinMode(ESTOP_PIN, PinMode::INPUT_PULLUP_MODE);

    // Check if E-stop is already active (NC button is open = stopped)
    const bool estopState = isActiveLevel(hal->digitalRead(ESTOP_PIN),
                                          ESTOP_PIN_ACTIVE_LOW);
    if (estopState) {
        LOG_WARN("Safety: E-stop is active at startup!");
        _estopActive = true;
        emergencyStop();
    }
    #endif

    // Snapshot active switches for edge detection during the blocking homing
    // routine. Normal motion checks active levels and fails closed.
    #if HARD_LIMITS_ENABLED
    _lastLimitMask = _homing
        ? (uint8_t)(_homing->readEndstops() & enabledHardLimitMask())
        : 0;
    #endif

    LOG_INFO("Safety: Initialized (soft_limits=%s hard_limits=%s)",
             SOFT_LIMITS_ENABLED ? "on" : "off",
             HARD_LIMITS_ENABLED ? "on" : "off");
}

void Safety::update() {
    // Check E-stop
    #ifdef ESTOP_PIN
    const bool estopState = isActiveLevel(hal->digitalRead(ESTOP_PIN),
                                          ESTOP_PIN_ACTIVE_LOW);

    if (estopState && (!_estopActive || (_status && !_status->isAlarmed()))) {
        emergencyStop();
    }
    _estopActive = estopState;
    #endif

    #if HARD_LIMITS_ENABLED
    const uint8_t limitMask = _homing
        ? (uint8_t)(_homing->readEndstops() & enabledHardLimitMask())
        : 0;
    if (_status && _status->getState() == MachineState::HOMING) {
        const uint8_t expectedHomeMask = _homing
            ? homeSwitchMaskForAxis(_homing->currentAxis()) : 0;
        const uint8_t newlyTriggered = (uint8_t)(limitMask & (uint8_t)~_lastLimitMask);
        _lastLimitMask = limitMask;
        // Let the expected home switch stop its own seek. A newly activated
        // opposite end or another enabled switch still aborts homing.
        if ((newlyTriggered & (uint8_t)~expectedHomeMask) != 0) {
            hardLimitTriggered();
        }
    } else {
        _lastLimitMask = limitMask;
        if (limitMask && _stepper && _stepper->getState() == StepperState::RUNNING) {
            hardLimitTriggered();
        }
    }
    #endif
}

bool Safety::checkSoftLimits(float target[4]) const {
    #if SOFT_LIMITS_ENABLED
    if (!_settings) return false;
    float maxTravel[4] = {
        _settings->maxTravel(0), _settings->maxTravel(1),
        _settings->maxTravel(2), _settings->maxTravel(3)
    };
    #if NUM_AXES >= 4
    return withinTravelLimits(target, maxTravel, 4);
    #elif NUM_AXES >= 3
    return withinTravelLimits(target, maxTravel, 3);
    #else
    return withinTravelLimits(target, maxTravel, 2);
    #endif
    #else
    (void)target;
    return true;
    #endif
}

void Safety::hardLimitTriggered() {
    if (!_stepper || !_status) return;

    LOG_ERROR("Safety: HARD LIMIT TRIGGERED — Emergency stop!");
    _stepper->emergencyStop();
    if (_tool) _tool->toolUp();
    _requiresHoming = true;
    _status->setAlarm(1); // Alarm 1: Hard limit
}

void Safety::emergencyStop() {
    if (!_stepper || !_status) return;

    LOG_ERROR("Safety: EMERGENCY STOP!");
    _stepper->emergencyStop();
    if (_tool) _tool->toolUp();
    _requiresHoming = true;
    _status->setAlarm(2); // Alarm 2: E-stop
}

void Safety::hardLimitISR() {
    if (_instance) _instance->hardLimitTriggered();
}

void Safety::estopISR() {
    if (_instance) _instance->emergencyStop();
}
