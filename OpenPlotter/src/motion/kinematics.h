/**
 * ============================================================================
 * OpenPlotter — Kinematics (Coordinate Transforms)
 * ============================================================================
 */

#ifndef KINEMATICS_H
#define KINEMATICS_H

#include "../gcode/commands.h"
#include "../../openplotter_config.h"
#include "../utils/math_utils.h"

/**
 * Handles coordinate system transformations:
 *   - Absolute/relative positioning
 *   - Inch/mm conversion
 *   - G92 position overrides
 *   - Work coordinate offsets (G10 L2)
 */
class Kinematics {
public:
    Kinematics();

    /**
     * Convert G-code target coordinates to absolute machine coordinates (mm).
     * Handles absolute/relative mode, inch/mm conversion, and offsets.
     *
     * @param block    Parsed G-code block with target values
     * @param modal    Current modal state
     * @param currentPos Current machine position in mm [X, Y, Z, C]
     * @param target   Output: absolute target position in mm [X, Y, Z, C]
     */
    void resolveTarget(const GCodeBlock& block, const ModalState& modal,
                        const float currentPos[4], float target[4]);

    /**
     * Apply G92 position override.
     * Sets the current position to the specified coordinates without moving.
     */
    void setG92Offset(const GCodeBlock& block, const float currentPos[4]);

    /**
     * Clear G92 offset (reset to zero).
     */
    void clearG92Offset();

    /**
     * Set work coordinate offset (G10 L2 P0 X.. Y..).
     */
    void setWorkOffset(const GCodeBlock& block);

    /**
     * Get the combined offset (work + G92).
     */
    void getOffset(float offset[4]) const;

private:
    // G92 offset: difference between reported position and machine position
    float _g92Offset[4] = {0};

    // Work coordinate offset (from G10 L2)
    float _workOffset[4] = {0};
};

#endif // KINEMATICS_H
