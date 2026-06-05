/**
 * ============================================================================
 * OpenPlotter — Machine Status & Reporting
 * ============================================================================
 */

#ifndef STATUS_H
#define STATUS_H

#include "../hal/hal.h"
#include "../motion/stepper.h"
#include "../motion/planner.h"
#include "../tools/tool_controller.h"
#include "../homing/homing.h"
#include "../../config.h"

// ── Machine State ───────────────────────────────────────────────────────────
enum class MachineState : uint8_t {
    IDLE,
    RUN,
    HOLD,
    HOMING,
    ALARM,
    CHECK,      // G-code check mode (parse but don't execute)
    SLEEP
};

class Status {
public:
    Status();
    void init(Stepper* stepper, Planner* planner, ToolController* tool,
              HomingManager* homing);

    /**
     * Get the current machine state as a string.
     */
    const char* getStateString() const;

    /**
     * Format a Grbl-compatible status report.
     * Format: <State|MPos:x,y,z|Bf:blocks,rxbytes|FS:feed,spindle>
     */
    void formatStatusReport(char* buf, size_t bufSize);

    /**
     * Set machine state.
     */
    void setState(MachineState state) { _state = state; }
    MachineState getState() const { return _state; }

    /**
     * Set alarm state with code.
     */
    void setAlarm(uint8_t code);
    void clearAlarm();
    bool isAlarmed() const { return _state == MachineState::ALARM; }
    uint8_t getAlarmCode() const { return _alarmCode; }

    /**
     * Auto-update state based on stepper/planner status.
     */
    void update();

private:
    MachineState _state = MachineState::IDLE;
    uint8_t _alarmCode = 0;

    Stepper* _stepper = nullptr;
    Planner* _planner = nullptr;
    ToolController* _tool = nullptr;
    HomingManager* _homing = nullptr;
};

#endif // STATUS_H
