# OpenPlotter — Troubleshooting & FAQ

This document covers common issues you might encounter during the assembly, configuration, and operation of your OpenPlotter cutting machine.

---

## 1. Stepper Motor Issues

### Motor hums/vibrates but does not rotate
*   **Cause**: Incorrect motor wiring or insufficient stepper driver current.
*   **Solutions**:
    1.  Check motor wire pairs. Use a multimeter to identify coil pairs. A single NEMA 17 stepper motor has two independent coils (A+/A- and B+/B-).
    2.  Check the driver current limit pot. If it is set too low, the motor won't have enough torque to turn. Adjust the small pot screw clockwise in small increments (10 degrees) to increase current.
    3.  Check if the stepper enable line is wired correctly.

### Motor rotates in the wrong direction
*   **Cause**: The direction pin is inverted, or motor coils are wired backward.
*   **Solutions**:
    1.  Flip the 4-pin motor connector on the RAMPS shield by 180 degrees.
    2.  Invert the axis direction in firmware using system commands:
        - For X axis: Send `$180=1` (or `$180=0` to revert).
        - For Y axis: Send `$181=1` (or `$181=0` to revert).
        - Save settings: `M500`.

### Drivers or motors are running extremely hot
*   **Cause**: Driver current limit (`Vref`) is adjusted too high.
*   **Solutions**:
    1.  Turn the potentiometer screw on the stepper driver counter-clockwise to reduce current limit.
    2.  Install small aluminum heatsinks on the driver ICs.
    3.  Add a cooling fan blowing directly over the drivers.

---

## 2. Homing and Endstop Issues

### Homing cycle triggers an immediate alarm
*   **Cause**: The limit switch is registered as triggered before moving.
*   **Solutions**:
    1.  Send `M119` to check endstop status.
    2.  Ensure limit switches are wired to the correct pins (e.g. Signal and GND).
    3.  If using normally open (NO) switches, they should report triggered when closed.
    4.  If switches are showing inverted behavior, adjust endstop configs in `config.h`.

### TMC2209 Sensorless homing grinds/crashes at the boundary
*   **Cause**: The StallGuard sensitivity threshold is set too low (not sensitive enough).
*   **Solutions**:
    1.  Send a lower value to the sensitivity register: `$161=30` (or lower).
    2.  Ensure the driver DIAG pin is securely connected to the Mega limit switch pin.
    3.  Verify the motor is running at a constant seek speed during homing. Sensorless homing requires constant speed, so verify your homing seek rate (`$150`) is appropriate.

---

## 3. Communication & Connection Issues

### Cannot connect via Web Serial (USB)
*   **Cause**: Another serial program is holding the port open, or baud rate mismatch.
*   **Solutions**:
    1.  Ensure you close Arduino Serial Monitor, Cura, or other slicers.
    2.  Double-check the baud rate in the connection modal. OpenPlotter defaults to `115200`.
    3.  Ensure your browser is Chrome, Edge, or Opera (Web Serial is not supported in Firefox or Safari).

### WiFi Connection drops or Web UI does not load (ESP32)
*   **Cause**: Weak signal, or ESP32 and PC are on different subnets.
*   **Solutions**:
    1.  If the ESP32 is in Access Point (AP) mode, verify your computer is connected to the SSID `OpenPlotter-XXXX`.
    2.  If in Station (STA) mode, verify the ESP32 joined your network and note the IP printed on the USB Serial terminal at boot.
    3.  If using `openplotter.local` mDNS hostname, ensure your network supports mDNS. If not, use the direct IP address (e.g., `192.168.1.50`).

---

## 4. Tool Actuation Issues

### Servo moves erratic or shakes
*   **Cause**: Electrical noise or voltage drops on the 5V rail.
*   **Solutions**:
    1.  Ensure you have a capacitor (e.g., 100µF to 1000µF) connected across the servo's power (+5V) and Ground (GND) lines near the servo connector.
    2.  If the servo is large, use an external 5V BEC (buck converter) power supply rather than the Arduino Mega 5V rail.

### Solenoid does not drop or gets hot
*   **Cause**: Solenoid is continuously drawing full current.
*   **Solutions**:
    1.  Use PWM holding current settings. Engage the solenoid at 100% duty cycle for 100ms, then drop PWM holding power to 30%-40% to maintain pull force without overheating.
