# Troubleshooting

## The companion app does not open

- Confirm Node.js is `22.12+` and run `npm ci` from `app/`.
- Run `npm run electron:start`; it builds the Vite UI before starting Electron.
- For build errors, run `npm run build` and keep the first error and complete output for a bug report.
- On Linux, confirm your desktop session can launch Electron and has access to the selected serial device.

## The serial port is missing or busy

- Close other software using the port, including serial monitors and firmware tools.
- Check the USB cable, board power, operating system device list, and any USB-to-serial driver required by the board.
- On Linux, confirm your user has the operating system's serial-port permission. Log out and back in after changing group membership.
- Reconnect only after the port is released. The default firmware baud rate is `115200`.

## The board does not compile or flash

- Confirm the board selection, firmware profile, and serial port match the hardware.
- Check network access if the first build needs PlatformIO packages or Arduino libraries.
- Read the first compiler or uploader error in the full log; later errors can be consequences.
- Use `cd OpenPlotter && pio run -e mega_ramps14` for the default PlatformIO profile. Select another environment only when it matches your board.
- Do not flash a binary from a different environment or board profile.

## Homing or endstops behave unexpectedly

- Stop motion and remove power if a switch does not stop the expected axis.
- Check the exact switch terminals, polarity, pull-up setting, board pin mapping, and homing direction against your board and firmware profile.
- Query endstop state and test switches by hand before homing.
- Do not enable sensorless homing unless the installed driver and wiring support it. Keep an independent cutoff within reach.

## Motion direction or distance is wrong

- Keep the tool raised and test short moves at low speed.
- Check steps per millimeter, axis inversion, microstep configuration, motor coil pairs, and travel limits.
- Change one setting at a time, then re-test. Power down before changing motor or driver wiring.

## Tool pressure or servo behavior is unsafe

- Remove material and blade contact. Check servo supply voltage and available current using the component manuals.
- Reduce pressure and test tool up/down commands with the machine stationary.
- If the tool does not reliably raise on stop or reset, disconnect tool power and do not run jobs.

## Report a reproducible issue

Include the operating system, Node version, app version or commit, exact board model and revision, shield and driver models, selected firmware profile, connection type, steps to reproduce, expected and observed behavior, and relevant logs. Remove Wi-Fi passwords, serial numbers, tokens, and other private values before posting logs publicly.
