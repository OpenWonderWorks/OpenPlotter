/**
 * ============================================================================
 * OpenPlotter — Servo Pen/Blade Controller
 * ============================================================================
 */

#ifndef PEN_SERVO_H
#define PEN_SERVO_H

#include "tool_controller.h"

class PenServo : public ToolController {
public:
    void init() override;
    void toolDown(float pressure = -1.0f) override;
    void toolUp() override;
    void setPressure(float pressure) override;
    bool isDown() const override { return _isDown; }
    float getPressure() const override { return _pressure; }

    void setUpAngle(uint8_t angle) { _upAngle = angle; }
    void setDownAngle(uint8_t angle) { _downAngle = angle; }
    void setDelay(uint16_t delayMs) { _delayMs = delayMs; }

private:
    bool _isDown = false;
    float _pressure = DEFAULT_BLADE_PRESSURE;
    uint8_t _upAngle = DEFAULT_SERVO_UP_ANGLE;
    uint8_t _downAngle = DEFAULT_SERVO_DOWN_ANGLE;
    uint16_t _delayMs = DEFAULT_SERVO_DELAY;

    uint8_t pressureToAngle(float pressure);
};

#endif // PEN_SERVO_H
