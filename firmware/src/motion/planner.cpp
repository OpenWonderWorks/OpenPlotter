/**
 * ============================================================================
 * OpenPlotter — Motion Planner Implementation
 * ============================================================================
 *
 * Implements the look-ahead motion planner with:
 *   1. Block planning: Convert target positions to step counts
 *   2. Velocity profiling: Calculate entry/exit speeds with junction deviation
 *   3. Trapezoidal profile: Compute accel/cruise/decel phases
 *   4. Segment preparation: Break blocks into ISR-consumable segments
 *   5. Arc linearization: Convert G2/G3 arcs to line segments
 *
 * ============================================================================
 */

#include "planner.h"
#include "../utils/math_utils.h"
#include "../utils/logger.h"
#include <string.h>
#include <math.h>

// ── Arc linearization parameters ────────────────────────────────────────────
#define ARC_TOLERANCE       0.002f      // mm — max deviation from true arc
#define ARC_SEGMENTS_MIN    4           // Minimum segments per arc
#define ARC_SEGMENTS_MAX    360         // Maximum segments per arc

// ============================================================================
// Constructor & Init
// ============================================================================

Planner::Planner() {
    reset();
}

void Planner::init(float stepsPerMm[4], float maxRate[4], float acceleration,
                   float junctionDeviation) {
    for (int i = 0; i < 4; i++) {
        _stepsPerMm[i] = stepsPerMm[i];
        _maxRate[i] = maxRate[i];
    }
    _acceleration = acceleration;
    _junctionDeviation = junctionDeviation;
}

void Planner::reset() {
    _blockBuffer.clear();
    _segmentBuffer.clear();
    memset(_positionSteps, 0, sizeof(_positionSteps));
    memset(_prevUnitVec, 0, sizeof(_prevUnitVec));
    _hasPrevBlock = false;
    _currentSegmentStep = 0;
    _feedOverride = 100;
}

// ============================================================================
// Position Management
// ============================================================================

void Planner::getPosition(float pos[4]) const {
    for (int i = 0; i < 4; i++) {
        pos[i] = (float)_positionSteps[i] / _stepsPerMm[i];
    }
}

void Planner::setPosition(float pos[4]) {
    for (int i = 0; i < 4; i++) {
        _positionSteps[i] = (int32_t)(pos[i] * _stepsPerMm[i]);
    }
}

void Planner::setPositionSteps(int32_t steps[4]) {
    memcpy(_positionSteps, steps, sizeof(_positionSteps));
}

// ============================================================================
// Settings Updates
// ============================================================================

void Planner::setStepsPerMm(uint8_t axis, float value) {
    if (axis < 4) _stepsPerMm[axis] = value;
}

void Planner::setMaxRate(uint8_t axis, float value) {
    if (axis < 4) _maxRate[axis] = value;
}

void Planner::setAcceleration(float value) { _acceleration = value; }
void Planner::setJunctionDeviation(float value) { _junctionDeviation = value; }
void Planner::setFeedOverride(uint8_t percent) {
    _feedOverride = MathUtils::clamp(percent, (uint8_t)10, (uint8_t)200);
}

// ============================================================================
// Plan Linear Move
// ============================================================================

bool Planner::planLinearMove(float target[4], float feedRate, bool isRapid,
                              bool toolOn, float toolPressure) {
    if (_blockBuffer.isFull()) {
        return false; // Buffer full — caller should retry later
    }

    MotionBlock block;

    // Convert target position from mm to steps
    int32_t targetSteps[4];
    float deltaFMm[4];
    float unitVec[4] = {0};
    float distanceSqMm = 0.0f;

    for (int i = 0; i < 4; i++) {
        targetSteps[i] = (int32_t)roundf(target[i] * _stepsPerMm[i]);
        block.deltaSteps[i] = targetSteps[i] - _positionSteps[i];
        block.targetSteps[i] = targetSteps[i];

        deltaFMm[i] = (float)block.deltaSteps[i] / _stepsPerMm[i];
        distanceSqMm += deltaFMm[i] * deltaFMm[i];

        // Set direction bits
        if (block.deltaSteps[i] < 0) {
            block.directionBits |= (1 << i);
            block.deltaSteps[i] = -block.deltaSteps[i]; // Store absolute
        }
    }

    block.distanceMm = sqrtf(distanceSqMm);

    // Skip zero-length moves
    if (block.distanceMm < 0.0001f) return true;

    // Compute unit vector (for junction speed calculation)
    for (int i = 0; i < 4; i++) {
        unitVec[i] = deltaFMm[i] / block.distanceMm;
    }

    // Find the dominant axis (most steps)
    block.totalSteps = 0;
    for (int i = 0; i < 4; i++) {
        if ((uint32_t)block.deltaSteps[i] > block.totalSteps) {
            block.totalSteps = (uint32_t)block.deltaSteps[i];
        }
    }

    // Calculate nominal speed (steps/sec)
    // Apply feed rate override
    float effectiveFeedRate = feedRate * (float)_feedOverride / 100.0f;

    if (isRapid) {
        // G0: Use maximum rate for each axis, compute resultant speed
        float maxSpeedMmSec = 1e6f;
        for (int i = 0; i < 4; i++) {
            if (fabsf(unitVec[i]) > 0.001f) {
                float axisMax = _maxRate[i] / 60.0f; // mm/min → mm/sec
                float limited = axisMax / fabsf(unitVec[i]);
                if (limited < maxSpeedMmSec) maxSpeedMmSec = limited;
            }
        }
        block.nominalSpeed = maxSpeedMmSec * (block.totalSteps / block.distanceMm);
    } else {
        // G1: Use specified feed rate
        float speedMmSec = effectiveFeedRate / 60.0f; // mm/min → mm/sec
        // Limit to per-axis max rates
        for (int i = 0; i < 4; i++) {
            if (fabsf(unitVec[i]) > 0.001f) {
                float axisMax = _maxRate[i] / 60.0f;
                float limited = axisMax / fabsf(unitVec[i]);
                if (speedMmSec > limited) speedMmSec = limited;
            }
        }
        block.nominalSpeed = speedMmSec * (block.totalSteps / block.distanceMm);
    }

    block.feedRate = effectiveFeedRate;
    block.isRapid = isRapid;
    block.toolOn = toolOn;
    block.toolPressure = toolPressure;

    // Acceleration in steps/sec²
    block.acceleration = _acceleration * (block.totalSteps / block.distanceMm);

    // Calculate maximum entry speed using junction deviation
    if (_hasPrevBlock) {
        block.maxEntrySpeed = calcJunctionSpeed(block, _prevUnitVec);
    } else {
        block.maxEntrySpeed = 0.0f;
    }

    block.entrySpeed = MathUtils::min2(block.maxEntrySpeed, block.nominalSpeed);
    block.exitSpeed = 0.0f; // Will be recalculated by look-ahead
    block.recalculate = true;

    // Add block to buffer
    _blockBuffer.push(block);

    // Update position
    for (int i = 0; i < 4; i++) {
        _positionSteps[i] = targetSteps[i];
    }

    // Save unit vector for next junction
    memcpy(_prevUnitVec, unitVec, sizeof(_prevUnitVec));
    _hasPrevBlock = true;

    // Run the look-ahead recalculation
    recalculateTrapezoidal();

    LOG_DEBUG_PLANNER("Block: dist=%.2fmm steps=%lu speed=%.0f",
                       block.distanceMm, block.totalSteps, block.nominalSpeed);

    return true;
}

// ============================================================================
// Arc Planning (G2/G3 → linearized segments)
// ============================================================================

bool Planner::planArcMove(float target[2], float offset[2], bool isClockwise,
                           float feedRate, bool toolOn, float toolPressure) {
    // Get current position in mm
    float currentPos[4];
    getPosition(currentPos);

    float startX = currentPos[0];
    float startY = currentPos[1];
    float endX = target[0];
    float endY = target[1];

    // Center of arc
    float centerX = startX + offset[0];
    float centerY = startY + offset[1];

    // Radius
    float radius = MathUtils::hypot2(offset[0], offset[1]);
    if (radius < 0.001f) return false;

    // Start and end angles
    float startAngle = atan2f(startY - centerY, startX - centerX);
    float endAngle = atan2f(endY - centerY, endX - centerX);

    // Calculate sweep angle
    float sweep;
    if (isClockwise) {
        sweep = startAngle - endAngle;
        if (sweep <= 0.0f) sweep += MathUtils::TWO_PI_F;
    } else {
        sweep = endAngle - startAngle;
        if (sweep <= 0.0f) sweep += MathUtils::TWO_PI_F;
    }

    // Number of segments based on arc tolerance
    float segments = fabsf(sweep * radius) / sqrtf(2.0f * ARC_TOLERANCE * radius);
    int numSegments = MathUtils::clamp((int)ceilf(segments),
                                        ARC_SEGMENTS_MIN, ARC_SEGMENTS_MAX);

    float angleIncrement = sweep / (float)numSegments;
    if (isClockwise) angleIncrement = -angleIncrement;

    // Generate line segments
    float targetPos[4] = {currentPos[0], currentPos[1], currentPos[2], currentPos[3]};

    for (int seg = 1; seg <= numSegments; seg++) {
        float angle = startAngle + angleIncrement * (float)seg;

        if (seg == numSegments) {
            // Last segment: snap exactly to target
            targetPos[0] = endX;
            targetPos[1] = endY;
        } else {
            targetPos[0] = centerX + radius * cosf(angle);
            targetPos[1] = centerY + radius * sinf(angle);
        }

        if (!planLinearMove(targetPos, feedRate, false, toolOn, toolPressure)) {
            return false; // Buffer full
        }
    }

    return true;
}

// ============================================================================
// Junction Speed Calculation
// ============================================================================
// Calculates the maximum entry speed for a block based on the angle
// between this block's direction and the previous block's direction.
// Uses the junction deviation method (similar to Grbl/Marlin).

float Planner::calcJunctionSpeed(const MotionBlock& block,
                                  const float prevUnitVec[4]) {
    // Calculate the cosine of the angle between the two direction vectors
    float cosTheta = 0.0f;
    float blockUnitVec[4] = {0};
    float distSteps = (float)block.totalSteps;

    for (int i = 0; i < 4; i++) {
        blockUnitVec[i] = (block.directionBits & (1 << i))
                          ? -(float)block.deltaSteps[i] / distSteps
                          : (float)block.deltaSteps[i] / distSteps;
        cosTheta += prevUnitVec[i] * blockUnitVec[i];
    }

    // Clamp to valid range
    cosTheta = MathUtils::clamp(cosTheta, -1.0f, 1.0f);

    // If vectors are pointing the same direction (cosTheta ≈ 1), max junction speed
    if (cosTheta > 0.9999f) {
        return block.nominalSpeed;
    }

    // If vectors are pointing opposite directions (cosTheta ≈ -1), full stop
    if (cosTheta < -0.9999f) {
        return 0.0f;
    }

    // Junction deviation formula:
    // v_junction = sqrt(deviation * acceleration * (1 - cos(theta)) / sin(theta/2))
    float sinHalfTheta = sqrtf(0.5f * (1.0f - cosTheta));
    if (sinHalfTheta < 0.001f) return block.nominalSpeed;

    float junctionSpeed = sqrtf(_acceleration * _junctionDeviation *
                                sinHalfTheta / (1.0f - sinHalfTheta));

    return MathUtils::min2(junctionSpeed, block.nominalSpeed);
}

// ============================================================================
// Look-Ahead Recalculation (Forward + Reverse Pass)
// ============================================================================

void Planner::recalculateTrapezoidal() {
    uint8_t count = _blockBuffer.count();
    if (count == 0) return;

    // Reverse pass: Set exit speeds from back to front
    // The last block always exits at 0 (decelerate to stop)
    reversePass();

    // Forward pass: Set entry speeds from front to back
    forwardPass();

    // Compute trapezoidal profile for each block
    for (uint8_t i = 0; i < count; i++) {
        MotionBlock* block = _blockBuffer.peekAt(i);
        if (block && block->recalculate) {
            computeTrapezoid(*block);
            block->recalculate = false;
        }
    }
}

void Planner::reversePass() {
    uint8_t count = _blockBuffer.count();
    if (count == 0) return;

    // Last block exits at 0
    MotionBlock* lastBlock = _blockBuffer.peekAt(count - 1);
    if (lastBlock) lastBlock->exitSpeed = 0.0f;

    // Work backwards
    for (int i = (int)count - 1; i > 0; i--) {
        MotionBlock* current = _blockBuffer.peekAt(i);
        MotionBlock* prev = _blockBuffer.peekAt(i - 1);
        if (!current || !prev) continue;

        // If current block can't reach its entry speed from its exit speed,
        // reduce the exit speed of the previous block.
        float maxEntry = maxAllowableSpeed(current->acceleration,
                                           current->exitSpeed,
                                           current->distanceMm * (current->totalSteps / current->distanceMm));
        if (current->entrySpeed > maxEntry) {
            current->entrySpeed = maxEntry;
            current->recalculate = true;
        }

        // The exit speed of the previous block = entry speed of current block
        prev->exitSpeed = current->entrySpeed;
        prev->recalculate = true;
    }
}

void Planner::forwardPass() {
    uint8_t count = _blockBuffer.count();
    if (count < 2) return;

    for (uint8_t i = 0; i < count - 1; i++) {
        MotionBlock* current = _blockBuffer.peekAt(i);
        MotionBlock* next = _blockBuffer.peekAt(i + 1);
        if (!current || !next) continue;

        // Check if current block can accelerate to the needed exit speed
        float maxExit = maxAllowableSpeed(current->acceleration,
                                          current->entrySpeed,
                                          current->distanceMm * (current->totalSteps / current->distanceMm));

        if (current->exitSpeed > maxExit) {
            current->exitSpeed = maxExit;
            current->recalculate = true;
        }

        // Propagate: next block's entry speed = current block's exit speed
        // (but not exceeding the junction limit)
        float nextEntry = MathUtils::min2(current->exitSpeed, next->maxEntrySpeed);
        if (next->entrySpeed != nextEntry) {
            next->entrySpeed = nextEntry;
            next->recalculate = true;
        }
    }
}

// ============================================================================
// Trapezoidal Profile Computation
// ============================================================================

void Planner::computeTrapezoid(MotionBlock& block) {
    // Calculate acceleration and deceleration distances in steps
    // Using kinematic equation: v² = v₀² + 2*a*d
    //   d = (v² - v₀²) / (2*a)

    float accelDistSteps = 0.0f;
    float decelDistSteps = 0.0f;

    if (block.acceleration > 0.0f) {
        accelDistSteps = (block.nominalSpeed * block.nominalSpeed -
                          block.entrySpeed * block.entrySpeed) /
                         (2.0f * block.acceleration);

        decelDistSteps = (block.nominalSpeed * block.nominalSpeed -
                          block.exitSpeed * block.exitSpeed) /
                         (2.0f * block.acceleration);
    }

    // Check if we can reach nominal speed
    float totalRampSteps = accelDistSteps + decelDistSteps;

    if (totalRampSteps > (float)block.totalSteps) {
        // Can't reach nominal speed — it's a triangular profile
        // Find the intersection point
        float intersectSteps = ((float)block.totalSteps +
                                (block.entrySpeed * block.entrySpeed -
                                 block.exitSpeed * block.exitSpeed) /
                                (2.0f * block.acceleration)) / 2.0f;

        block.accelerateSteps = MathUtils::clamp((uint32_t)intersectSteps,
                                                  (uint32_t)0, block.totalSteps);
        block.decelerateSteps = block.totalSteps - block.accelerateSteps;
        block.cruiseSteps = 0;
    } else {
        // Trapezoidal profile
        block.accelerateSteps = (uint32_t)ceilf(accelDistSteps);
        block.decelerateSteps = (uint32_t)ceilf(decelDistSteps);

        if (block.accelerateSteps + block.decelerateSteps > block.totalSteps) {
            block.decelerateSteps = block.totalSteps - block.accelerateSteps;
        }

        block.cruiseSteps = block.totalSteps - block.accelerateSteps - block.decelerateSteps;
    }
}

// ============================================================================
// Max Allowable Speed
// ============================================================================
// Given starting velocity, acceleration, and distance, compute the max
// achievable velocity: v² = v₀² + 2*a*d

float Planner::maxAllowableSpeed(float acceleration, float targetVelocity,
                                  float distance) {
    return sqrtf(targetVelocity * targetVelocity + 2.0f * acceleration * distance);
}

// ============================================================================
// Segment Preparation
// ============================================================================
// Called by the segment timer (~1kHz) to prepare step segments from
// motion blocks. Each segment represents a fixed number of steps at
// a specific frequency, pre-calculated for the ISR to consume.

void Planner::prepareNextSegment() {
    if (_segmentBuffer.isFull()) return;
    if (_blockBuffer.isEmpty()) return;

    MotionBlock* block = _blockBuffer.peek();
    if (!block) return;

    StepSegment segment;

    // Determine current phase and speed
    float currentSpeed;
    if (_currentSegmentStep < block->accelerateSteps) {
        // Acceleration phase
        float progress = (float)_currentSegmentStep / (float)block->accelerateSteps;
        currentSpeed = block->entrySpeed +
                       (block->nominalSpeed - block->entrySpeed) * progress;
    } else if (_currentSegmentStep < block->accelerateSteps + block->cruiseSteps) {
        // Cruise phase
        currentSpeed = block->nominalSpeed;
    } else {
        // Deceleration phase
        uint32_t decelStep = _currentSegmentStep -
                             block->accelerateSteps - block->cruiseSteps;
        float progress = (float)decelStep / (float)block->decelerateSteps;
        currentSpeed = block->nominalSpeed -
                       (block->nominalSpeed - block->exitSpeed) * progress;
    }

    // Clamp speed
    if (currentSpeed < 10.0f) currentSpeed = 10.0f;

    // Calculate steps for this segment
    // We aim for ~1ms worth of steps per segment
    uint32_t stepsThisSegment = (uint32_t)(currentSpeed / 1000.0f); // steps per ms
    if (stepsThisSegment < 1) stepsThisSegment = 1;

    // Don't exceed remaining steps in the block
    uint32_t remainingSteps = block->totalSteps - _currentSegmentStep;
    if (stepsThisSegment > remainingSteps) {
        stepsThisSegment = remainingSteps;
    }

    segment.stepsPerSegment = stepsThisSegment;
    segment.stepFrequency = (uint32_t)currentSpeed;
    segment.directionBits = block->directionBits;

    // Set up Bresenham counters for multi-axis synchronization
    segment.bresenhamDominant = block->totalSteps;
    for (int i = 0; i < 4; i++) {
        segment.bresenhamDelta[i] = block->deltaSteps[i];
        segment.bresenhamError[i] = -(int32_t)(block->totalSteps / 2);
    }

    // Check if this is the last segment of the block
    _currentSegmentStep += stepsThisSegment;
    if (_currentSegmentStep >= block->totalSteps) {
        segment.lastSegment = true;
        _currentSegmentStep = 0;

        // Remove completed block from buffer
        MotionBlock discarded;
        _blockBuffer.pop(discarded);
    } else {
        segment.lastSegment = false;
    }

    _segmentBuffer.push(segment);
}

bool Planner::getNextSegment(StepSegment& segment) {
    return _segmentBuffer.pop(segment);
}
