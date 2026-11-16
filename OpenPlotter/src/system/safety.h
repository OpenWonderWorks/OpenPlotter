/**
 * ============================================================================
 * OpenPlotter — Safety System
 * ============================================================================
 */

#ifndef SAFETY_H
#define SAFETY_H

#include "../hal/hal.h"
#include "../motion/stepper.h"
#include "../motion/planner.h"
#include "../homing/homing.h"
#include "../tools/tool_controller.h"
#include "settings.h"
#include "status.h"
#include "../../openplotter_config.h"

class Safety {
public:
    void init(Stepper* stepper, Planner* planner, Status* status,
              Settings* settings, HomingManager* homing, ToolController* tool);

    /**
     * Check safety conditions. Call from main loop.
     * Triggers emergency stop if any condition is violated.
     */
    void update();

    /**
     * Check if a target position is within soft limits.
     */
    bool checkSoftLimits(float target[4]) const;

    /**
     * Handle hard limit switch trigger during operation.
     */
    void hardLimitTriggered();

    /**
     * Handle emergency stop button press.
     */
    void emergencyStop();

    /** A stop makes the estimated position unsafe until homing succeeds. */
    bool requiresHoming() const { return _requiresHoming; }
    void markPositionKnown() { _requiresHoming = false; }
    void markPositionUnknown() { _requiresHoming = true; }
    bool estopActive() const { return _estopActive; }

private:
    Stepper* _stepper = nullptr;
    Planner* _planner = nullptr;
    Status* _status = nullptr;
    Settings* _settings = nullptr;
    HomingManager* _homing = nullptr;
    ToolController* _tool = nullptr;
    bool _estopActive = false;
    // Machine coordinates are unknown after every controller reboot.
    bool _requiresHoming = true;
    uint8_t _lastLimitMask = 0;

    static void hardLimitISR();
    static void estopISR();
    static Safety* _instance;
};

#endif // SAFETY_H
