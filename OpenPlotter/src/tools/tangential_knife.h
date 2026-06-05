/**
 * ============================================================================
 * OpenPlotter — Tangential Knife Controller
 * ============================================================================
 *
 * Controls a rotary C-axis for tangential knife cutting. The blade is
 * actively steered by a stepper motor to always face the direction of
 * travel. At sharp corners (beyond the swivel threshold), the knife
 * lifts, rotates, and drops back down.
 *
 * This is a Phase 2 advanced feature. Uses the E0 stepper on RAMPS 1.4.
 *
 * ============================================================================
 */

#ifndef TANGENTIAL_KNIFE_H
#define TANGENTIAL_KNIFE_H

#include "tool_controller.h"
#include "pen_servo.h"

class TangentialKnife : public ToolController {
public:
    TangentialKnife();

    void init() override;
    void toolDown(float pressure = -1.0f) override;
    void toolUp() override;
    void setPressure(float pressure) override;
    bool isDown() const override { return _isDown; }
    float getPressure() const override { return _pressure; }

    /**
     * Update the blade angle based on the direction of the next move.
     * Called before each cutting move by the motion executor.
     *
     * @param angleRad The target angle in radians
     * @param forceRotate If true, always rotate (even below threshold)
     */
    void setBladeAngle(float angleRad, bool forceRotate = false);

    /**
     * Get the current blade angle in radians.
     */
    float getCurrentAngle() const { return _currentAngle; }

    /**
     * Set the swivel threshold (degrees). Angle changes below this
     * are handled by drag; above this triggers lift-rotate-drop.
     */
    void setSwivelThreshold(float degrees) { _swivelThreshold = degrees; }

    /**
     * Set blade offset (mm). Distance from rotation center to blade tip.
     */
    void setBladeOffset(float mm) { _bladeOffset = mm; }

private:
    PenServo _liftServo;        // Servo for lifting the blade
    bool _isDown = false;
    float _pressure = DEFAULT_BLADE_PRESSURE;
    float _currentAngle = 0.0f; // Current blade angle in radians
    float _swivelThreshold = TANGENTIAL_SWIVEL_THRESHOLD;
    float _bladeOffset = TANGENTIAL_BLADE_OFFSET;

    void rotateBladeTo(float angleRad);
    void liftRotateDrop(float angleRad);
};

#endif // TANGENTIAL_KNIFE_H
