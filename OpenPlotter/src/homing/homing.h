/**
 * ============================================================================
 * OpenPlotter — Homing System Header
 * ============================================================================
 */

#ifndef HOMING_H
#define HOMING_H

#include "../hal/hal.h"
#include "../motion/stepper.h"
#include "../motion/planner.h"
#include "../../openplotter_config.h"
#include <stdint.h>

// ── Homing Method ───────────────────────────────────────────────────────────
enum class HomingMethod : uint8_t {
    SENSORED = 0,     // Physical limit switches
    SENSORLESS = 1    // TMC2209 StallGuard
};

// ── Homing State ────────────────────────────────────────────────────────────
enum class HomingState : uint8_t {
    IDLE,
    SEEKING,          // Fast approach toward switch/stall
    PULLING_OFF,      // Moving away from switch
    FEEDING,          // Slow precision approach
    COMPLETE,
    ERROR
};

// ── Homing Result ───────────────────────────────────────────────────────────
enum class HomingResult : uint8_t {
    OK,
    SWITCH_NOT_FOUND,
    STALL_NOT_DETECTED,
    TMC_COMM_ERROR,
    TIMEOUT,
    ABORTED
};

class HomingManager {
public:
    HomingManager();

    /**
     * Initialize homing with references to stepper and planner.
     */
    void init(Stepper* stepper, Planner* planner);

    /**
     * Execute a full homing cycle for all configured axes.
     * Blocking call — returns when homing is complete or fails.
     * @return HomingResult indicating success or failure type
     */
    HomingResult homeAll();

    /**
     * Home a single axis.
     * @param axis 0=X, 1=Y, 2=Z, 3=C
     * @return HomingResult
     */
    HomingResult homeAxis(uint8_t axis);

    /**
     * Set homing method.
     */
    void setMethod(HomingMethod method) { _method = method; }
    HomingMethod getMethod() const { return _method; }

    /**
     * Check if machine has been homed.
     */
    bool isHomed() const { return _isHomed; }

    /**
     * Get current homing state.
     */
    HomingState getState() const { return _state; }

    /**
     * Read current limit switch states.
     * @return Bitmask: bit0=X_MIN, bit1=X_MAX, bit2=Y_MIN, etc.
     */
    uint8_t readEndstops();

    /**
     * Set StallGuard threshold for sensorless homing.
     */
    void setSgThreshold(uint8_t threshold) { _sgThreshold = threshold; }

    /**
     * Set homing current for sensorless homing (mA).
     */
    void setSgHomingCurrent(uint16_t current) { _sgHomingCurrent = current; }

    /**
     * Abort current homing cycle.
     */
    void abort() { _aborted = true; }

private:
    Stepper* _stepper = nullptr;
    Planner* _planner = nullptr;
    HomingMethod _method = HomingMethod::SENSORED;
    HomingState _state = HomingState::IDLE;
    bool _isHomed = false;
    bool _aborted = false;

    // Sensorless homing parameters
    uint8_t _sgThreshold = DEFAULT_SG_THRESHOLD;
    uint16_t _sgHomingCurrent = DEFAULT_SG_HOMING_CURRENT;

    // Axis pin mapping helpers
    uint8_t getStepPin(uint8_t axis);
    uint8_t getDirPin(uint8_t axis);
    uint8_t getEnablePin(uint8_t axis);
    uint8_t getMinPin(uint8_t axis);
    bool getHomeDir(uint8_t axis);
    float getSeekRate(uint8_t axis);
    float getFeedRate(uint8_t axis);

    // Internal homing methods
    HomingResult homeSensored(uint8_t axis);
    HomingResult homeSensorless(uint8_t axis);

    // Low-level step generation for homing (bypasses planner)
    void stepAxis(uint8_t axis, bool direction, float speedMmPerSec,
                  uint32_t maxSteps, bool (*stopCondition)(uint8_t axis, void* ctx),
                  void* ctx);
};

#endif // HOMING_H
