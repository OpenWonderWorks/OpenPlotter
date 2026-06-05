/**
 * ============================================================================
 * OpenPlotter — Hardware Abstraction Layer Interface
 * ============================================================================
 *
 * All hardware access goes through this interface. The firmware core calls
 * HAL methods, and platform-specific implementations (AVR, ESP32) handle
 * the actual register/peripheral access.
 *
 * This allows the SAME motion planner, G-code parser, and tool controller
 * code to run on ANY supported microcontroller.
 *
 * ============================================================================
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>

// ── Pin Modes ───────────────────────────────────────────────────────────────
enum class PinMode : uint8_t {
    INPUT_MODE,
    OUTPUT_MODE,
    INPUT_PULLUP_MODE
};

// ── Interrupt Trigger ───────────────────────────────────────────────────────
enum class InterruptTrigger : uint8_t {
    TRIG_RISING,
    TRIG_FALLING,
    TRIG_CHANGE
};

// ── Timer ID ────────────────────────────────────────────────────────────────
enum class TimerID : uint8_t {
    STEP_TIMER,         // Primary stepper pulse timer
    SEGMENT_TIMER       // Segment preparation timer
};

/**
 * Hardware Abstraction Layer — Abstract Interface
 *
 * Platform-specific implementations must override all pure virtual methods.
 */
class HAL {
public:
    virtual ~HAL() = default;

    // ── Lifecycle ───────────────────────────────────────────────────────────
    virtual void init() = 0;

    // ── GPIO ────────────────────────────────────────────────────────────────
    virtual void pinMode(uint8_t pin, PinMode mode) = 0;
    virtual void digitalWrite(uint8_t pin, bool value) = 0;
    virtual bool digitalRead(uint8_t pin) = 0;

    // ── PWM / Analog ────────────────────────────────────────────────────────
    virtual void analogWrite(uint8_t pin, uint8_t value) = 0;  // 0-255
    virtual uint16_t analogRead(uint8_t pin) = 0;               // 0-1023 (AVR) or 0-4095 (ESP32)

    // ── Servo Control ───────────────────────────────────────────────────────
    virtual void servoAttach(uint8_t pin) = 0;
    virtual void servoWrite(uint8_t pin, uint8_t angleDegrees) = 0;
    virtual void servoWriteMicroseconds(uint8_t pin, uint16_t us) = 0;
    virtual void servoDetach(uint8_t pin) = 0;

    // ── Step Timer (ISR-driven stepper pulses) ──────────────────────────────
    // The step timer fires an interrupt at a configurable frequency.
    // The ISR callback toggles step pins using Bresenham's algorithm.
    typedef void (*TimerCallback)();

    virtual void stepTimerInit(TimerCallback callback) = 0;
    virtual void stepTimerSetFrequency(uint32_t frequencyHz) = 0;
    virtual void stepTimerStart() = 0;
    virtual void stepTimerStop() = 0;
    virtual bool stepTimerIsRunning() = 0;

    // ── Segment Preparation Timer ──────────────────────────────────────────
    // Lower-priority timer for preparing the next step segment from the
    // motion planner. Runs at ~1kHz.
    virtual void segmentTimerInit(TimerCallback callback) = 0;
    virtual void segmentTimerStart() = 0;
    virtual void segmentTimerStop() = 0;

    // ── External Interrupts (limit switches) ────────────────────────────────
    typedef void (*InterruptCallback)();

    virtual void attachInterrupt(uint8_t pin, InterruptCallback callback,
                                  InterruptTrigger trigger) = 0;
    virtual void detachInterrupt(uint8_t pin) = 0;

    // ── Critical Sections ──────────────────────────────────────────────────
    // Disable/re-enable interrupts for atomic operations
    virtual void disableInterrupts() = 0;
    virtual void enableInterrupts() = 0;

    // ── Persistent Storage (EEPROM / NVS) ──────────────────────────────────
    virtual void storageInit() = 0;
    virtual uint8_t storageReadByte(uint16_t address) = 0;
    virtual void storageWriteByte(uint16_t address, uint8_t value) = 0;
    virtual float storageReadFloat(uint16_t address) = 0;
    virtual void storageWriteFloat(uint16_t address, float value) = 0;
    virtual void storageCommit() = 0;  // ESP32 NVS needs explicit commit

    // ── Serial Communication ────────────────────────────────────────────────
    virtual void serialInit(uint32_t baud) = 0;
    virtual int  serialAvailable() = 0;
    virtual int  serialRead() = 0;
    virtual void serialWrite(uint8_t byte) = 0;
    virtual void serialPrint(const char* str) = 0;
    virtual void serialPrintln(const char* str) = 0;
    virtual void serialFlush() = 0;

    // ── Bridge Serial (hybrid mode) ────────────────────────────────────────
    virtual bool hasBridgeSerial() { return false; }
    virtual void bridgeSerialInit(uint32_t baud) {}
    virtual int  bridgeSerialAvailable() { return 0; }
    virtual int  bridgeSerialRead() { return -1; }
    virtual void bridgeSerialWrite(uint8_t byte) {}
    virtual void bridgeSerialPrint(const char* str) {}

    // ── TMC UART ────────────────────────────────────────────────────────────
    virtual bool hasTmcUart() { return false; }
    // TMC communication is handled by the TMCStepper library directly.
    // The HAL just provides the UART pins. See sensorless_homing.cpp.

    // ── Timing ──────────────────────────────────────────────────────────────
    virtual uint32_t millis() = 0;
    virtual uint32_t micros() = 0;
    virtual void delayMs(uint32_t ms) = 0;
    virtual void delayUs(uint32_t us) = 0;

    // ── System ──────────────────────────────────────────────────────────────
    virtual void reset() = 0;              // Software reset
    virtual uint32_t getFreeMemory() = 0;  // Bytes of free RAM

    // ── Inline Step Pulse Helpers ───────────────────────────────────────────
    // These are called from the ISR at very high frequency.
    // Implementations should be as fast as possible (direct register access).
    inline void stepPulseOn(uint8_t stepPin) { digitalWrite(stepPin, true); }
    inline void stepPulseOff(uint8_t stepPin) { digitalWrite(stepPin, false); }
    inline void setDirection(uint8_t dirPin, bool forward) { digitalWrite(dirPin, forward); }
    inline void enableStepper(uint8_t enablePin, bool active) {
        // Active LOW by default (STEPPER_ENABLE_ACTIVE_LOW in config)
        digitalWrite(enablePin, !active);
    }
};

// ── Global HAL instance ─────────────────────────────────────────────────────
// Created in main.cpp based on build target
extern HAL* hal;

#endif // HAL_H
