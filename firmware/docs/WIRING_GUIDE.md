# OpenPlotter — Complete Wiring & Hardware Guide

This guide describes how to connect the stepper motors, drivers, tools, and power supply for all supported configurations of OpenPlotter. 

---

## 1. Primary Configuration: Arduino Mega 2560 + RAMPS 1.4 + ESP32

This is the recommended setup for cutting plotters. The Arduino Mega runs the core motion planner and stepper engine, while the ESP32 serves the web interface and handles WebSocket/Serial routing.

### Hardware Requirements
- Arduino Mega 2560 board
- RAMPS 1.4 (or 1.5 / 1.6) shield
- 2x TMC2209 Stepper Drivers (UART mode for X and Y axes)
- 1x Servo Motor (SG90 or MG90S) for pen/blade up-down control
- 1x ESP32 DevKit V1 board
- 2x NEMA 17 Stepper Motors (for X and Y)
- 12V or 24V DC Power Supply (minimum 5A / 60W)
- 2x 1kΩ Resistors (for TMC2209 UART)
- Jumper wires and breadboard or perfboard for ESP32 routing

---

### System Interconnection Schematic

```
                          12V / 24V DC POWER SUPPLY
                        ┌────────────────────────┐
                        │     [V+]      [GND]    │
                        └───────┬─────────┬──────┘
                                │         │
         ┌──────────────────────┼─────────┼──────────────────────┐
         │ RAMPS 1.4 Shield     ▼         ▼                      │
         │ Power Input Header [12V IN]  [GND IN]                 │
         │                                                       │
         │ X-DRIVER SLOT  ───► Connects to X Stepper Motor       │
         │ Y-DRIVER SLOT  ───► Connects to Y Stepper Motor       │
         │                                                       │
         │ X-MIN Header   ───► Limit Switch Signal & GND         │
         │ Y-MIN Header   ───► Limit Switch Signal & GND         │
         │                                                       │
         │ SERVO-0 (Pin 11)──► PWM Signal (Orange/Yellow wire)    │
         │ SERVO-0 [5V]   ───► 5V Power (Red wire)               │
         │ SERVO-0 [GND]  ───► Common Ground (Black/Brown wire)  │
         │                                                       │
         │ AUX-1 Pins     ───► TX1 (Pin 18) & RX1 (Pin 19)       │
         │ AUX-2 Pins     ───► TMC UART TX (Pin 40), RX (Pin A9) │
         └──────────────────────┬─────────┬──────────────────────┘
                                │         │
                                │         │ Serial Bridge
                                ▼         ▼
                        ┌────────────────────────┐
                        │ ESP32 DevKit V1        │
                        │                        │
                        │ GPIO 16 (RX2) ◄── Mega TX1
                        │ GPIO 17 (TX2) ◄── Mega RX1
                        │ GND           ◄── Mega GND
                        │ VIN           ◄── 5V Power Input
                        └────────────────────────┘
```

---

### Step-by-Step Connection Instructions

#### A. Power Supply Connection
1. Locate the green power input terminal block on the RAMPS 1.4 board.
2. Connect the **Positive (+)** wire of your 12V or 24V power supply to the positive terminal (labeled `12V / 5A`).
3. Connect the **Negative (-)** wire of your power supply to the negative terminal (labeled `GND`).
4. **WARNING**: Double-check polarity with a multimeter. Reversed polarity will destroy the RAMPS board and Arduino Mega instantly.

#### B. Stepper Driver Installation (X and Y Axes)
1. If using **TMC2209 drivers** in UART mode:
   - Remove all three microstepping jumpers under the X and Y driver slots on the RAMPS shield.
   - For TMC2209 UART addressing, set address pins:
     - **X Axis (Address 0)**: Connect MS1 and MS2 to GND.
     - **Y Axis (Address 1)**: Connect MS1 to VCC (5V), MS2 to GND.
2. Insert the stepper drivers into the slots. 
   - **CRITICAL**: The potentiometer adjustment screw must face **away** from the green power input terminals. Placing them backward will burn the drivers.

#### C. TMC2209 UART Control Wiring (for Sensorless Homing)
1. Install a **1kΩ resistor** inline between the UART pin (PDN_UART) of the X driver and the Arduino Mega **Pin 40 (AUX-2)**.
2. Install a **1kΩ resistor** inline between the UART pin (PDN_UART) of the Y driver and the Arduino Mega **Pin A9 (AUX-2)**.
3. Connect the **DIAG** pin of the X driver to the **X_MIN Signal pin** (Pin 3).
4. Connect the **DIAG** pin of the Y driver to the **Y_MIN Signal pin** (Pin 14).
5. In `config.h`, set `#define HOMING_METHOD 1` to enable StallGuard sensorless homing.

#### D. Pen/Blade Servo Connection
1. The servo connects to the **SERVO-0** pins located on the far left of the RAMPS 1.4 shield.
2. Connect the **Brown/Black** ground wire to the `GND` pin.
3. Connect the **Red** power wire to the `5V` pin.
   - **NOTE**: The RAMPS 5V rail requires power. Make sure you place a jumper between the `5V` and `VCC` pins on the RAMPS board to route USB power to the servo, or use an external 5V BEC regulator if the servo draws too much current and resets the Mega.
4. Connect the **Orange/Yellow** signal wire to the signal pin (Arduino Mega pin 11).

#### E. Mega to ESP32 Serial Bridge Connections
Connect the Mega to the ESP32 DevKit to allow wireless control:
1. Connect **Mega TX1 (Pin 18)** to **ESP32 RX2 (GPIO 16)**.
2. Connect **Mega RX1 (Pin 19)** to **ESP32 TX2 (GPIO 17)**.
3. Connect **Mega GND** to **ESP32 GND** (essential for reference potential).
4. Connect **Mega 5V** to **ESP32 VIN** to power the ESP32 directly from the Mega.

---

## 2. ESP32 Standalone Configuration

In this setup, a single ESP32 handles both the G-code parsing, motion planning, and the Web UI.

| Signal | ESP32 GPIO | Description |
|:---|:---|:---|
| X_STEP | GPIO 26 | Step pulse for X axis stepper |
| X_DIR | GPIO 27 | Direction control for X axis |
| X_EN | GPIO 14 | Enable/Disable X motor |
| Y_STEP | GPIO 25 | Step pulse for Y axis stepper |
| Y_DIR | GPIO 33 | Direction control for Y axis |
| Y_EN | GPIO 32 | Enable/Disable Y motor |
| SERVO_0 | GPIO 13 | PWM control for pen/blade servo |
| X_LIMIT | GPIO 34 | Input for X axis limit switch |
| Y_LIMIT | GPIO 35 | Input for Y axis limit switch |
| TMC_TX | GPIO 17 | UART TX for TMC driver communication |
| TMC_RX | GPIO 16 | UART RX for TMC driver communication |
| GND | GND | Common reference ground |

---

## 3. Arduino Nano Standalone Configuration

A compact, low-cost option using an Arduino Nano (ATmega328P). Due to limited pins, it does not support WiFi/Bluetooth or UART TMC communication. It uses A4988 or DRV8825 drivers with traditional limit switches.

| Signal | Nano Pin | Description |
|:---|:---|:---|
| X_STEP | D2 | Step pulse for X axis |
| X_DIR | D5 | Direction control for X axis |
| X_EN | D8 | Enable/Disable all steppers |
| Y_STEP | D3 | Step pulse for Y axis |
| Y_DIR | D6 | Direction control for Y axis |
| SERVO_0 | D9 | PWM control for pen servo |
| X_LIMIT | D10 | X limit switch input |
| Y_LIMIT | D11 | Y limit switch input |
| GND | GND | Common reference ground |

---

## 4. Limit Switch Options (Sensored Homing)

If you are not using TMC2209 sensorless homing, connect physical limit switches to the endstop pins:
1. **Normally Closed (NC)**: Connect the switch between the **Signal** pin and **GND** pin. NC is highly recommended because a broken wire will trigger an immediate safety alarm.
2. In `config.h`, adjust the pull-up settings:
   - `#define LIMIT_PULLUPS_ENABLE true`
   - `#define LIMIT_INVERT_PLUG false` (for NC switches)
