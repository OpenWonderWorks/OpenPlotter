/**
 * ============================================================================
 * OpenPlotter — Machine Status Implementation
 * ============================================================================
 */

#include "status.h"
#include <stdio.h>
#include <string.h>

Status::Status() {}

void Status::init(Stepper* stepper, Planner* planner, ToolController* tool,
                  HomingManager* homing) {
    _stepper = stepper;
    _planner = planner;
    _tool = tool;
    _homing = homing;
}

const char* Status::getStateString() const {
    switch (_state) {
        case MachineState::IDLE:   return "Idle";
        case MachineState::RUN:    return "Run";
        case MachineState::HOLD:   return "Hold";
        case MachineState::HOMING: return "Home";
        case MachineState::ALARM:  return "Alarm";
        case MachineState::CHECK:  return "Check";
        case MachineState::SLEEP:  return "Sleep";
        default:                   return "Unknown";
    }
}

void Status::formatStatusReport(char* buf, size_t bufSize) {
    float pos[4] = {0};
    if (_planner) _planner->getPosition(pos);

    uint8_t bufferAvail = _planner ? _planner->availableBlocks() : 0;
    float feedRate = 0; // TODO: get from stepper
    bool toolOn = _tool ? _tool->isDown() : false;

    snprintf(buf, bufSize,
             "%s|MPos:%.3f,%.3f,%.3f|Bf:%d|FS:%.0f,%d",
             getStateString(),
             pos[0], pos[1], pos[2],
             bufferAvail,
             feedRate,
             toolOn ? 1 : 0);
}

void Status::setAlarm(uint8_t code) {
    _state = MachineState::ALARM;
    _alarmCode = code;
}

void Status::clearAlarm() {
    _alarmCode = 0;
    _state = MachineState::IDLE;
}

void Status::update() {
    if (_state == MachineState::ALARM) return; // Don't auto-update from alarm

    if (_stepper) {
        switch (_stepper->getState()) {
            case StepperState::RUNNING:
                _state = MachineState::RUN;
                break;
            case StepperState::HOLD:
                _state = MachineState::HOLD;
                break;
            case StepperState::HOMING:
                _state = MachineState::HOMING;
                break;
            case StepperState::IDLE:
                _state = MachineState::IDLE;
                break;
        }
    }
}
