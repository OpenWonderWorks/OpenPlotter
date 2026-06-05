/**
 * ============================================================================
 * OpenPlotter — ESP32 HAL Implementation
 * ============================================================================
 *
 * Uses ESP32 hardware timers for stepper ISR, LEDC for servo/PWM,
 * NVS (Preferences) for persistent storage.
 *
 * ============================================================================
 */

#if defined(BOARD_ESP32)

#include "hal_esp32.h"
#include "config.h"
#include <Preferences.h>
#include <esp_system.h>

// ── NVS Preferences instance ───────────────────────────────────────────────
static Preferences _prefs;

// ── Static callbacks for ISR ────────────────────────────────────────────────
static HAL::TimerCallback _stepTimerCb = nullptr;
static HAL::TimerCallback _segmentTimerCb = nullptr;

static void IRAM_ATTR stepTimerISR() {
    if (_stepTimerCb) _stepTimerCb();
}

static void IRAM_ATTR segmentTimerISR() {
    if (_segmentTimerCb) _segmentTimerCb();
}

// ============================================================================
// Lifecycle
// ============================================================================

void HAL_ESP32::init() {
    // ESP32 Arduino framework handles basic init
}

// ============================================================================
// GPIO
// ============================================================================

void HAL_ESP32::pinMode(uint8_t pin, PinMode mode) {
    switch (mode) {
        case PinMode::INPUT_MODE:        ::pinMode(pin, INPUT); break;
        case PinMode::OUTPUT_MODE:       ::pinMode(pin, OUTPUT); break;
        case PinMode::INPUT_PULLUP_MODE: ::pinMode(pin, INPUT_PULLUP); break;
    }
}

void HAL_ESP32::digitalWrite(uint8_t pin, bool value) {
    ::digitalWrite(pin, value ? HIGH : LOW);
}

bool HAL_ESP32::digitalRead(uint8_t pin) {
    return ::digitalRead(pin) == HIGH;
}

// ============================================================================
// PWM / Analog
// ============================================================================

uint8_t HAL_ESP32::getPwmChannel(uint8_t pin) {
    // Check if pin already has an assigned channel
    for (uint8_t i = PWM_CHANNEL_START; i < _nextPwmChannel; i++) {
        if (_pwmChannelMap[i] == pin) return i;
    }
    // Assign new channel
    uint8_t ch = _nextPwmChannel++;
    if (ch >= 16) ch = 15; // Clamp to max channels
    _pwmChannelMap[ch] = pin;
    ledcSetup(ch, 5000, 8);  // 5kHz, 8-bit resolution
    ledcAttachPin(pin, ch);
    return ch;
}

void HAL_ESP32::analogWrite(uint8_t pin, uint8_t value) {
    uint8_t ch = getPwmChannel(pin);
    ledcWrite(ch, value);
}

uint16_t HAL_ESP32::analogRead(uint8_t pin) {
    return ::analogRead(pin);  // 12-bit on ESP32 (0-4095)
}

// ============================================================================
// Servo (via LEDC)
// ============================================================================
// ESP32 doesn't have a native Servo library. We use LEDC PWM channel 0
// at 50Hz with 16-bit resolution to generate servo control pulses.
//
// Servo pulse width: 544µs (0°) to 2400µs (180°)
// At 50Hz (20ms period), 16-bit (65536 ticks):
//   1 tick = 20000µs / 65536 = 0.305µs
//   544µs  = 1785 ticks
//   2400µs = 7864 ticks

void HAL_ESP32::servoAttach(uint8_t pin) {
    _servoPin = pin;
    ledcSetup(SERVO_CHANNEL, SERVO_FREQ, SERVO_RESOLUTION);
    ledcAttachPin(pin, SERVO_CHANNEL);
    _servoAttached = true;
}

void HAL_ESP32::servoWrite(uint8_t pin, uint8_t angleDegrees) {
    (void)pin;
    if (!_servoAttached) return;

    // Map 0-180 degrees to pulse width in microseconds
    uint16_t pulseUs = map(angleDegrees, 0, 180, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    servoWriteMicroseconds(_servoPin, pulseUs);
}

void HAL_ESP32::servoWriteMicroseconds(uint8_t pin, uint16_t us) {
    (void)pin;
    if (!_servoAttached) return;

    // Convert microseconds to LEDC ticks
    // ticks = us * 65536 / 20000
    uint32_t ticks = (uint32_t)us * 65536 / 20000;
    ledcWrite(SERVO_CHANNEL, ticks);
}

void HAL_ESP32::servoDetach(uint8_t pin) {
    (void)pin;
    ledcDetachPin(_servoPin);
    _servoAttached = false;
}

// ============================================================================
// Step Timer (Hardware Timer 0)
// ============================================================================
// ESP32 hardware timers run at 80MHz base clock.
// We use a prescaler of 80 to get a 1µs tick (1MHz timer clock).
// Then the alarm value directly represents microseconds between interrupts.

void HAL_ESP32::stepTimerInit(TimerCallback callback) {
    _stepTimerCb = callback;

    // Timer 0, prescaler 80 (80MHz / 80 = 1MHz = 1µs tick), count up
    _stepTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(_stepTimer, &stepTimerISR, true);
    // Default: 125µs interval = 8kHz
    timerAlarmWrite(_stepTimer, 125, true);  // auto-reload
}

void HAL_ESP32::stepTimerSetFrequency(uint32_t frequencyHz) {
    if (frequencyHz == 0 || !_stepTimer) return;

    // interval_us = 1,000,000 / frequency
    uint64_t intervalUs = 1000000ULL / frequencyHz;
    if (intervalUs < 1) intervalUs = 1;

    timerAlarmWrite(_stepTimer, intervalUs, true);
}

void HAL_ESP32::stepTimerStart() {
    if (_stepTimer) {
        timerAlarmEnable(_stepTimer);
        _stepTimerRunning = true;
    }
}

void HAL_ESP32::stepTimerStop() {
    if (_stepTimer) {
        timerAlarmDisable(_stepTimer);
        _stepTimerRunning = false;
    }
}

bool HAL_ESP32::stepTimerIsRunning() {
    return _stepTimerRunning;
}

// ============================================================================
// Segment Preparation Timer (Hardware Timer 1)
// ============================================================================

void HAL_ESP32::segmentTimerInit(TimerCallback callback) {
    _segmentTimerCb = callback;

    _segmentTimer = timerBegin(1, 80, true);   // Timer 1, 1µs tick
    timerAttachInterrupt(_segmentTimer, &segmentTimerISR, true);
    timerAlarmWrite(_segmentTimer, 1000, true); // 1ms = 1kHz
}

void HAL_ESP32::segmentTimerStart() {
    if (_segmentTimer) timerAlarmEnable(_segmentTimer);
}

void HAL_ESP32::segmentTimerStop() {
    if (_segmentTimer) timerAlarmDisable(_segmentTimer);
}

// ============================================================================
// External Interrupts
// ============================================================================

void HAL_ESP32::attachInterrupt(uint8_t pin, InterruptCallback callback,
                                 InterruptTrigger trigger) {
    int mode;
    switch (trigger) {
        case InterruptTrigger::RISING:  mode = RISING; break;
        case InterruptTrigger::FALLING: mode = FALLING; break;
        case InterruptTrigger::CHANGE:  mode = CHANGE; break;
        default: mode = CHANGE;
    }
    ::attachInterrupt(digitalPinToInterrupt(pin), callback, mode);
}

void HAL_ESP32::detachInterrupt(uint8_t pin) {
    ::detachInterrupt(digitalPinToInterrupt(pin));
}

// ============================================================================
// Critical Sections
// ============================================================================

static portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

void HAL_ESP32::disableInterrupts() {
    portENTER_CRITICAL(&_spinlock);
}

void HAL_ESP32::enableInterrupts() {
    portEXIT_CRITICAL(&_spinlock);
}

// ============================================================================
// Persistent Storage (NVS via Preferences)
// ============================================================================

void HAL_ESP32::storageInit() {
    _prefs.begin("openplotter", false); // read-write mode
}

uint8_t HAL_ESP32::storageReadByte(uint16_t address) {
    char key[8];
    snprintf(key, sizeof(key), "b%u", address);
    return _prefs.getUChar(key, 0xFF);
}

void HAL_ESP32::storageWriteByte(uint16_t address, uint8_t value) {
    char key[8];
    snprintf(key, sizeof(key), "b%u", address);
    _prefs.putUChar(key, value);
}

float HAL_ESP32::storageReadFloat(uint16_t address) {
    char key[8];
    snprintf(key, sizeof(key), "f%u", address);
    return _prefs.getFloat(key, 0.0f);
}

void HAL_ESP32::storageWriteFloat(uint16_t address, float value) {
    char key[8];
    snprintf(key, sizeof(key), "f%u", address);
    _prefs.putFloat(key, value);
}

void HAL_ESP32::storageCommit() {
    // NVS commits are immediate with Preferences library
    // But we end and re-begin to flush
    _prefs.end();
    _prefs.begin("openplotter", false);
}

// ============================================================================
// Serial Communication
// ============================================================================

void HAL_ESP32::serialInit(uint32_t baud) {
    Serial.begin(baud);
}

int  HAL_ESP32::serialAvailable() { return Serial.available(); }
int  HAL_ESP32::serialRead() { return Serial.read(); }
void HAL_ESP32::serialWrite(uint8_t byte) { Serial.write(byte); }
void HAL_ESP32::serialPrint(const char* str) { Serial.print(str); }
void HAL_ESP32::serialPrintln(const char* str) { Serial.println(str); }
void HAL_ESP32::serialFlush() { Serial.flush(); }

// ============================================================================
// Bridge Serial (Serial2 — for hybrid or bridge mode)
// ============================================================================

bool HAL_ESP32::hasBridgeSerial() {
    #if defined(HYBRID_MODE) || defined(BRIDGE_MODE)
        return true;
    #else
        return false;
    #endif
}

void HAL_ESP32::bridgeSerialInit(uint32_t baud) {
    #if defined(HYBRID_MODE) || defined(BRIDGE_MODE)
        #ifdef TMC_UART_RX_PIN
            Serial2.begin(baud, SERIAL_8N1, TMC_UART_RX_PIN, TMC_UART_TX_PIN);
        #else
            Serial2.begin(baud, SERIAL_8N1, 16, 17); // Default GPIO16 RX, GPIO17 TX
        #endif
    #else
        (void)baud;
    #endif
}

int HAL_ESP32::bridgeSerialAvailable() {
    #if defined(HYBRID_MODE) || defined(BRIDGE_MODE)
        return Serial2.available();
    #else
        return 0;
    #endif
}

int HAL_ESP32::bridgeSerialRead() {
    #if defined(HYBRID_MODE) || defined(BRIDGE_MODE)
        return Serial2.read();
    #else
        return -1;
    #endif
}

void HAL_ESP32::bridgeSerialWrite(uint8_t byte) {
    #if defined(HYBRID_MODE) || defined(BRIDGE_MODE)
        Serial2.write(byte);
    #else
        (void)byte;
    #endif
}

void HAL_ESP32::bridgeSerialPrint(const char* str) {
    #if defined(HYBRID_MODE) || defined(BRIDGE_MODE)
        Serial2.print(str);
    #else
        (void)str;
    #endif
}

// ============================================================================
// TMC UART
// ============================================================================

bool HAL_ESP32::hasTmcUart() {
    #ifdef HAS_TMC_UART
        return true;
    #else
        return false;
    #endif
}

// ============================================================================
// Timing
// ============================================================================

uint32_t HAL_ESP32::millis() { return ::millis(); }
uint32_t HAL_ESP32::micros() { return ::micros(); }
void HAL_ESP32::delayMs(uint32_t ms) { delay(ms); }
void HAL_ESP32::delayUs(uint32_t us) { delayMicroseconds(us); }

// ============================================================================
// System
// ============================================================================

void HAL_ESP32::reset() {
    esp_restart();
}

uint32_t HAL_ESP32::getFreeMemory() {
    return ESP.getFreeHeap();
}

#endif // BOARD_ESP32
