/**
 * ============================================================================
 * OpenPlotter — Stepper Driver Manager Implementation
 * ============================================================================
 */

#include "tmc_driver.h"
#include "config.h"
#include "../utils/logger.h"
#include "../hal/hal.h"
#include "../system/settings.h"
#include <SPI.h>
#include <TMCStepper.h>

#if !defined(BOARD_ESP32) && !defined(BOARD_NATIVE)
#include <SoftwareSerial.h>
#endif

extern HAL* hal;
extern Settings settings;

// Driver type selections (read from config.h or settings)
static uint8_t driverTypes[4] = {
    DEFAULT_DRIVER_X,
    DEFAULT_DRIVER_Y,
    DEFAULT_DRIVER_Z,
    DEFAULT_DRIVER_C
};

// Addresses/Pins for drivers
static uint8_t driverAddresses[4] = { 0, 1, 2, 3 }; // TMC2208/TMC2209 UART addresses
static uint8_t csPins[4] = { X_CS_PIN, Y_CS_PIN, Z_CS_PIN, C_CS_PIN }; // TMC2130/TMC5160 CS pins

// We use pointers to the base class or check subclasses.
// Since TMCStepper class hierarchy has different classes for each driver type,
// we'll instantiate specific classes based on driver type per axis.
// To keep things simple, we create static objects for axes and wrap them.

struct DriverWrap {
    TMC2208Stepper* tmc2208 = nullptr;
    TMC2209Stepper* tmc2209 = nullptr;
    TMC2130Stepper* tmc2130 = nullptr;
    TMC5160Stepper* tmc5160 = nullptr;
};

static DriverWrap drivers[4];

#if !defined(BOARD_ESP32) && !defined(BOARD_NATIVE)
// SoftwareSerial for AVR board sharing a single UART bus
static SoftwareSerial* avrTmcSerial = nullptr;
#endif

// Helper to check if TMC communication is active for an axis
static bool isTmcDriver(uint8_t axis) {
    if (axis >= 4) return false;
    return (driverTypes[axis] == DRIVER_TMC2208 || 
            driverTypes[axis] == DRIVER_TMC2209 || 
            driverTypes[axis] == DRIVER_TMC2130 || 
            driverTypes[axis] == DRIVER_TMC5160);
}

bool isSensorlessCapable(uint8_t axis) {
    if (axis >= 4) return false;
    return (driverTypes[axis] == DRIVER_TMC2209 || 
            driverTypes[axis] == DRIVER_TMC2130 || 
            driverTypes[axis] == DRIVER_TMC5160);
}

void initTmcDrivers() {
    LOG_INFO("DriverMgr: Initializing stepper drivers...");

    // Check if we need SPI
    bool needsSPI = false;
    // Check if we need UART
    bool needsUART = false;

    for (int i = 0; i < 4; i++) {
        if (driverTypes[i] == DRIVER_TMC2130 || driverTypes[i] == DRIVER_TMC5160) {
            needsSPI = true;
        }
        if (driverTypes[i] == DRIVER_TMC2208 || driverTypes[i] == DRIVER_TMC2209) {
            needsUART = true;
        }
    }

    // Initialize SPI if required
    if (needsSPI) {
        LOG_INFO("DriverMgr: Starting SPI bus...");
        SPI.begin();
        for (int i = 0; i < 4; i++) {
            if (driverTypes[i] == DRIVER_TMC2130 || driverTypes[i] == DRIVER_TMC5160) {
                pinMode(csPins[i], OUTPUT);
                digitalWrite(csPins[i], HIGH); // Disable CS initially
            }
        }
    }

    // Initialize UART if required
    Stream* uartStream = nullptr;
    #ifdef HAS_TMC_UART
    if (needsUART) {
        LOG_INFO("DriverMgr: Starting TMC UART bus...");
        #if defined(BOARD_ESP32)
            // ESP32 uses Hardware Serial 2
            Serial2.begin(TMC_BAUD_RATE, SERIAL_8N1, TMC_UART_RX_PIN, TMC_UART_TX_PIN);
            uartStream = &Serial2;
        #elif !defined(BOARD_NATIVE)
            // AVR uses SoftwareSerial
            avrTmcSerial = new SoftwareSerial(TMC_UART_RX_PIN, TMC_UART_TX_PIN);
            avrTmcSerial->begin(TMC_BAUD_RATE);
            uartStream = avrTmcSerial;
        #endif
    }
    #endif

    // Instantiate and configure drivers
    for (uint8_t i = 0; i < 4; i++) {
        const char* axisNames[] = { "X", "Y", "Z", "C" };
        
        if (!isTmcDriver(i)) {
            const char* stdDrivers[] = { "A4988", "DRV8825" };
            LOG_INFO("DriverMgr: Axis %s configured with standalone driver %s (Vref adjusted on pot)", 
                     axisNames[i], stdDrivers[driverTypes[i]]);
            continue;
        }

        // Configure based on type
        float senseResistor = 0.11f; // Standard Sense resistor for most TMC drivers
        if (driverTypes[i] == DRIVER_TMC5160) {
            senseResistor = 0.075f; // Standard Sense resistor for TMC5160
        }

        uint16_t runCurrent = settings.tmcRunCurrent();
        uint16_t holdCurrent = settings.tmcHoldCurrent();
        uint16_t microsteps = settings.tmcMicrosteps();
        bool stealth = settings.tmcStealthChop();
        float holdRatio = holdCurrent / (float)runCurrent;

        switch (driverTypes[i]) {
            case DRIVER_TMC2208:
                if (uartStream) {
                    drivers[i].tmc2208 = new TMC2208Stepper(uartStream, senseResistor, driverAddresses[i]);
                    drivers[i].tmc2208->begin();
                    drivers[i].tmc2208->rms_current(runCurrent, holdRatio);
                    drivers[i].tmc2208->microsteps(microsteps);
                    drivers[i].tmc2208->toff(4); // Enable driver
                    drivers[i].tmc2208->en_spreadCycle(!stealth);
                    LOG_INFO("DriverMgr: Axis %s (TMC2208 UART) initialized at %d mA", axisNames[i], runCurrent);
                }
                break;

            case DRIVER_TMC2209:
                if (uartStream) {
                    drivers[i].tmc2209 = new TMC2209Stepper(uartStream, senseResistor, driverAddresses[i]);
                    drivers[i].tmc2209->begin();
                    drivers[i].tmc2209->rms_current(runCurrent, holdRatio);
                    drivers[i].tmc2209->microsteps(microsteps);
                    drivers[i].tmc2209->toff(4);
                    drivers[i].tmc2209->en_spreadCycle(!stealth);
                    drivers[i].tmc2209->pwm_autoscale(true);
                    LOG_INFO("DriverMgr: Axis %s (TMC2209 UART) initialized at %d mA", axisNames[i], runCurrent);
                }
                break;

            case DRIVER_TMC2130:
                drivers[i].tmc2130 = new TMC2130Stepper(csPins[i], senseResistor);
                drivers[i].tmc2130->begin();
                drivers[i].tmc2130->rms_current(runCurrent, holdRatio);
                drivers[i].tmc2130->microsteps(microsteps);
                drivers[i].tmc2130->toff(4);
                drivers[i].tmc2130->en_spreadCycle(!stealth);
                LOG_INFO("DriverMgr: Axis %s (TMC2130 SPI) initialized at %d mA", axisNames[i], runCurrent);
                break;

            case DRIVER_TMC5160:
                drivers[i].tmc5160 = new TMC5160Stepper(csPins[i], senseResistor);
                drivers[i].tmc5160->begin();
                drivers[i].tmc5160->rms_current(runCurrent, holdRatio);
                drivers[i].tmc5160->microsteps(microsteps);
                drivers[i].tmc5160->toff(4);
                drivers[i].tmc5160->en_spreadCycle(!stealth);
                LOG_INFO("DriverMgr: Axis %s (TMC5160 High-Current SPI) initialized at %d mA", axisNames[i], runCurrent);
                break;
        }
    }
}

void configureTmcForHoming(uint8_t axis, bool enableHoming) {
    if (axis >= 4 || !isSensorlessCapable(axis)) return;

    uint16_t homingCurrent = DEFAULT_SG_HOMING_CURRENT;
    uint8_t threshold = DEFAULT_SG_THRESHOLD;

    uint16_t runCurrent = settings.tmcRunCurrent();
    uint16_t holdCurrent = settings.tmcHoldCurrent();
    bool stealth = settings.tmcStealthChop();
    float holdRatio = holdCurrent / (float)runCurrent;

    switch (driverTypes[axis]) {
        case DRIVER_TMC2209:
            if (drivers[axis].tmc2209) {
                if (enableHoming) {
                    drivers[axis].tmc2209->rms_current(homingCurrent);
                    drivers[axis].tmc2209->SGTHRS(threshold);
                    drivers[axis].tmc2209->en_spreadCycle(true); // StallGuard requires SpreadCycle
                    drivers[axis].tmc2209->TCOOLTHRS(0xFFFFF); // StallGuard active at all speeds
                    drivers[axis].tmc2209->diag1_stall(true);   // Output stall on DIAG pin
                } else {
                    // Restore defaults
                    drivers[axis].tmc2209->rms_current(runCurrent, holdRatio);
                    drivers[axis].tmc2209->SGTHRS(0); // Disable StallGuard
                    drivers[axis].tmc2209->en_spreadCycle(!stealth);
                }
            }
            break;

        case DRIVER_TMC2130:
            if (drivers[axis].tmc2130) {
                if (enableHoming) {
                    drivers[axis].tmc2130->rms_current(homingCurrent);
                    drivers[axis].tmc2130->sg_stall_value(threshold); // TMC2130 StallGuard sensitivity
                    drivers[axis].tmc2130->en_spreadCycle(true);
                    drivers[axis].tmc2130->TCOOLTHRS(0xFFFFF);
                    drivers[axis].tmc2130->diag1_stall(true);
                } else {
                    drivers[axis].tmc2130->rms_current(runCurrent, holdRatio);
                    drivers[axis].tmc2130->sg_stall_value(0);
                    drivers[axis].tmc2130->en_spreadCycle(!stealth);
                }
            }
            break;

        case DRIVER_TMC5160:
            if (drivers[axis].tmc5160) {
                if (enableHoming) {
                    drivers[axis].tmc5160->rms_current(homingCurrent);
                    drivers[axis].tmc5160->sg_stall_value(threshold); // TMC5160 StallGuard sensitivity
                    drivers[axis].tmc5160->en_spreadCycle(true);
                    drivers[axis].tmc5160->TCOOLTHRS(0xFFFFF);
                    drivers[axis].tmc5160->diag0_stall(true);
                } else {
                    drivers[axis].tmc5160->rms_current(runCurrent, holdRatio);
                    drivers[axis].tmc5160->sg_stall_value(0);
                    drivers[axis].tmc5160->en_spreadCycle(!stealth);
                }
            }
            break;
    }
}

void updateTmcCurrents(uint16_t runCurrentMa, uint16_t holdCurrentMa) {
    float holdRatio = holdCurrentMa / (float)runCurrentMa;
    for (int i = 0; i < 4; i++) {
        if (!isTmcDriver(i)) continue;
        switch (driverTypes[i]) {
            case DRIVER_TMC2208: if (drivers[i].tmc2208) drivers[i].tmc2208->rms_current(runCurrentMa, holdRatio); break;
            case DRIVER_TMC2209: if (drivers[i].tmc2209) drivers[i].tmc2209->rms_current(runCurrentMa, holdRatio); break;
            case DRIVER_TMC2130: if (drivers[i].tmc2130) drivers[i].tmc2130->rms_current(runCurrentMa, holdRatio); break;
            case DRIVER_TMC5160: if (drivers[i].tmc5160) drivers[i].tmc5160->rms_current(runCurrentMa, holdRatio); break;
        }
    }
}

void updateTmcMicrosteps(uint16_t microsteps) {
    for (int i = 0; i < 4; i++) {
        if (!isTmcDriver(i)) continue;
        switch (driverTypes[i]) {
            case DRIVER_TMC2208: if (drivers[i].tmc2208) drivers[i].tmc2208->microsteps(microsteps); break;
            case DRIVER_TMC2209: if (drivers[i].tmc2209) drivers[i].tmc2209->microsteps(microsteps); break;
            case DRIVER_TMC2130: if (drivers[i].tmc2130) drivers[i].tmc2130->microsteps(microsteps); break;
            case DRIVER_TMC5160: if (drivers[i].tmc5160) drivers[i].tmc5160->microsteps(microsteps); break;
        }
    }
}

void updateTmcStealthChop(bool enable) {
    for (int i = 0; i < 4; i++) {
        if (!isTmcDriver(i)) continue;
        switch (driverTypes[i]) {
            case DRIVER_TMC2208: if (drivers[i].tmc2208) drivers[i].tmc2208->en_spreadCycle(!enable); break;
            case DRIVER_TMC2209: if (drivers[i].tmc2209) drivers[i].tmc2209->en_spreadCycle(!enable); break;
            case DRIVER_TMC2130: if (drivers[i].tmc2130) drivers[i].tmc2130->en_spreadCycle(!enable); break;
            case DRIVER_TMC5160: if (drivers[i].tmc5160) drivers[i].tmc5160->en_spreadCycle(!enable); break;
        }
    }
}
