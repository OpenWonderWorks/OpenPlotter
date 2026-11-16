# Hardware and wiring safety

This guide is a safety checklist, not a board-specific wiring schematic. OpenPlotter does not currently publish a verified wiring map for every board, shield, driver, motor, and tool combination. Use the exact manufacturer's manual and the pin definitions for the firmware profile you selected.

## Before wiring or changing settings

- Disconnect the machine from mains and remove USB power before changing any motor, driver, endstop, or tool wiring. Wait for the power supply capacitors to discharge.
- Do not connect or disconnect a stepper motor while its driver is powered. This can destroy the driver and can cause unexpected motion.
- Keep mains wiring inside a suitable enclosure. If you are not qualified to work on mains circuits, use a certified supply with enclosed input terminals and ask a qualified person to wire it.
- Check the voltage, polarity, and current rating of the supply, controller, drivers, motors, and tool. A connector that fits is not evidence that the parts are electrically compatible.
- Keep a physical way to remove motor power within reach. Firmware emergency-stop, hold, homing, and soft-limit features are not substitutes for a hardware cutoff.
- Secure the frame and workpiece, clear the motion area, and keep hands away from moving parts.

## Identify the configured pins

The default firmware profile is `BOARD_MEGA_RAMPS14` in `OpenPlotter/openplotter_config.h`. Profile selection, board-specific pins, and feature flags are defined in:

- `OpenPlotter/openplotter_config.h`
- `OpenPlotter/src/hal/pins/pins_ramps14.h`
- `OpenPlotter/src/hal/pins/pins_mega.h`
- `OpenPlotter/src/hal/pins/pins_nano.h`
- `OpenPlotter/src/hal/pins/pins_esp32.h`

Check the profile and the exact board revision before connecting anything. RAMPS clones and other shields can differ. Do not rely on pin numbers from a different board revision or an old online diagram.

## Stepper motors and drivers

1. Identify each motor coil pair from its data sheet or with a multimeter. Wire each pair to one driver coil output using the driver's labels.
2. Confirm the driver's orientation, current limit, microstep jumpers, enable polarity, and voltage range from that driver's documentation.
3. Set motor current conservatively. Monitor driver and motor temperature during initial testing; stop if either overheats or behaves unexpectedly.
4. With power removed, verify that the motor shaft turns freely and that wires cannot snag on the frame.
5. On first power-up, test one axis at a time with the tool lifted and a low jog speed. If direction is wrong, power down and correct the configured inversion or coil wiring only after checking the manufacturer's instructions.

Do not assume TMC UART/SPI control is active just because a TMC driver is installed. It requires a matching firmware profile, driver type, wiring, address or chip-select, and board pin mapping.

## Endstops and homing

- Use the switch terminals and input voltage specified by the controller and switch manufacturer. Do not apply 5 V to a signal pin unless the board documentation says that input is 5 V tolerant.
- Configure normally-open/normally-closed logic and pull-up behavior to match the actual wiring. The current defaults are in `OpenPlotter/openplotter_config.h`.
- Hard-limit polling uses X_MIN and Y_MIN by default. Enable a MAX or Z switch with its `HARD_LIMIT_*_ENABLED` setting only after that input is wired and tested; an open NC input is reported as triggered.
- Before homing, query and manually test every endstop. Verify that the firmware reports the expected state when each switch is pressed and released.
- Verify homing direction, travel clearance, and seek speed with motors disabled or disconnected where safe, then use low speed with the tool lifted. Keep the cutoff within reach.
- Do not use sensorless homing unless the specific driver and firmware configuration support it and you have validated it on that machine.

## Servo and other tools

- Confirm servo voltage and stall current. Many servos should use a separate regulated supply rather than the controller's 5 V rail.
- When using a separate tool supply, follow the board and tool instructions for grounding and isolation. Do not join grounds blindly when a design uses isolated outputs.
- Keep blades, solenoids, and other actuators physically disengaged during initial motion checks. Test their range and pressure with material removed.
- Ensure tool commands leave the tool raised when a job ends, is stopped, or the controller resets.

## First movement checklist

1. Remove the blade or lift the pen.
2. Set conservative speeds, acceleration, travel limits, and tool pressure.
3. Confirm endstop state and homing direction before homing.
4. Jog a short distance on one axis, then check actual direction and distance.
5. Repeat for the other axis and verify that the emergency power cutoff stops motion.
6. Run a small air cut before cutting material.

If the machine does not behave as expected, remove power and inspect wiring before changing settings. Include your board model/revision, driver, firmware profile, and relevant pin configuration in a bug report.
