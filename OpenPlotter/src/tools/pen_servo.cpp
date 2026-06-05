/**
 * ============================================================================
 * OpenPlotter — Servo Pen/Blade Controller Implementation
 * ============================================================================
 *
 * Controls a hobby servo to raise/lower the pen or blade holder.
 * Pressure is mapped to the servo angle range: higher pressure moves
 * the servo further toward the "down" position.
 *
 * ============================================================================
 */

#include "pen_servo.h"
#include "../utils/math_utils.h"
#include "../utils/logger.h"

void PenServo::init() {
    hal->servoAttach(PEN_SERVO_PIN);
    toolUp(); // Start in safe position
    LOG_INFO("PenServo: Initialized (up=%d° down=%d° delay=%dms)",
             _upAngle, _downAngle, _delayMs);
}

void PenServo::toolDown(float pressure) {
    if (pressure >= 0.0f) {
        _pressure = MathUtils::clamp(pressure, 0.0f, 255.0f);
    }

    uint8_t targetAngle = pressureToAngle(_pressure);
    hal->servoWrite(PEN_SERVO_PIN, targetAngle);
    hal->delayMs(_delayMs);
    _isDown = true;

    LOG_DEBUG_GCODE("PenServo: DOWN angle=%d° pressure=%.0f", targetAngle, _pressure);
}

void PenServo::toolUp() {
    hal->servoWrite(PEN_SERVO_PIN, _upAngle);
    hal->delayMs(_delayMs);
    _isDown = false;

    LOG_DEBUG_GCODE("PenServo: UP angle=%d°", _upAngle);
}

void PenServo::setPressure(float pressure) {
    _pressure = MathUtils::clamp(pressure, 0.0f, 255.0f);
    if (_isDown) {
        // If currently engaged, update angle immediately
        uint8_t targetAngle = pressureToAngle(_pressure);
        hal->servoWrite(PEN_SERVO_PIN, targetAngle);
    }
}

uint8_t PenServo::pressureToAngle(float pressure) {
    // Map pressure (0-255) to angle range (upAngle → downAngle)
    // Higher pressure = closer to downAngle (more force)
    return (uint8_t)MathUtils::mapFloat(
        pressure, 0.0f, 255.0f,
        (float)_upAngle, (float)_downAngle
    );
}
