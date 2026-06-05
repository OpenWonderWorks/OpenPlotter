/**
 * ============================================================================
 * OpenPlotter — Motion Planner Header
 * ============================================================================
 *
 * Look-ahead motion planner with trapezoidal velocity profiling.
 * Converts parsed G-code commands into velocity-profiled motion blocks
 * that the stepper engine consumes.
 *
 * ============================================================================
 */

#ifndef PLANNER_H
#define PLANNER_H

#include "../gcode/commands.h"
#include "../utils/ring_buffer.h"
#include "config.h"
#include <stdint.h>

// ── Motion Block ────────────────────────────────────────────────────────────
// Represents one planned movement segment with acceleration profile.
struct MotionBlock {
    // Target position in steps (absolute from machine zero)
    int32_t targetSteps[4] = {0};   // X, Y, Z, C

    // Delta steps for this block (signed: positive = forward)
    int32_t deltaSteps[4] = {0};    // Absolute step count per axis

    // Direction bits (bit 0 = X, bit 1 = Y, bit 2 = Z, bit 3 = C)
    uint8_t directionBits = 0;

    // Step counts (absolute values)
    uint32_t totalSteps = 0;        // Max step count among all axes (dominant axis)

    // Velocity profile (in steps/sec)
    float entrySpeed = 0.0f;        // Entry speed for this block
    float nominalSpeed = 0.0f;      // Cruise speed (max speed for this block)
    float exitSpeed = 0.0f;         // Exit speed (entry speed of next block)
    float maxEntrySpeed = 0.0f;     // Maximum possible entry speed (from junction calc)
    float acceleration = 0.0f;      // Acceleration in steps/sec²

    // Distance in mm
    float distanceMm = 0.0f;

    // Feed rate (mm/min)
    float feedRate = 0.0f;

    // Acceleration ramp step counts
    uint32_t accelerateSteps = 0;   // Steps in acceleration phase
    uint32_t decelerateSteps = 0;   // Steps in deceleration phase
    uint32_t cruiseSteps = 0;       // Steps at nominal speed

    // Flags
    bool isRapid = false;           // G0 rapid move (max speed, no cutting)
    bool recalculate = true;        // Needs velocity profile recalculation

    // Tool state during this block
    bool toolOn = false;
    float toolPressure = 0.0f;
};

// ── Step Segment ────────────────────────────────────────────────────────────
// A smaller piece of a motion block, pre-calculated for the ISR.
// Each segment represents a fixed number of steps at a specific frequency.
struct StepSegment {
    uint32_t stepsPerSegment = 0;   // Steps to execute in this segment
    uint32_t stepFrequency = 0;     // Step frequency (Hz) for the timer
    uint8_t directionBits = 0;      // Direction for each axis

    // Bresenham counters for multi-axis sync
    int32_t bresenhamError[4] = {0};
    int32_t bresenhamDelta[4] = {0};
    uint32_t bresenhamDominant = 0;  // Dominant axis step count

    bool lastSegment = false;       // Is this the last segment of a block?
};

// ── Planner Class ───────────────────────────────────────────────────────────
class Planner {
public:
    Planner();

    /**
     * Initialize the planner with machine settings.
     */
    void init(float stepsPerMm[4], float maxRate[4], float acceleration,
              float junctionDeviation);

    /**
     * Plan a linear move.
     * @param target Target position in mm (absolute or relative based on modal)
     * @param feedRate Feed rate in mm/min
     * @param isRapid True if G0 (rapid), false if G1 (controlled)
     * @param toolOn Whether the tool is engaged
     * @param toolPressure Tool pressure value
     * @return true if block was added, false if buffer is full
     */
    bool planLinearMove(float target[4], float feedRate, bool isRapid,
                        bool toolOn, float toolPressure);

    /**
     * Plan an arc move (G2/G3).
     * Internally linearizes the arc into small line segments.
     * @param target End point in mm
     * @param offset I/J center offset from start
     * @param isClockwise True = G2, False = G3
     * @param feedRate Feed rate in mm/min
     * @param toolOn Whether the tool is engaged
     * @param toolPressure Tool pressure value
     * @return true if all segments were added
     */
    bool planArcMove(float target[2], float offset[2], bool isClockwise,
                     float feedRate, bool toolOn, float toolPressure);

    /**
     * Get the next prepared step segment for the ISR.
     * Called by the segment preparation timer (~1kHz).
     * @param segment Output: the next segment
     * @return true if a segment is available, false if buffer is empty
     */
    bool getNextSegment(StepSegment& segment);

    /**
     * Prepare the next step segment from the motion block buffer.
     * Called by the segment preparation timer callback.
     */
    void prepareNextSegment();

    /**
     * @return true if the planner has blocks waiting to be executed.
     */
    bool hasBlocks() const { return !_blockBuffer.isEmpty(); }

    /**
     * @return number of free block slots in the planner buffer.
     */
    uint8_t availableBlocks() const { return _blockBuffer.available(); }

    /**
     * @return true if the planner buffer is full.
     */
    bool isFull() const { return _blockBuffer.isFull(); }

    /**
     * Get current machine position in mm.
     */
    void getPosition(float pos[4]) const;

    /**
     * Set current machine position in mm (e.g., after homing).
     */
    void setPosition(float pos[4]);

    /**
     * Set current position in steps (after homing).
     */
    void setPositionSteps(int32_t steps[4]);

    /**
     * Reset the planner (clear all blocks, reset position).
     */
    void reset();

    /**
     * Update machine settings.
     */
    void setStepsPerMm(uint8_t axis, float value);
    void setMaxRate(uint8_t axis, float value);
    void setAcceleration(float value);
    void setJunctionDeviation(float value);

    /**
     * Feed rate override (percentage: 100 = normal, 50 = half, 200 = double)
     */
    void setFeedOverride(uint8_t percent);
    uint8_t getFeedOverride() const { return _feedOverride; }

private:
    // Block buffer
    RingBuffer<MotionBlock, PLANNER_BUFFER_SIZE> _blockBuffer;

    // Segment buffer (consumed by the stepper ISR)
    RingBuffer<StepSegment, SEGMENT_BUFFER_SIZE> _segmentBuffer;

    // Current machine position in steps (absolute from home)
    int32_t _positionSteps[4] = {0};

    // Machine parameters
    float _stepsPerMm[4] = {80, 80, 400, 17.778f};
    float _maxRate[4] = {5000, 5000, 1000, 3600};  // mm/min (deg/min for C)
    float _acceleration = 500.0f;       // mm/s²
    float _junctionDeviation = 0.01f;   // mm

    // Feed rate override
    uint8_t _feedOverride = 100;

    // Previous block's unit vector (for junction speed calculation)
    float _prevUnitVec[4] = {0};
    bool _hasPrevBlock = false;

    // Segment preparation state
    uint32_t _currentSegmentStep = 0;
    uint8_t _currentBlockIndex = 0;

    // Internal methods
    void recalculateTrapezoidal();
    float calcJunctionSpeed(const MotionBlock& block, const float prevUnitVec[4]);
    void computeTrapezoid(MotionBlock& block);
    float maxAllowableSpeed(float acceleration, float targetVelocity, float distance);
    void forwardPass();
    void reversePass();
};

#endif // PLANNER_H
