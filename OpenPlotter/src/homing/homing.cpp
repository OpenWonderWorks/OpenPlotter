/**
 * ============================================================================
 * OpenPlotter — Homing System Implementation
 * ============================================================================
 *
 * Implements both sensored (limit switch) and sensorless (TMC StallGuard)
 * homing for all configured axes.
 *
 * SENSORED HOMING SEQUENCE:
 *   1. Seek: Move toward home at high speed until switch triggers
 *   2. Pull-off: Move away from switch by pull-off distance
 *   3. Feed: Approach switch slowly for precision
 *   4. Zero: Set position to 0
 *
 * SENSORLESS HOMING SEQUENCE:
 *   1. Configure TMC2209: Set StallGuard threshold, reduce current
 *   2. Move at constant speed toward end of axis
 *   3. Monitor DIAG pin for stall detection
 *   4. On stall: stop, pull off, restore normal current
 *   5. Set position to 0
 *
 * ============================================================================
 */

#include "homing.h"
#include "../utils/logger.h"
#include "../motion/tmc_driver.h"
#include "../../openplotter_config.h"
#include <string.h>

// ============================================================================
// Constructor & Init
// ============================================================================

HomingManager::HomingManager() {}

void HomingManager::init(Stepper* stepper, Planner* planner) {
    _stepper = stepper;
    _planner = planner;

    // Configure endstop pins as inputs
    hal->pinMode(X_MIN_PIN, LIMIT_SWITCH_PULLUP
                 ? PinMode::INPUT_PULLUP_MODE : PinMode::INPUT_MODE);
    hal->pinMode(Y_MIN_PIN, LIMIT_SWITCH_PULLUP
                 ? PinMode::INPUT_PULLUP_MODE : PinMode::INPUT_MODE);

    #ifdef Z_MIN_PIN
    hal->pinMode(Z_MIN_PIN, LIMIT_SWITCH_PULLUP
                 ? PinMode::INPUT_PULLUP_MODE : PinMode::INPUT_MODE);
    #endif
}

// ============================================================================
// Pin Mapping Helpers
// ============================================================================

uint8_t HomingManager::getStepPin(uint8_t axis) {
    switch (axis) {
        case 0: return X_STEP_PIN;
        case 1: return Y_STEP_PIN;
        #ifdef Z_STEP_PIN
        case 2: return Z_STEP_PIN;
        #endif
        #ifdef C_STEP_PIN
        case 3: return C_STEP_PIN;
        #endif
        default: return 0;
    }
}

uint8_t HomingManager::getDirPin(uint8_t axis) {
    switch (axis) {
        case 0: return X_DIR_PIN;
        case 1: return Y_DIR_PIN;
        #ifdef Z_DIR_PIN
        case 2: return Z_DIR_PIN;
        #endif
        #ifdef C_DIR_PIN
        case 3: return C_DIR_PIN;
        #endif
        default: return 0;
    }
}

uint8_t HomingManager::getEnablePin(uint8_t axis) {
    switch (axis) {
        case 0: return X_ENABLE_PIN;
        case 1: return Y_ENABLE_PIN;
        #ifdef Z_ENABLE_PIN
        case 2: return Z_ENABLE_PIN;
        #endif
        #ifdef C_ENABLE_PIN
        case 3: return C_ENABLE_PIN;
        #endif
        default: return 0;
    }
}

uint8_t HomingManager::getMinPin(uint8_t axis) {
    switch (axis) {
        case 0: return X_MIN_PIN;
        case 1: return Y_MIN_PIN;
        #ifdef Z_MIN_PIN
        case 2: return Z_MIN_PIN;
        #endif
        default: return 0;
    }
}

bool HomingManager::getHomeDir(uint8_t axis) {
    switch (axis) {
        case 0: return HOME_DIR_X;
        case 1: return HOME_DIR_Y;
        case 2: return HOME_DIR_Z;
        default: return false;
    }
}

float HomingManager::getSeekRate(uint8_t axis) {
    (void)axis;
    return DEFAULT_HOMING_SEEK_RATE; // mm/min
}

float HomingManager::getFeedRate(uint8_t axis) {
    (void)axis;
    return DEFAULT_HOMING_FEED_RATE; // mm/min
}

// ============================================================================
// Endstop Reading
// ============================================================================

uint8_t HomingManager::readEndstops() {
    uint8_t status = 0;

    bool xMin = hal->digitalRead(X_MIN_PIN);
    bool yMin = hal->digitalRead(Y_MIN_PIN);

    // If switches are normally closed, invert the reading
    if (LIMIT_SWITCH_NC) {
        xMin = !xMin;
        yMin = !yMin;
    }

    if (xMin) status |= 0x01;
    if (yMin) status |= 0x04;

    #ifdef Z_MIN_PIN
    bool zMin = hal->digitalRead(Z_MIN_PIN);
    if (LIMIT_SWITCH_NC) zMin = !zMin;
    if (zMin) status |= 0x10;
    #endif

    return status;
}

// ============================================================================
// Home All Axes
// ============================================================================

HomingResult HomingManager::homeAll() {
    _aborted = false;
    _isHomed = false;

    LOG_INFO("Homing: Starting (%s mode)",
             _method == HomingMethod::SENSORED ? "sensored" : "sensorless");

    // Enable motors
    _stepper->enableMotors(true);
    hal->delayMs(50); // Let motors settle

    // Home X first, then Y
    HomingResult result;

    result = homeAxis(0); // X
    if (result != HomingResult::OK) return result;

    result = homeAxis(1); // Y
    if (result != HomingResult::OK) return result;

    // Home Z if configured
    #if NUM_AXES >= 3 && defined(Z_STEP_PIN)
    result = homeAxis(2); // Z
    if (result != HomingResult::OK) return result;
    #endif

    _isHomed = true;

    // Set machine position to zero
    float zeroPos[4] = {0, 0, 0, 0};
    _planner->setPosition(zeroPos);

    LOG_INFO("Homing: Complete");
    return HomingResult::OK;
}

// ============================================================================
// Home Single Axis
// ============================================================================

HomingResult HomingManager::homeAxis(uint8_t axis) {
    if (axis >= 4) return HomingResult::SWITCH_NOT_FOUND;

    const char* axisNames[] = {"X", "Y", "Z", "C"};
    LOG_INFO("Homing: Axis %s", axisNames[axis]);

    _state = HomingState::SEEKING;

    HomingResult result;

    if (_method == HomingMethod::SENSORLESS) {
        result = homeSensorless(axis);
    } else {
        result = homeSensored(axis);
    }

    _state = (result == HomingResult::OK) ? HomingState::COMPLETE : HomingState::ERROR;
    return result;
}

// ============================================================================
// Low-Level Step Generation (Bypasses Planner)
// ============================================================================
// During homing, we bypass the motion planner and directly generate step
// pulses. This gives us direct control over speed and direction, and lets
// us react immediately to switch triggers or stall detection.

void HomingManager::stepAxis(uint8_t axis, bool direction, float speedMmPerSec,
                              uint32_t maxSteps,
                              bool (*stopCondition)(uint8_t axis, void* ctx),
                              void* ctx) {
    uint8_t stepPin = getStepPin(axis);
    uint8_t dirPin = getDirPin(axis);

    // Set direction
    bool invert = false;
    switch (axis) {
        case 0: invert = DEFAULT_INVERT_X; break;
        case 1: invert = DEFAULT_INVERT_Y; break;
        case 2: invert = DEFAULT_INVERT_Z; break;
        case 3: invert = DEFAULT_INVERT_C; break;
    }
    hal->digitalWrite(dirPin, direction ? !invert : invert);
    hal->delayUs(STEP_IDLE_DELAY_MS);

    // Calculate step interval in microseconds
    float stepsPerMm = DEFAULT_STEPS_PER_MM_X; // TODO: per-axis from settings
    if (axis == 1) stepsPerMm = DEFAULT_STEPS_PER_MM_Y;
    if (axis == 2) stepsPerMm = DEFAULT_STEPS_PER_MM_Z;

    float stepsPerSec = speedMmPerSec * stepsPerMm;
    uint32_t stepIntervalUs = (uint32_t)(1000000.0f / stepsPerSec);
    if (stepIntervalUs < 20) stepIntervalUs = 20; // Minimum 20µs between steps

    // Step loop
    for (uint32_t i = 0; i < maxSteps; i++) {
        if (_aborted) return;

        // Check stop condition
        if (stopCondition && stopCondition(axis, ctx)) {
            LOG_DEBUG_HOMING("Stop condition met at step %lu", i);
            return;
        }

        // Generate step pulse
        hal->digitalWrite(stepPin, true);
        hal->delayUs(STEP_PULSE_WIDTH_US);
        hal->digitalWrite(stepPin, false);
        hal->delayUs(stepIntervalUs - STEP_PULSE_WIDTH_US);
    }
}

// ============================================================================
// Sensored Homing (Limit Switches)
// ============================================================================

// Stop condition: limit switch triggered
static bool limitSwitchTriggered(uint8_t axis, void* ctx) {
    HomingManager* homing = (HomingManager*)ctx;
    uint8_t endstops = homing->readEndstops();

    switch (axis) {
        case 0: return (endstops & 0x01) != 0; // X_MIN
        case 1: return (endstops & 0x04) != 0; // Y_MIN
        case 2: return (endstops & 0x10) != 0; // Z_MIN
        default: return false;
    }
}

// Stop condition: limit switch NOT triggered (for pull-off)
static bool limitSwitchReleased(uint8_t axis, void* ctx) {
    return !limitSwitchTriggered(axis, ctx);
}

HomingResult HomingManager::homeSensored(uint8_t axis) {
    bool homeDir = getHomeDir(axis);
    float seekRate = getSeekRate(axis) / 60.0f;  // mm/min → mm/sec
    float feedRate = getFeedRate(axis) / 60.0f;
    float pulloffMm = DEFAULT_HOMING_PULLOFF;

    float stepsPerMm = DEFAULT_STEPS_PER_MM_X;
    if (axis == 1) stepsPerMm = DEFAULT_STEPS_PER_MM_Y;
    if (axis == 2) stepsPerMm = DEFAULT_STEPS_PER_MM_Z;

    uint32_t maxSeekSteps = (uint32_t)(DEFAULT_MAX_TRAVEL_X * stepsPerMm * 1.5f);
    uint32_t pulloffSteps = (uint32_t)(pulloffMm * stepsPerMm);
    uint32_t maxFeedSteps = (uint32_t)(pulloffMm * 3.0f * stepsPerMm);

    // ── Phase 1: SEEK — Fast approach toward home switch ──────────────
    LOG_DEBUG_HOMING("Phase 1: Seeking (%.0f mm/min)", seekRate * 60.0f);
    _state = HomingState::SEEKING;

    // If switch is already triggered, pull off first
    if (limitSwitchTriggered(axis, this)) {
        LOG_DEBUG_HOMING("Switch already triggered, pulling off first");
        stepAxis(axis, !homeDir, seekRate, pulloffSteps * 3,
                 limitSwitchReleased, this);
        hal->delayMs(HOMING_DEBOUNCE_MS);
    }

    stepAxis(axis, homeDir, seekRate, maxSeekSteps,
             limitSwitchTriggered, this);

    if (_aborted) return HomingResult::ABORTED;

    // Verify switch was actually hit
    if (!limitSwitchTriggered(axis, this)) {
        LOG_ERROR("Homing: Switch not found for axis %d", axis);
        return HomingResult::SWITCH_NOT_FOUND;
    }

    hal->delayMs(HOMING_DEBOUNCE_MS);

    // ── Phase 2: PULL-OFF — Move away from switch ─────────────────────
    LOG_DEBUG_HOMING("Phase 2: Pulling off (%.1f mm)", pulloffMm);
    _state = HomingState::PULLING_OFF;

    stepAxis(axis, !homeDir, seekRate, pulloffSteps,
             limitSwitchReleased, this);

    if (_aborted) return HomingResult::ABORTED;
    hal->delayMs(HOMING_DEBOUNCE_MS);

    // ── Phase 3: FEED — Slow precision approach ───────────────────────
    LOG_DEBUG_HOMING("Phase 3: Feeding (%.0f mm/min)", feedRate * 60.0f);
    _state = HomingState::FEEDING;

    stepAxis(axis, homeDir, feedRate, maxFeedSteps,
             limitSwitchTriggered, this);

    if (_aborted) return HomingResult::ABORTED;

    if (!limitSwitchTriggered(axis, this)) {
        LOG_ERROR("Homing: Switch not found in feed phase for axis %d", axis);
        return HomingResult::SWITCH_NOT_FOUND;
    }

    hal->delayMs(HOMING_DEBOUNCE_MS);

    // ── Phase 4: Final pull-off ────────────────────────────────────────
    stepAxis(axis, !homeDir, feedRate, pulloffSteps, nullptr, nullptr);

    LOG_INFO("Homing: Axis %d complete", axis);
    return HomingResult::OK;
}

// ============================================================================
// Sensorless Homing (TMC2209 StallGuard)
// ============================================================================

#ifdef HAS_TMC_UART
#include <TMCStepper.h>

// StallGuard stall detection via DIAG pin
static volatile bool _stallDetected = false;

static void stallISR() {
    _stallDetected = true;
}

static bool stallCondition(uint8_t axis, void* ctx) {
    (void)axis;
    (void)ctx;
    return _stallDetected;
}

HomingResult HomingManager::homeSensorless(uint8_t axis) {
    if (!isSensorlessCapable(axis)) {
        LOG_WARN("Sensorless homing not supported on selected driver for axis %d. TRIG_FALLING back to sensored.", axis);
        return homeSensored(axis);
    }

    bool homeDir = getHomeDir(axis);
    float homingSpeed = DEFAULT_SG_HOMING_SPEED; // mm/sec (constant speed required)
    float pulloffMm = DEFAULT_HOMING_PULLOFF;

    float stepsPerMm = DEFAULT_STEPS_PER_MM_X;
    if (axis == 1) stepsPerMm = DEFAULT_STEPS_PER_MM_Y;
    if (axis == 2) stepsPerMm = DEFAULT_STEPS_PER_MM_Z;

    uint32_t maxSteps = (uint32_t)(DEFAULT_MAX_TRAVEL_X * stepsPerMm * 1.5f);
    uint32_t pulloffSteps = (uint32_t)(pulloffMm * stepsPerMm);

    // ── Configure TMC driver for homing (current, StealthChop bypass, DIAG out) ──
    LOG_DEBUG_HOMING("Configuring driver StallGuard for sensorless homing");
    configureTmcForHoming(axis, true);
    hal->delayMs(50); // Let driver settle

    // ── Attach interrupt on DIAG pin ───────────────────────────────────
    uint8_t diagPin = getMinPin(axis); // DIAG connects to limit switch header
    hal->pinMode(diagPin, PinMode::INPUT_MODE);
    _stallDetected = false;
    hal->attachInterrupt(diagPin, stallISR, InterruptTrigger::TRIG_RISING);

    // ── Phase 1: Move toward end at constant speed ─────────────────────
    LOG_DEBUG_HOMING("Sensorless seek: speed=%.0f mm/s, threshold=%d, current=%d mA",
                     homingSpeed, _sgThreshold, _sgHomingCurrent);
    _state = HomingState::SEEKING;

    stepAxis(axis, homeDir, homingSpeed, maxSteps, stallCondition, nullptr);

    // Detach interrupt
    hal->detachInterrupt(diagPin);

    // Turn off StallGuard and restore normal currents
    configureTmcForHoming(axis, false);

    if (_aborted) {
        return HomingResult::ABORTED;
    }

    if (!_stallDetected) {
        LOG_ERROR("Sensorless homing: Stall not detected for axis %d", axis);
        return HomingResult::STALL_NOT_DETECTED;
    }

    LOG_DEBUG_HOMING("Stall detected! Pulling off...");

    // ── Phase 2: Pull off ──────────────────────────────────────────────
    _state = HomingState::PULLING_OFF;
    _stallDetected = false;
    hal->delayMs(100); // Let motor settle after stall

    stepAxis(axis, !homeDir, homingSpeed * 0.5f, pulloffSteps, nullptr, nullptr);

    LOG_INFO("Sensorless homing: Axis %d complete", axis);
    return HomingResult::OK;
}

#else
HomingResult HomingManager::homeSensorless(uint8_t axis) {
    LOG_WARN("Sensorless homing not supported. Falling back to sensored.");
    return homeSensored(axis);
}
#endif
