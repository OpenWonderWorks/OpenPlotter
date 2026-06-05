/**
 * ============================================================================
 * OpenPlotter — ESP32 HAL Header
 * ============================================================================
 */

#ifndef HAL_ESP32_H
#define HAL_ESP32_H

#if defined(BOARD_ESP32)

#include "hal.h"
#include <Arduino.h>

class HAL_ESP32 : public HAL {
public:
    void init() override;

    // GPIO
    void pinMode(uint8_t pin, PinMode mode) override;
    void digitalWrite(uint8_t pin, bool value) override;
    bool digitalRead(uint8_t pin) override;

    // PWM / Analog
    void analogWrite(uint8_t pin, uint8_t value) override;
    uint16_t analogRead(uint8_t pin) override;

    // Servo (via LEDC)
    void servoAttach(uint8_t pin) override;
    void servoWrite(uint8_t pin, uint8_t angleDegrees) override;
    void servoWriteMicroseconds(uint8_t pin, uint16_t us) override;
    void servoDetach(uint8_t pin) override;

    // Step Timer (hw_timer)
    void stepTimerInit(TimerCallback callback) override;
    void stepTimerSetFrequency(uint32_t frequencyHz) override;
    void stepTimerStart() override;
    void stepTimerStop() override;
    bool stepTimerIsRunning() override;

    // Segment Timer
    void segmentTimerInit(TimerCallback callback) override;
    void segmentTimerStart() override;
    void segmentTimerStop() override;

    // Interrupts
    void attachInterrupt(uint8_t pin, InterruptCallback callback,
                          InterruptTrigger trigger) override;
    void detachInterrupt(uint8_t pin) override;

    // Critical sections
    void disableInterrupts() override;
    void enableInterrupts() override;

    // Storage (NVS via Preferences)
    void storageInit() override;
    uint8_t storageReadByte(uint16_t address) override;
    void storageWriteByte(uint16_t address, uint8_t value) override;
    float storageReadFloat(uint16_t address) override;
    void storageWriteFloat(uint16_t address, float value) override;
    void storageCommit() override;

    // Serial
    void serialInit(uint32_t baud) override;
    int  serialAvailable() override;
    int  serialRead() override;
    void serialWrite(uint8_t byte) override;
    void serialPrint(const char* str) override;
    void serialPrintln(const char* str) override;
    void serialFlush() override;

    // Bridge Serial (ESP32 in bridge mode uses Serial2)
    bool hasBridgeSerial() override;
    void bridgeSerialInit(uint32_t baud) override;
    int  bridgeSerialAvailable() override;
    int  bridgeSerialRead() override;
    void bridgeSerialWrite(uint8_t byte) override;
    void bridgeSerialPrint(const char* str) override;

    // TMC
    bool hasTmcUart() override;

    // Timing
    uint32_t millis() override;
    uint32_t micros() override;
    void delayMs(uint32_t ms) override;
    void delayUs(uint32_t us) override;

    // System
    void reset() override;
    uint32_t getFreeMemory() override;

private:
    hw_timer_t* _stepTimer = nullptr;
    hw_timer_t* _segmentTimer = nullptr;
    bool _stepTimerRunning = false;
    uint8_t _servoPin = 0;
    bool _servoAttached = false;

    // LEDC configuration for servo
    static constexpr uint8_t SERVO_CHANNEL = 0;
    static constexpr uint32_t SERVO_FREQ = 50;       // 50Hz
    static constexpr uint8_t SERVO_RESOLUTION = 16;   // 16-bit

    // LEDC for analogWrite emulation
    static constexpr uint8_t PWM_CHANNEL_START = 2;  // Channels 0-1 reserved for servo
    uint8_t _pwmChannelMap[16] = {0};  // pin → LEDC channel mapping
    uint8_t _nextPwmChannel = PWM_CHANNEL_START;

    uint8_t getPwmChannel(uint8_t pin);
};

#endif // BOARD_ESP32
#endif // HAL_ESP32_H
