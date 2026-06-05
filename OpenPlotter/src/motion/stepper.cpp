/**
 * ============================================================================
 * OpenPlotter — Stepper Engine Implementation
 * ============================================================================
 *
 * The stepper engine is the most performance-critical component.
 * The step timer ISR fires at the step frequency and must complete
 * in microseconds to avoid jitter.
 *
 * Architecture:
 *   1. Segment Timer (~1kHz): Calls planner to prepare segments
 *   2. Step Timer (variable Hz): Executes individual step pulses
 *      using Bresenham's algorithm for multi-axis sync
 *
 * The ISR toggles step pins directly. Direction pins are set when
 * loading a new segment.
 *
 * ============================================================================
 */

#include "stepper.h"
#include "../utils/logger.h"
#include <string.h>

// ── Static instance for ISR access ──────────────────────────────────────────
Stepper* Stepper::_instance = nullptr;

// ============================================================================
// Constructor & Init
// ============================================================================

Stepper::Stepper() {
    _instance = this;
    memset(&_currentSegment, 0, sizeof(_currentSegment));
}

void Stepper::init() {
    // Configure step and direction pins as outputs
    hal->pinMode(X_STEP_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(X_DIR_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(X_ENABLE_PIN, PinMode::OUTPUT_MODE);

    hal->pinMode(Y_STEP_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(Y_DIR_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(Y_ENABLE_PIN, PinMode::OUTPUT_MODE);

    #ifdef Z_STEP_PIN
    hal->pinMode(Z_STEP_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(Z_DIR_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(Z_ENABLE_PIN, PinMode::OUTPUT_MODE);
    #endif

    #ifdef C_STEP_PIN
    hal->pinMode(C_STEP_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(C_DIR_PIN, PinMode::OUTPUT_MODE);
    hal->pinMode(C_ENABLE_PIN, PinMode::OUTPUT_MODE);
    #endif

    // Disable motors initially
    enableMotors(false);

    // Initialize timers
    hal->stepTimerInit(stepTimerCallback);
    hal->segmentTimerInit(segmentTimerCallback);

    _state = StepperState::IDLE;
    _lastActivityTime = hal->millis();
}

// ============================================================================
// Motor Enable/Disable
// ============================================================================

void Stepper::enableMotors(bool enable) {
    _motorsEnabled = enable;

    // Active LOW enable pins (STEPPER_ENABLE_ACTIVE_LOW)
    bool pinLevel = STEPPER_ENABLE_ACTIVE_LOW ? !enable : enable;

    hal->digitalWrite(X_ENABLE_PIN, pinLevel);
    hal->digitalWrite(Y_ENABLE_PIN, pinLevel);

    #ifdef Z_ENABLE_PIN
    hal->digitalWrite(Z_ENABLE_PIN, pinLevel);
    #endif
    #ifdef C_ENABLE_PIN
    hal->digitalWrite(C_ENABLE_PIN, pinLevel);
    #endif

    if (enable) {
        _lastActivityTime = hal->millis();
    }
}

// ============================================================================
// Motion Control
// ============================================================================

void Stepper::start() {
    if (_state == StepperState::RUNNING) return;

    enableMotors(true);
    _state = StepperState::RUNNING;

    // Start the segment preparation timer
    hal->segmentTimerStart();

    LOG_INFO("Stepper: Motion started");
}

void Stepper::emergencyStop() {
    // Immediately stop all timers
    hal->stepTimerStop();
    hal->segmentTimerStop();

    // Reset all step pins LOW
    hal->digitalWrite(X_STEP_PIN, false);
    hal->digitalWrite(Y_STEP_PIN, false);
    #ifdef Z_STEP_PIN
    hal->digitalWrite(Z_STEP_PIN, false);
    #endif
    #ifdef C_STEP_PIN
    hal->digitalWrite(C_STEP_PIN, false);
    #endif

    _state = StepperState::IDLE;
    _hasSegment = false;
    _segmentStepCount = 0;

    LOG_WARN("Stepper: EMERGENCY STOP");
}

void Stepper::feedHold() {
    if (_state == StepperState::RUNNING) {
        _state = StepperState::HOLD;
        LOG_INFO("Stepper: Feed hold");
    }
}

void Stepper::resume() {
    if (_state == StepperState::HOLD) {
        _state = StepperState::RUNNING;
        hal->segmentTimerStart();
        LOG_INFO("Stepper: Resumed");
    }
}

// ============================================================================
// Direction Setup
// ============================================================================

void Stepper::setDirections(uint8_t dirBits) {
    bool invertX = DEFAULT_INVERT_X;
    bool invertY = DEFAULT_INVERT_Y;
    bool invertZ = DEFAULT_INVERT_Z;
    bool invertC = DEFAULT_INVERT_C;

    hal->digitalWrite(X_DIR_PIN, (dirBits & 0x01) ? !invertX : invertX);
    hal->digitalWrite(Y_DIR_PIN, (dirBits & 0x02) ? !invertY : invertY);
    #ifdef Z_DIR_PIN
    hal->digitalWrite(Z_DIR_PIN, (dirBits & 0x04) ? !invertZ : invertZ);
    #endif
    #ifdef C_DIR_PIN
    hal->digitalWrite(C_DIR_PIN, (dirBits & 0x08) ? !invertC : invertC);
    #endif

    // Wait for direction setup time
    hal->delayUs(STEP_IDLE_DELAY_MS);
}

// ============================================================================
// Load Next Segment
// ============================================================================

void Stepper::loadNextSegment() {
    if (!_planner) return;

    StepSegment segment;
    if (_planner->getNextSegment(segment)) {
        _currentSegment = segment;
        _hasSegment = true;
        _segmentStepCount = 0;

        // Set directions for this segment
        setDirections(segment.directionBits);

        // Set step timer frequency
        if (segment.stepFrequency > 0) {
            hal->stepTimerSetFrequency(segment.stepFrequency);
            hal->stepTimerStart();
        }

        // Reset Bresenham errors
        for (int i = 0; i < 4; i++) {
            _bresenhamError[i] = -(int32_t)(_currentSegment.bresenhamDominant / 2);
        }
    } else {
        // No more segments — stop
        _hasSegment = false;
        hal->stepTimerStop();

        if (!_planner->hasBlocks()) {
            _state = StepperState::IDLE;
            hal->segmentTimerStop();
            _lastActivityTime = hal->millis();
        }
    }
}

// ============================================================================
// Step Timer ISR — The Hot Path
// ============================================================================
// This ISR fires at the step frequency (up to ~30kHz on AVR, ~200kHz on ESP32).
// It MUST be as fast as possible. Every microsecond counts.
//
// The Bresenham algorithm distributes steps across axes proportionally:
//   For each axis:
//     error += delta[axis]
//     if error > 0:
//       step that axis
//       error -= dominant_steps

void Stepper::stepTimerCallback() {
    if (!_instance || !_instance->_hasSegment) return;

    Stepper& s = *_instance;

    // ── Step pulse generation using Bresenham's algorithm ──────────────
    // The dominant axis steps every cycle. Other axes step proportionally.

    bool stepX = false, stepY = false;
    #ifdef Z_STEP_PIN
    bool stepZ = false;
    #endif
    #ifdef C_STEP_PIN
    bool stepC = false;
    #endif

    // X axis
    s._bresenhamError[0] += s._currentSegment.bresenhamDelta[0];
    if (s._bresenhamError[0] > 0) {
        stepX = true;
        s._bresenhamError[0] -= (int32_t)s._currentSegment.bresenhamDominant;
    }

    // Y axis
    s._bresenhamError[1] += s._currentSegment.bresenhamDelta[1];
    if (s._bresenhamError[1] > 0) {
        stepY = true;
        s._bresenhamError[1] -= (int32_t)s._currentSegment.bresenhamDominant;
    }

    #ifdef Z_STEP_PIN
    // Z axis
    s._bresenhamError[2] += s._currentSegment.bresenhamDelta[2];
    if (s._bresenhamError[2] > 0) {
        stepZ = true;
        s._bresenhamError[2] -= (int32_t)s._currentSegment.bresenhamDominant;
    }
    #endif

    #ifdef C_STEP_PIN
    // C axis
    s._bresenhamError[3] += s._currentSegment.bresenhamDelta[3];
    if (s._bresenhamError[3] > 0) {
        stepC = true;
        s._bresenhamError[3] -= (int32_t)s._currentSegment.bresenhamDominant;
    }
    #endif

    // ── Set step pins HIGH ────────────────────────────────────────────
    if (stepX) hal->digitalWrite(X_STEP_PIN, true);
    if (stepY) hal->digitalWrite(Y_STEP_PIN, true);
    #ifdef Z_STEP_PIN
    if (stepZ) hal->digitalWrite(Z_STEP_PIN, true);
    #endif
    #ifdef C_STEP_PIN
    if (stepC) hal->digitalWrite(C_STEP_PIN, true);
    #endif

    // ── Minimum pulse width delay ─────────────────────────────────────
    // A4988 requires ≥1µs, DRV8825 requires ≥1.9µs.
    // We use STEP_PULSE_WIDTH_US (default 3µs) for safety.
    hal->delayUs(STEP_PULSE_WIDTH_US);

    // ── Set step pins LOW ─────────────────────────────────────────────
    if (stepX) hal->digitalWrite(X_STEP_PIN, false);
    if (stepY) hal->digitalWrite(Y_STEP_PIN, false);
    #ifdef Z_STEP_PIN
    if (stepZ) hal->digitalWrite(Z_STEP_PIN, false);
    #endif
    #ifdef C_STEP_PIN
    if (stepC) hal->digitalWrite(C_STEP_PIN, false);
    #endif

    // ── Advance segment step counter ──────────────────────────────────
    s._segmentStepCount++;
    if (s._segmentStepCount >= s._currentSegment.stepsPerSegment) {
        // This segment is complete — ISR will be paused until next segment loads
        s._hasSegment = false;
        hal->stepTimerStop();
    }
}

// ============================================================================
// Segment Preparation Timer Callback (~1kHz)
// ============================================================================
// Called every ~1ms. Checks if the ISR needs a new segment and prepares one
// from the planner's motion block buffer.

void Stepper::segmentTimerCallback() {
    if (!_instance || !_instance->_planner) return;

    Stepper& s = *_instance;

    // If the ISR has consumed its segment, load the next one
    if (!s._hasSegment && s._state == StepperState::RUNNING) {
        // Ask the planner to prepare the next segment
        s._planner->prepareNextSegment();

        // Try to load it
        s.loadNextSegment();
    }
}

// ============================================================================
// Idle Timer Management
// ============================================================================

void Stepper::updateIdleTimer() {
    if (_state == StepperState::IDLE && _motorsEnabled) {
        uint32_t elapsed = hal->millis() - _lastActivityTime;
        if (elapsed > (uint32_t)MOTOR_IDLE_TIMEOUT_SEC * 1000UL) {
            enableMotors(false);
            LOG_INFO("Stepper: Motors disabled (idle timeout)");
        }
    }
}
