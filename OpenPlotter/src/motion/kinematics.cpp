/**
 * ============================================================================
 * OpenPlotter — Kinematics Implementation
 * ============================================================================
 */

#include "kinematics.h"
#include <string.h>
#include <math.h>

Kinematics::Kinematics() {
    clearG92Offset();
    memset(_workOffset, 0, sizeof(_workOffset));
}

void Kinematics::resolveTarget(const GCodeBlock& block, const ModalState& modal,
                                const float currentPos[4], float target[4]) {
    // Start with current position
    for (int i = 0; i < 4; i++) {
        target[i] = currentPos[i];
    }

    // Extract raw values from block
    float rawValues[4] = {0};
    bool hasValue[4] = {false};

    if (block.hasX) { rawValues[0] = block.x; hasValue[0] = true; }
    if (block.hasY) { rawValues[1] = block.y; hasValue[1] = true; }
    if (block.hasZ) { rawValues[2] = block.z; hasValue[2] = true; }
    if (block.hasC) { rawValues[3] = block.c; hasValue[3] = true; }

    // Apply inch → mm conversion if needed
    if (modal.inchMode) {
        for (int i = 0; i < 4; i++) {
            if (hasValue[i]) {
                rawValues[i] *= MathUtils::MM_PER_INCH;
            }
        }
    }

    // Apply absolute or relative positioning
    for (int i = 0; i < 4; i++) {
        if (!hasValue[i]) continue;

        if (modal.absoluteMode) {
            // Absolute: target = commanded value + offsets
            target[i] = rawValues[i] + _g92Offset[i] + _workOffset[i];
        } else {
            // Relative: target = current + delta
            target[i] = currentPos[i] + rawValues[i];
        }
    }
}

void Kinematics::setG92Offset(const GCodeBlock& block, const float currentPos[4]) {
    // G92 X10 Y20 means: "I want the current position to be called (10, 20)"
    // Offset = currentPos - newPos

    if (block.hasX) _g92Offset[0] = currentPos[0] - block.x;
    if (block.hasY) _g92Offset[1] = currentPos[1] - block.y;
    if (block.hasZ) _g92Offset[2] = currentPos[2] - block.z;
    if (block.hasC) _g92Offset[3] = currentPos[3] - block.c;

    // If no axes specified, reset all offsets
    if (!block.hasX && !block.hasY && !block.hasZ && !block.hasC) {
        clearG92Offset();
    }
}

void Kinematics::clearG92Offset() {
    memset(_g92Offset, 0, sizeof(_g92Offset));
}

void Kinematics::setWorkOffset(const GCodeBlock& block) {
    // G10 L2 P0 X10 Y20 sets the work offset
    if (block.hasX) _workOffset[0] = block.x;
    if (block.hasY) _workOffset[1] = block.y;
    if (block.hasZ) _workOffset[2] = block.z;
    if (block.hasC) _workOffset[3] = block.c;
}

void Kinematics::getOffset(float offset[4]) const {
    for (int i = 0; i < 4; i++) {
        offset[i] = _g92Offset[i] + _workOffset[i];
    }
}
