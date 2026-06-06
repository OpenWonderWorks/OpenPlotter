# OpenPlotter: Comprehensive Wiring Guide

This guide covers wiring the electronic components for your OpenPlotter. It supports two primary open-source control configurations: the **Arduino Mega 2560 + RAMPS 1.4** and the **Arduino Nano + CNC Shield v3**.

## Table of Contents
1. [General Stepper Motor Wiring](#1-general-stepper-motor-wiring)
2. [Driver Selection: A4988 vs TMC2208/2209](#2-driver-selection)
3. [Arduino Mega 2560 + RAMPS 1.4](#3-arduino-mega-2560--ramps-14)
4. [Arduino Nano + CNC Shield V3](#4-arduino-nano--cnc-shield-v3)
5. [Endstops / Limit Switches](#5-endstops)
6. [Servo Motor (Pen Lift)](#6-servo-motor-pen-lift)

---

## 1. General Stepper Motor Wiring

NEMA 17 stepper motors generally come with 4 wires. You must pair the wires into their respective coils (A and B). 
- If you don't know the pairs, cross two wires and turn the shaft by hand. If it offers resistance, those two wires belong to the same coil.
- Typical colors: **Black/Green** (Coil A) and **Red/Blue** (Coil B).
- Connect the wires to the 4-pin header next to the driver in the order: `1B, 1A, 2A, 2B`.

> [!WARNING]
> **NEVER unplug or plug in a stepper motor while the board is powered.** Doing so will instantly destroy the stepper driver chip.

---

## 2. Driver Selection

### A4988 or DRV8825
- Standard "dumb" drivers. 
- You MUST adjust the VREF potentiometer manually using a multimeter to set the motor current.
- **Microstepping**: Set using the jumpers under the driver. 
  - For A4988 (1/16th): Install all 3 jumpers.

### TMC2208 / TMC2209
- Silent and highly advanced.
- Depending on the board, these can be run in **Standalone (STEP/DIR)** mode or **UART** mode.
- If run in UART mode, you can set the motor current dynamically in the OpenPlotter Companion App under **Machine Settings**.
- **Microstepping**: Usually TMC drivers interpolate to 256 microsteps automatically.

---

## 3. Arduino Mega 2560 + RAMPS 1.4

The RAMPS 1.4 shield stacks perfectly on top of the Arduino Mega.

### Power
- Connect a 12V 5A (minimum) power supply to the **5A green terminal block** on the RAMPS shield.
- (Optional) Connect to the 11A block if you are using a heated bed, but a plotter does not require this.

### Drivers
- Plug your drivers into the **X** and **Y** sockets. 
- **Orientation:** Look for the `EN` pin on the driver and match it with the `EN` pin on the RAMPS board (usually closest to the edge with the power terminals).

---

## 4. Arduino Nano + CNC Shield V3

The CNC shield is highly popular for plotters due to its compact size.

### Power
- Connect a 12V (or 24V if your drivers support it) power supply to the blue terminal block on the shield.

### Drivers
- Plug drivers into the **X** and **Y** sockets.
- **Orientation:** Look for the `EN` pin on the driver and match it with the `EN` pin printed on the CNC shield PCB.

---

## 5. Endstops
Limit switches are used for Homing (`$H`).
- Use the **C (Common)** and **NO (Normally Open)** pins on the microswitch.
- **RAMPS 1.4:** Connect to `X-MIN` and `Y-MIN` on the endstop pins block. Only connect to the `S` (Signal) and `-` (Ground) pins. **Do NOT connect to `+` (5V) unless you are using active optical/hall-effect endstops, otherwise you will short the board!**
- **CNC Shield:** Connect to the `X+ / X-` and `Y+ / Y-` pins.

---

## 6. Servo Motor (Pen Lift)

If you are using a standard RC Servo (like an SG90) to lift the pen:
- **RAMPS 1.4:** Connect it to the servo block (next to the reset button). Usually **Servo 1 (Pin 11)**. Make sure the 5V jumper is installed next to the reset button to power the servos from the Arduino's 5V regulator, OR provide an external 5V to the servo pins.
- **CNC Shield:** Connect the Signal wire (usually yellow/orange) to the **Z+ Endstop pin (Pin 11)**, and grab 5V and GND from an adjacent header.

> [!TIP]
> Use the Companion App to quickly send `M3 S<angle>` to test your servo sweep angles before running a plot!
