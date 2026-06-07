# Troubleshooting

This guide covers common issues and how to fix them in OpenPlotter.

## 1. Alarm Locks (`ALARM: Hard limit` or `ALARM: Soft limit`)

If your machine suddenly stops and the terminal displays an `ALARM` state:
- **Hard Limit**: A limit switch was triggered during a non-homing move. Ensure your cables are securely connected and not picking up EMI (electromagnetic interference). If EMI is the issue, twist your endstop wires or add a 104 (0.1µF) capacitor across the endstop signal and ground pins.
- **Soft Limit**: A command tried to send the machine past its defined bed dimensions (`$130` and `$131`). Ensure your design is placed inside the valid cutting area in the Companion App.

**To clear an alarm**, type `$X` in the console, or click the **Kill Alarm ($X)** button in the Safety tab.

## 2. Firmware Compilation Fails (Flash Orchestrator)

If the Flash Orchestrator fails to compile the firmware:
- Open the **Log Panel** at the bottom of the app to see the compiler output.
- `unable to find numeric literal operator 'operator""f'`: Ensure you are using version `3.2.3+` of the companion app, as this bug was patched.
- `arduino-cli not found`: The app should automatically install `arduino-cli`. If it didn't, restart the app while connected to the internet.

## 3. Serial Port Connection Fails

If clicking "Connect Machine" fails:
- **Windows**: You may need to install the CH340 or CP2102 drivers depending on your Arduino clone.
- Ensure no other software (like Arduino IDE or Cura) is currently connected to the COM port. Only one program can connect at a time.
- The default baud rate for OpenPlotter is `115200`.

## 4. Machine Moves the Wrong Way

If your X or Y axis moves in the opposite direction of what you expect:
- **Hardware Fix**: Power off the machine, and flip the 4-pin stepper motor connector 180 degrees.
- **Software Fix**: Use the OpenPlotter Companion App -> **Machine Settings** -> **Invert X** or **Invert Y** checkboxes, then click **Flash Firmware**.

## 5. Stepper Motors Overheating

If your stepper motors are too hot to touch:
- **A4988 / DRV8825**: Turn the tiny screw on the driver counter-clockwise slightly to reduce the VREF (current limit).
- **TMC2209 (UART Mode)**: Open the Companion App, lower the `Motor Current (mA)` in the settings, and re-flash.
