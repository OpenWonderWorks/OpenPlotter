/**
 * ============================================================================
 * OpenPlotter — Tangential Knife Controller Implementation
 * ============================================================================
 */

#include "tangential_knife.h"
#include "../utils/math_utils.h"
#include "../utils/logger.h"
#include <math.h>

TangentialKnife::TangentialKnife() {}

void TangentialKnife::init() {
    _liftServo.init();

    #ifdef C_STEP_PIN
    hal->pinMode(C_STEP_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(C_DIR_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(C_ENABLE_PIN, PinMode::OUTPUT_MODE);
    hal->digitalWrite(C_ENABLE_PIN, !STEPPER_ENABLE_ACTIVE_LOW); // Disable initially
    #endif

    LOG_INFO("TangentialKnife: Initialized (swivel=%.1f° offset=%.2fmm)",
             _swivelThreshold, _bladeOffset);
}

void TangentialKnife::toolDown(float pressure) {
    if (pressure >= 0.0f) {
        _pressure = MathUtils::clamp(pressure, 0.0f, 255.0f);
    }
    _liftServo.toolDown(_pressure);
    _isDown = true;
}

void TangentialKnife::toolUp() {
    _liftServo.toolUp();
    _isDown = false;
}

void TangentialKnife::setPressure(float pressure) {
    _pressure = MathUtils::clamp(pressure, 0.0f, 255.0f);
    _liftServo.setPressure(_pressure);
}

void TangentialKnife::setBladeAngle(float angleRad, bool forceRotate) {
    #ifndef C_STEP_PIN
    (void)angleRad;
    (void)forceRotate;
    return; // No C-axis available
    #else

    // Normalize angle to -π to π
    while (angleRad > MathUtils::PI_F) angleRad -= MathUtils::TWO_PI_F;
    while (angleRad < -MathUtils::PI_F) angleRad += MathUtils::TWO_PI_F;

    // Calculate angle TRIG_CHANGE
    float deltaAngle = angleRad - _currentAngle;

    // Normalize delta to -π to π
    while (deltaAngle > MathUtils::PI_F) deltaAngle -= MathUtils::TWO_PI_F;
    while (deltaAngle < -MathUtils::PI_F) deltaAngle += MathUtils::TWO_PI_F;

    float deltaAngleDeg = fabsf(MathUtils::radToDeg(deltaAngle));

    if (deltaAngleDeg < 0.5f && !forceRotate) {
        return; // Angle TRIG_CHANGE too small — let drag handle it
    }

    if (deltaAngleDeg > _swivelThreshold && _isDown) {
        // Sharp corner: lift → rotate → drop
        liftRotateDrop(angleRad);
    } else {
        // Gradual curve: rotate while cutting (drag knife behavior)
        rotateBladeTo(angleRad);
    }

    #endif
}

void TangentialKnife::rotateBladeTo(float angleRad) {
    #ifdef C_STEP_PIN

    float deltaAngle = angleRad - _currentAngle;

    // Normalize to shortest path
    while (deltaAngle > MathUtils::PI_F) deltaAngle -= MathUtils::TWO_PI_F;
    while (deltaAngle < -MathUtils::PI_F) deltaAngle += MathUtils::TWO_PI_F;

    float deltaDeg = MathUtils::radToDeg(deltaAngle);
    int32_t steps = (int32_t)(deltaDeg * DEFAULT_STEPS_PER_DEG_C);

    if (steps == 0) return;

    // Set direction
    bool dir = (steps > 0);
    bool invert = DEFAULT_INVERT_C;
    hal->digitalWrite(C_DIR_PIN, dir ? !invert : invert);
    hal->delayUs(STEP_IDLE_DELAY_MS);

    // Enable C-axis stepper
    hal->digitalWrite(C_ENABLE_PIN, STEPPER_ENABLE_ACTIVE_LOW ? false : true);

    // Generate steps
    uint32_t absSteps = (uint32_t)MathUtils::iabs(steps);
    uint32_t stepDelay = 500; // µs — moderate speed for rotation

    for (uint32_t i = 0; i < absSteps; i++) {
        hal->digitalWrite(C_STEP_PIN, true);
        hal->delayUs(STEP_PULSE_WIDTH_US);
        hal->digitalWrite(C_STEP_PIN, false);
        hal->delayUs(stepDelay);
    }

    _currentAngle = angleRad;

    #else
    (void)angleRad;
    #endif
}

void TangentialKnife::liftRotateDrop(float angleRad) {
    LOG_DEBUG_GCODE("TangentialKnife: Lift-Rotate-Drop to %.1f°",
                     MathUtils::radToDeg(angleRad));

    // 1. Lift blade
    _liftServo.toolUp();

    // 2. Rotate to new angle
    rotateBladeTo(angleRad);

    // 3. Drop blade
    _liftServo.toolDown(_pressure);

    _isDown = true;
}
