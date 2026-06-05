/**
 * ============================================================================
 * OpenPlotter — Solenoid Blade Controller Implementation
 * ============================================================================
 *
 * Controls a solenoid for blade engagement. Uses PWM for pressure control:
 *   - Initial engage: Full power (SOLENOID_ENGAGE_PWM) for positive engagement
 *   - Hold: Reduced power (SOLENOID_HOLD_PWM) to minimize heat
 *
 * ============================================================================
 */

#include "blade_solenoid.h"
#include "../utils/math_utils.h"
#include "../utils/logger.h"

void BladeSolenoid::init() {
    hal->pinMode(SOLENOID_PIN, PinMode::OUTPUT_MODE);
    hal->analogWrite(SOLENOID_PIN, 0); // Start disengaged
    LOG_INFO("BladeSolenoid: Initialized on pin %d", SOLENOID_PIN);
}

void BladeSolenoid::toolDown(float pressure) {
    if (pressure >= 0.0f) {
        _pressure = MathUtils::clamp(pressure, 0.0f, 255.0f);
    }

    // Full power engage
    hal->analogWrite(SOLENOID_PIN, SOLENOID_ENGAGE_PWM);
    hal->delayMs(SOLENOID_ENGAGE_DELAY);

    // Reduce to holding power (scaled by pressure)
    uint8_t holdPwm = (uint8_t)MathUtils::mapFloat(
        _pressure, 0.0f, 255.0f,
        0.0f, (float)SOLENOID_HOLD_PWM
    );
    hal->analogWrite(SOLENOID_PIN, holdPwm);

    _isDown = true;
    LOG_DEBUG_GCODE("BladeSolenoid: DOWN hold_pwm=%d pressure=%.0f", holdPwm, _pressure);
}

void BladeSolenoid::toolUp() {
    hal->analogWrite(SOLENOID_PIN, 0);
    _isDown = false;
    LOG_DEBUG_GCODE("BladeSolenoid: UP");
}

void BladeSolenoid::setPressure(float pressure) {
    _pressure = MathUtils::clamp(pressure, 0.0f, 255.0f);
    if (_isDown) {
        uint8_t holdPwm = (uint8_t)MathUtils::mapFloat(
            _pressure, 0.0f, 255.0f,
            0.0f, (float)SOLENOID_HOLD_PWM
        );
        hal->analogWrite(SOLENOID_PIN, holdPwm);
    }
}
