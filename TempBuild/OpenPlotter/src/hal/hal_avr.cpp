/**
 * ============================================================================
 * OpenPlotter — AVR HAL Implementation (Arduino Mega / Nano)
 * ============================================================================
 *
 * Uses AVR Timer1 (16-bit) for the stepper pulse interrupt and
 * Timer3 (Mega only) or Timer2 (Nano) for segment preparation.
 *
 * ============================================================================
 */

#if defined(BOARD_MEGA) || defined(BOARD_NANO)

#include "hal_avr.h"
#include "../../config.h"
#include <avr/wdt.h>
#include <avr/interrupt.h>

// ── Static callback storage (ISR can't call member functions) ───────────────
static HAL::TimerCallback _stepTimerCb = nullptr;
static HAL::TimerCallback _segmentTimerCb = nullptr;

// ── Timer1 ISR — Stepper Pulse Generation ──────────────────────────────────
ISR(TIMER1_COMPA_vect) {
    if (_stepTimerCb) _stepTimerCb();
}

// ── Timer3 ISR (Mega) or Timer2 ISR (Nano) — Segment Preparation ──────────
#if defined(BOARD_MEGA)
ISR(TIMER3_COMPA_vect) {
    if (_segmentTimerCb) _segmentTimerCb();
}
#else
ISR(TIMER2_COMPA_vect) {
    if (_segmentTimerCb) _segmentTimerCb();
}
#endif

// ============================================================================
// Lifecycle
// ============================================================================

void HAL_AVR::init() {
    // Nothing special needed — Arduino framework handles basic init
}

// ============================================================================
// GPIO
// ============================================================================

void HAL_AVR::pinMode(uint8_t pin, PinMode mode) {
    switch (mode) {
        case PinMode::INPUT_MODE:       ::pinMode(pin, INPUT); break;
        case PinMode::OUTPUT_MODE:      ::pinMode(pin, OUTPUT); break;
        case PinMode::INPUT_PULLUP_MODE: ::pinMode(pin, INPUT_PULLUP); break;
    }
}

void HAL_AVR::digitalWrite(uint8_t pin, bool value) {
    ::digitalWrite(pin, value ? HIGH : LOW);
}

bool HAL_AVR::digitalRead(uint8_t pin) {
    return ::digitalRead(pin) == HIGH;
}

// ============================================================================
// PWM / Analog
// ============================================================================

void HAL_AVR::analogWrite(uint8_t pin, uint8_t value) {
    ::analogWrite(pin, value);
}

uint16_t HAL_AVR::analogRead(uint8_t pin) {
    return ::analogRead(pin);
}

// ============================================================================
// Servo
// ============================================================================

void HAL_AVR::servoAttach(uint8_t pin) {
    _servoPin = pin;
    _servo.attach(pin, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
}

void HAL_AVR::servoWrite(uint8_t pin, uint8_t angleDegrees) {
    (void)pin; // Uses the attached servo
    _servo.write(angleDegrees);
}

void HAL_AVR::servoWriteMicroseconds(uint8_t pin, uint16_t us) {
    (void)pin;
    _servo.writeMicroseconds(us);
}

void HAL_AVR::servoDetach(uint8_t pin) {
    (void)pin;
    _servo.detach();
}

// ============================================================================
// Step Timer (Timer1 — 16-bit)
// ============================================================================
// Timer1 is configured in CTC mode (Clear Timer on Compare Match).
// OCR1A sets the compare value, which determines the interrupt frequency:
//   frequency = F_CPU / (prescaler * (OCR1A + 1))
//
// With no prescaler (CS10 = 1):
//   At 16MHz, OCR1A = 199  → 80kHz
//   At 16MHz, OCR1A = 1599 → 10kHz
//   At 16MHz, OCR1A = 15999→ 1kHz

void HAL_AVR::stepTimerInit(TimerCallback callback) {
    _stepTimerCb = callback;

    cli(); // Disable interrupts during setup
    TCCR1A = 0;                         // Normal mode (no PWM)
    TCCR1B = 0;                         // Stop timer
    TCNT1 = 0;                          // Reset counter
    OCR1A = 2000;                       // Default: ~8kHz
    TCCR1B |= (1 << WGM12);            // CTC mode
    TCCR1B |= (1 << CS10);             // No prescaler (full 16MHz clock)
    // Don't enable interrupt yet — wait for stepTimerStart()
    sei();
}

void HAL_AVR::stepTimerSetFrequency(uint32_t frequencyHz) {
    if (frequencyHz == 0) return;

    // OCR1A = (F_CPU / frequency) - 1
    // Clamp to 16-bit range
    uint32_t ocr = (F_CPU / frequencyHz) - 1;
    if (ocr > 65535) ocr = 65535;
    if (ocr < 1) ocr = 1;

    cli();
    OCR1A = (uint16_t)ocr;
    sei();
}

void HAL_AVR::stepTimerStart() {
    TIMSK1 |= (1 << OCIE1A);   // Enable Timer1 compare A interrupt
    _stepTimerRunning = true;
}

void HAL_AVR::stepTimerStop() {
    TIMSK1 &= ~(1 << OCIE1A);  // Disable Timer1 compare A interrupt
    _stepTimerRunning = false;
}

bool HAL_AVR::stepTimerIsRunning() {
    return _stepTimerRunning;
}

// ============================================================================
// Segment Preparation Timer
// ============================================================================

#if defined(BOARD_MEGA)
// Timer3 (16-bit) on Mega — configured for ~1kHz
void HAL_AVR::segmentTimerInit(TimerCallback callback) {
    _segmentTimerCb = callback;
    cli();
    TCCR3A = 0;
    TCCR3B = 0;
    TCNT3 = 0;
    OCR3A = 15999;                      // 16MHz / 1 / 16000 = 1kHz
    TCCR3B |= (1 << WGM32);            // CTC mode
    TCCR3B |= (1 << CS30);             // No prescaler
    sei();
}

void HAL_AVR::segmentTimerStart() {
    TIMSK3 |= (1 << OCIE3A);
}

void HAL_AVR::segmentTimerStop() {
    TIMSK3 &= ~(1 << OCIE3A);
}

#else
// Timer2 (8-bit) on Nano — configured for ~1kHz with prescaler
void HAL_AVR::segmentTimerInit(TimerCallback callback) {
    _segmentTimerCb = callback;
    cli();
    TCCR2A = 0;
    TCCR2B = 0;
    TCNT2 = 0;
    OCR2A = 249;                        // 16MHz / 64 / 250 = 1kHz
    TCCR2A |= (1 << WGM21);            // CTC mode
    TCCR2B |= (1 << CS22);             // Prescaler 64
    sei();
}

void HAL_AVR::segmentTimerStart() {
    TIMSK2 |= (1 << OCIE2A);
}

void HAL_AVR::segmentTimerStop() {
    TIMSK2 &= ~(1 << OCIE2A);
}
#endif

// ============================================================================
// External Interrupts
// ============================================================================

void HAL_AVR::attachInterrupt(uint8_t pin, InterruptCallback callback,
                               InterruptTrigger trigger) {
    int mode;
    switch (trigger) {
        case InterruptTrigger::TRIG_RISING:  mode = TRIG_RISING; break;
        case InterruptTrigger::TRIG_FALLING: mode = TRIG_FALLING; break;
        case InterruptTrigger::TRIG_CHANGE:  mode = TRIG_CHANGE; break;
        default: mode = TRIG_CHANGE;
    }

    int intNum = digitalPinToInterrupt(pin);
    if (intNum != NOT_AN_INTERRUPT) {
        ::attachInterrupt(intNum, callback, mode);
    }
}

void HAL_AVR::detachInterrupt(uint8_t pin) {
    int intNum = digitalPinToInterrupt(pin);
    if (intNum != NOT_AN_INTERRUPT) {
        ::detachInterrupt(intNum);
    }
}

// ============================================================================
// Critical Sections
// ============================================================================

void HAL_AVR::disableInterrupts() { cli(); }
void HAL_AVR::enableInterrupts() { sei(); }

// ============================================================================
// Persistent Storage (EEPROM)
// ============================================================================

void HAL_AVR::storageInit() {
    // EEPROM on AVR doesn't need initialization
}

uint8_t HAL_AVR::storageReadByte(uint16_t address) {
    return EEPROM.read(address);
}

void HAL_AVR::storageWriteByte(uint16_t address, uint8_t value) {
    EEPROM.update(address, value);  // Only writes if value changed (saves wear)
}

float HAL_AVR::storageReadFloat(uint16_t address) {
    float val;
    EEPROM.get(address, val);
    return val;
}

void HAL_AVR::storageWriteFloat(uint16_t address, float value) {
    EEPROM.put(address, value);
}

void HAL_AVR::storageCommit() {
    // AVR EEPROM writes are immediate — no commit needed
}

// ============================================================================
// Serial Communication
// ============================================================================

void HAL_AVR::serialInit(uint32_t baud) {
    Serial.begin(baud);
    while (!Serial) { ; } // Wait for serial port (Leonardo/Micro only, instant on Mega/Nano)
}

int  HAL_AVR::serialAvailable() { return Serial.available(); }
int  HAL_AVR::serialRead() { return Serial.read(); }
void HAL_AVR::serialWrite(uint8_t byte) { Serial.write(byte); }
void HAL_AVR::serialPrint(const char* str) { Serial.print(str); }
void HAL_AVR::serialPrintln(const char* str) { Serial.println(str); }
void HAL_AVR::serialFlush() { Serial.flush(); }

// ============================================================================
// Bridge Serial (Hybrid Mode)
// ============================================================================

bool HAL_AVR::hasBridgeSerial() {
    #ifdef HYBRID_MODE
        return true;
    #else
        return false;
    #endif
}

void HAL_AVR::bridgeSerialInit(uint32_t baud) {
    #ifdef HYBRID_MODE
        #if defined(BOARD_MEGA)
            Serial1.begin(baud);
        #elif defined(BOARD_NANO)
            _bridgeSerial = new SoftwareSerial(BRIDGE_RX_PIN, BRIDGE_TX_PIN);
            _bridgeSerial->begin(baud);
        #endif
    #else
        (void)baud;
    #endif
}

int HAL_AVR::bridgeSerialAvailable() {
    #ifdef HYBRID_MODE
        #if defined(BOARD_MEGA)
            return Serial1.available();
        #elif defined(BOARD_NANO)
            return _bridgeSerial ? _bridgeSerial->available() : 0;
        #endif
    #endif
    return 0;
}

int HAL_AVR::bridgeSerialRead() {
    #ifdef HYBRID_MODE
        #if defined(BOARD_MEGA)
            return Serial1.read();
        #elif defined(BOARD_NANO)
            return _bridgeSerial ? _bridgeSerial->read() : -1;
        #endif
    #endif
    return -1;
}

void HAL_AVR::bridgeSerialWrite(uint8_t byte) {
    #ifdef HYBRID_MODE
        #if defined(BOARD_MEGA)
            Serial1.write(byte);
        #elif defined(BOARD_NANO)
            if (_bridgeSerial) _bridgeSerial->write(byte);
        #endif
    #else
        (void)byte;
    #endif
}

void HAL_AVR::bridgeSerialPrint(const char* str) {
    #ifdef HYBRID_MODE
        #if defined(BOARD_MEGA)
            Serial1.print(str);
        #elif defined(BOARD_NANO)
            if (_bridgeSerial) _bridgeSerial->print(str);
        #endif
    #else
        (void)str;
    #endif
}

// ============================================================================
// TMC UART
// ============================================================================

bool HAL_AVR::hasTmcUart() {
    #ifdef HAS_TMC_UART
        return true;
    #else
        return false;
    #endif
}

// ============================================================================
// Timing
// ============================================================================

uint32_t HAL_AVR::millis() { return ::millis(); }
uint32_t HAL_AVR::micros() { return ::micros(); }
void HAL_AVR::delayMs(uint32_t ms) { delay(ms); }
void HAL_AVR::delayUs(uint32_t us) { delayMicroseconds(us); }

// ============================================================================
// System
// ============================================================================

void HAL_AVR::reset() {
    // Use watchdog timer to force a reset
    wdt_enable(WDTO_15MS);
    while (true) { ; }
}

uint32_t HAL_AVR::getFreeMemory() {
    extern int __heap_start, *__brkval;
    int v;
    return (uint32_t)&v - (__brkval == 0
        ? (uint32_t)&__heap_start
        : (uint32_t)__brkval);
}

#endif // BOARD_MEGA || BOARD_NANO
