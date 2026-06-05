/**
 * ============================================================================
 * OpenPlotter — Safety System Implementation
 * ============================================================================
 */

#include "safety.h"
#include "../utils/logger.h"

Safety* Safety::_instance = nullptr;

void Safety::init(Stepper* stepper, Planner* planner, Status* status) {
    _stepper = stepper;
    _planner = planner;
    _status = status;
    _instance = this;

    // Configure E-stop pin
    #ifdef ESTOP_PIN
    hal->pinMode(ESTOP_PIN, PinMode::INPUT_PULLUP_MODE);

    // Check if E-stop is already active (NC button is open = stopped)
    bool estopState = hal->digitalRead(ESTOP_PIN);
    if (ESTOP_PIN_ACTIVE_LOW) estopState = !estopState;
    if (estopState) {
        LOG_WARN("Safety: E-stop is active at startup!");
        _estopActive = true;
    }
    #endif

    // Set up hard limit interrupts if enabled
    #if HARD_LIMITS_ENABLED
    // NOTE: Hard limit interrupts are only active during RUN state.
    // During homing, they are temporarily disabled.
    #endif

    LOG_INFO("Safety: Initialized (soft_limits=%s hard_limits=%s)",
             SOFT_LIMITS_ENABLED ? "on" : "off",
             HARD_LIMITS_ENABLED ? "on" : "off");
}

void Safety::update() {
    // Check E-stop
    #ifdef ESTOP_PIN
    bool estopState = hal->digitalRead(ESTOP_PIN);
    if (ESTOP_PIN_ACTIVE_LOW) estopState = !estopState;

    if (estopState && !_estopActive) {
        emergencyStop();
    }
    _estopActive = estopState;
    #endif
}

bool Safety::checkSoftLimits(float target[4]) const {
    #if SOFT_LIMITS_ENABLED
    if (target[0] < 0.0f || target[0] > DEFAULT_MAX_TRAVEL_X) return false;
    if (target[1] < 0.0f || target[1] > DEFAULT_MAX_TRAVEL_Y) return false;
    #if NUM_AXES >= 3
    if (target[2] < 0.0f || target[2] > DEFAULT_MAX_TRAVEL_Z) return false;
    #endif
    #else
    (void)target;
    #endif
    return true;
}

void Safety::hardLimitTriggered() {
    if (!_stepper || !_status) return;

    LOG_ERROR("Safety: HARD LIMIT TRIGGERED — Emergency stop!");
    _stepper->emergencyStop();
    _status->setAlarm(1); // Alarm 1: Hard limit
}

void Safety::emergencyStop() {
    if (!_stepper || !_status) return;

    LOG_ERROR("Safety: EMERGENCY STOP!");
    _stepper->emergencyStop();
    _status->setAlarm(2); // Alarm 2: E-stop
}

void Safety::hardLimitISR() {
    if (_instance) _instance->hardLimitTriggered();
}

void Safety::estopISR() {
    if (_instance) _instance->emergencyStop();
}
