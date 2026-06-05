/**
 * ============================================================================
 * OpenPlotter — Stepper Engine Header
 * ============================================================================
 *
 * Interrupt-driven stepper pulse generation using Bresenham's line algorithm
 * for multi-axis synchronization.
 *
 * ============================================================================
 */

#ifndef STEPPER_H
#define STEPPER_H

#include "../hal/hal.h"
#include "planner.h"
#include "../../openplotter_config.h"
#include <stdint.h>

// ── Stepper State ───────────────────────────────────────────────────────────
enum class StepperState : uint8_t {
    IDLE,           // No motion, timers stopped
    RUNNING,        // Executing motion blocks
    HOLD,           // Feed hold (decelerating to stop)
    HOMING          // Executing homing cycle
};

class Stepper {
public:
    Stepper();

    /**
     * Initialize the stepper engine with pin configuration.
     * Must be called after HAL is initialized.
     */
    void init();

    /**
     * Start executing motion blocks from the planner.
     */
    void start();

    /**
     * Stop all motion immediately (emergency stop).
     */
    void emergencyStop();

    /**
     * Initiate a feed hold (controlled deceleration to stop).
     */
    void feedHold();

    /**
     * Resume after a feed hold.
     */
    void resume();

    /**
     * Enable/disable stepper motors.
     */
    void enableMotors(bool enable);

    /**
     * Set the planner instance.
     */
    void setPlanner(Planner* planner) { _planner = planner; }

    /**
     * Get current state.
     */
    StepperState getState() const { return _state; }

    /**
     * @return true if any axis is currently moving.
     */
    bool isMoving() const { return _state == StepperState::RUNNING; }

    /**
     * Called periodically from main loop to check for idle timeout.
     */
    void updateIdleTimer();

    /**
     * Step timer ISR callback. This is called at the step frequency
     * from the hardware timer interrupt.
     * MUST be as fast as possible — direct register access preferred.
     */
    static void stepTimerCallback();

    /**
     * Segment preparation timer callback. Called at ~1kHz to prepare
     * the next step segment from the planner.
     */
    static void segmentTimerCallback();

private:
    Planner* _planner = nullptr;
    StepperState _state = StepperState::IDLE;

    // Current segment being executed
    StepSegment _currentSegment;
    bool _hasSegment = false;
    uint32_t _segmentStepCount = 0;     // Steps executed in current segment

    // Bresenham counters (for multi-axis sync within the ISR)
    int32_t _bresenhamError[4] = {0};

    // Pulse state
    bool _stepPinState = false;
    uint32_t _stepPulseEndTime = 0;

    // Idle timeout
    uint32_t _lastActivityTime = 0;
    bool _motorsEnabled = false;

    // Static instance pointer (ISR needs access)
    static Stepper* _instance;

    // Internal methods
    void loadNextSegment();
    void setDirections(uint8_t dirBits);
};

#endif // STEPPER_H
