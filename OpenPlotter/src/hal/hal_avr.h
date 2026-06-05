/**
 * ============================================================================
 * OpenPlotter — AVR HAL Implementation (Arduino Mega / Nano)
 * ============================================================================
 */

#ifndef HAL_AVR_H
#define HAL_AVR_H

#if defined(BOARD_MEGA) || defined(BOARD_NANO)

#include "hal.h"
#include <Arduino.h>
#include <Servo.h>
#include <EEPROM.h>

#ifdef HYBRID_MODE
    #if defined(BOARD_NANO)
        #include <SoftwareSerial.h>
    #endif
#endif

class HAL_AVR : public HAL {
public:
    void init() override;

    // GPIO
    void pinMode(uint8_t pin, PinMode mode) override;
    void digitalWrite(uint8_t pin, bool value) override;
    bool digitalRead(uint8_t pin) override;

    // PWM / Analog
    void analogWrite(uint8_t pin, uint8_t value) override;
    uint16_t analogRead(uint8_t pin) override;

    // Servo
    void servoAttach(uint8_t pin) override;
    void servoWrite(uint8_t pin, uint8_t angleDegrees) override;
    void servoWriteMicroseconds(uint8_t pin, uint16_t us) override;
    void servoDetach(uint8_t pin) override;

    // Step Timer
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

    // Storage (EEPROM)
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

    // Bridge Serial
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
    Servo _servo;
    uint8_t _servoPin = 0;
    bool _stepTimerRunning = false;

    #if defined(BOARD_NANO) && defined(HYBRID_MODE)
        SoftwareSerial* _bridgeSerial = nullptr;
    #endif
};

#endif // BOARD_MEGA || BOARD_NANO
#endif // HAL_AVR_H
