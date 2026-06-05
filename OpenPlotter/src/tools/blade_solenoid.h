/**
 * ============================================================================
 * OpenPlotter — Solenoid Blade Controller
 * ============================================================================
 */

#ifndef BLADE_SOLENOID_H
#define BLADE_SOLENOID_H

#include "tool_controller.h"

class BladeSolenoid : public ToolController {
public:
    void init() override;
    void toolDown(float pressure = -1.0f) override;
    void toolUp() override;
    void setPressure(float pressure) override;
    bool isDown() const override { return _isDown; }
    float getPressure() const override { return _pressure; }

private:
    bool _isDown = false;
    float _pressure = DEFAULT_BLADE_PRESSURE;
};

#endif // BLADE_SOLENOID_H
