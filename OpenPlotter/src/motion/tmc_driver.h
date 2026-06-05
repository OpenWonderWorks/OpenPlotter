/**
 * ============================================================================
 * OpenPlotter — Stepper Driver Manager
 * ============================================================================
 */

#ifndef TMC_DRIVER_H
#define TMC_DRIVER_H

#include <Arduino.h>

// Stepper Driver Type Definitions
#define DRIVER_A4988       0  // Standard, manual pot Vref, no communications
#define DRIVER_DRV8825     1  // Standard, manual pot Vref, no communications
#define DRIVER_TMC2208     2  // UART, silent, no StallGuard
#define DRIVER_TMC2209     3  // UART, silent, StallGuard (sensorless homing)
#define DRIVER_TMC2130     4  // SPI, StallGuard (sensorless homing)
#define DRIVER_TMC5160     5  // SPI, High-Current (up to 3A+), StallGuard (sensorless homing) - BADASS!

// Functions
void initTmcDrivers();
bool isSensorlessCapable(uint8_t axis);
void configureTmcForHoming(uint8_t axis, bool enableHoming);
void updateTmcCurrents(uint16_t runCurrentMa, uint16_t holdCurrentMa);
void updateTmcMicrosteps(uint16_t microsteps);
void updateTmcStealthChop(bool enable);

#endif // TMC_DRIVER_H
