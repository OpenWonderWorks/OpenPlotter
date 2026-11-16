/**
 * Small, hardware-independent safety checks shared by the firmware and native
 * regression tests.
 */
#ifndef OPENPLOTTER_SAFETY_CHECKS_H
#define OPENPLOTTER_SAFETY_CHECKS_H

#include <math.h>
#include <stdint.h>

inline bool isActiveLevel(bool pinHigh, bool activeLow) {
    return activeLow ? !pinHigh : pinHigh;
}

// With the documented NC-to-GND wiring and an input pull-up, a healthy switch
// reads LOW and an opened/triggered switch reads HIGH.
inline bool isLimitTriggered(bool pinHigh, bool normallyClosed) {
    return normallyClosed ? pinHigh : !pinHigh;
}

inline uint8_t homeSwitchMaskForAxis(uint8_t axis) {
    switch (axis) {
        case 0: return 0x01; // X_MIN
        case 1: return 0x04; // Y_MIN
        case 2: return 0x10; // Z_MIN
        default: return 0;
    }
}

inline bool isHomeSwitchActive(uint8_t endstopMask, uint8_t axis) {
    const uint8_t homeMask = homeSwitchMaskForAxis(axis);
    return homeMask != 0 && (endstopMask & homeMask) != 0;
}

// These commands can stop motion or disengage outputs without moving the
// machine. Keep them available when position is unknown or an alarm is active.
inline bool isSafeMCodeWhenStopped(uint16_t mcode) {
    switch (mcode) {
        case 0:   // M0 program pause
        case 1:   // M1 optional pause
        case 2:   // M2 program end
        case 5:   // M5 tool up/off
        case 18:  // M18 disable motors
        case 30:  // M30 program end
        case 107: // M107 output off
            return true;
        default:
            return false;
    }
}

inline bool withinTravelLimits(const float target[4], const float maxTravel[4],
                               uint8_t axisCount) {
    if (!target || !maxTravel || axisCount == 0 || axisCount > 4) return false;

    for (uint8_t axis = 0; axis < axisCount; ++axis) {
        if (!isfinite(target[axis]) || !isfinite(maxTravel[axis]) ||
            maxTravel[axis] <= 0.0f || target[axis] < 0.0f ||
            target[axis] > maxTravel[axis]) {
            return false;
        }
    }
    return true;
}

inline bool blocksCommandUntilHomed(bool requiresHoming, bool hasCommand,
                                    bool homingCommand, bool readOnlyQuery) {
    return requiresHoming && hasCommand && !homingCommand && !readOnlyQuery;
}

inline bool canStartHoming(bool alarmActive, bool estopActive) {
    return !alarmActive && !estopActive;
}

#endif // OPENPLOTTER_SAFETY_CHECKS_H
