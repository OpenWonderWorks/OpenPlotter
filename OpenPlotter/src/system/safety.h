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
#include "status.h"
#include "config.h"

class Safety {
public:
    void init(Stepper* stepper, Planner* planner, Status* status);

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

private:
    Stepper* _stepper = nullptr;
    Planner* _planner = nullptr;
    Status* _status = nullptr;
    bool _estopActive = false;

    static void hardLimitISR();
    static void estopISR();
    static Safety* _instance;
};

#endif // SAFETY_H
