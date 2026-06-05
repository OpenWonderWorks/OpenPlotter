/**
 * ============================================================================
 * OpenPlotter — Tool Controller Interface & Implementations
 * ============================================================================
 */

#ifndef TOOL_CONTROLLER_H
#define TOOL_CONTROLLER_H

#include "../hal/hal.h"
#include "config.h"
#include <stdint.h>

/**
 * Abstract tool controller interface.
 * All tool types (servo pen, solenoid blade, tangential knife) implement this.
 */
class ToolController {
public:
    virtual ~ToolController() = default;

    virtual void init() = 0;
    virtual void toolDown(float pressure = -1.0f) = 0;  // Engage tool (pen down / blade down)
    virtual void toolUp() = 0;                            // Disengage tool
    virtual void setPressure(float pressure) = 0;         // 0-255 pressure level
    virtual bool isDown() const = 0;
    virtual float getPressure() const = 0;
};

#endif // TOOL_CONTROLLER_H
