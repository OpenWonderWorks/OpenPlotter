# Setup guide

This guide covers the companion app and firmware build. Read the safety notes in [WIRING_GUIDE.md](WIRING_GUIDE.md) before connecting a controller or motor.

## 1. Requirements

- Git
- Node.js `22.12+` and npm, for the companion app
- PlatformIO Core, for direct firmware builds
- A compatible USB serial driver for your controller

The Android directory is an experimental UI scaffold. It is not a machine-control app and is not part of the supported setup path.

## 2. Run the companion app

```sh
git clone https://github.com/OpenWonderWorks/OpenPlotter.git
cd OpenPlotter/app
npm ci
npm run electron:start
```

The start command builds `app/dist/` and then launches Electron. It needs network access the first time npm downloads dependencies. The app's firmware toolchain may also download Arduino tools and libraries when you use the flash workflow.

For the browser UI instead, run:

```sh
npm run dev
```

Open the local URL printed by Vite. Browser USB serial access requires a supported Chromium-based browser and a secure local origin such as `localhost`.

For development, `npm run electron:dev` builds the current UI and launches the desktop app. Changes require rebuilding/restarting. `npm test` runs the app's unit tests and `npm run build` checks the web bundle.

## 3. Choose a firmware profile

The checked-in default in `OpenPlotter/openplotter_config.h` is Mega 2560 with RAMPS 1.4. Other configured profiles cover standalone Mega, ESP32 standalone, and Mega/ESP32 hybrid setups. Nano and Nano hybrid entries are experimental and currently exceed ATmega328P flash and RAM limits; do not flash them. Select only a profile that matches the actual board and wiring; firmware profiles are not a statement of hardware certification.

Before flashing:

1. Confirm the exact board and shield revision from their labels and manufacturer documentation.
2. Check every step, direction, enable, endstop, and tool pin against the board pinout and the firmware profile.
3. Confirm motor-driver current, motor voltage, power supply, and tool power limits from the component documentation.
4. Disconnect power while changing wiring. Keep the cutting tool lifted and the work area clear during the first motion checks.
5. Set conservative speed, acceleration, travel limits, and homing settings before enabling movement.

Do not enable sensorless homing unless the installed driver, UART/SPI wiring, and configuration support it. Keep physical endstops installed during initial setup where the machine supports them.

## 4. Build firmware directly

Install PlatformIO Core, then from the repository root run:

```sh
cd OpenPlotter
pio run -e mega_ramps14
```

The selected environment must match the controller. Build outputs are written below `OpenPlotter/.pio/build/`. To flash, use PlatformIO's upload command only after confirming the environment and serial port:

```sh
pio run -e mega_ramps14 -t upload --upload-port PORT
```

Replace `PORT` with the detected device path. Do not copy firmware from a different environment to your board. You can also use the companion app's firmware workflow, which requires the configured Arduino toolchain and the correct board selection.

## 5. Connect and validate

1. Select the USB connection and a serial port. The default firmware baud rate is `115200`.
2. Connect with the tool raised and the emergency power cutoff within reach.
3. Query status and endstops; confirm reported state changes when switches are pressed by hand.
4. Firmware treats machine position as unknown after every controller restart. Confirm homing direction, switch polarity, speed, and clearance, then complete homing before motion or tool-on commands.
5. Jog a short distance at low speed in each axis direction. Stop immediately if direction, distance, or endstop behavior is unexpected.
6. Run a small air cut with the tool disengaged before trying material.

After an emergency stop or hard-limit alarm, release the cause, clear the alarm with `$X`, and home again before moving. The controller rejects motion until homing succeeds.

Use a real, independent power cutoff where practical. Software stop commands and soft limits are not substitutes for hardware safety controls.

## 6. Downloads and releases

Versioned desktop and firmware assets are listed on the [GitHub Releases page](https://github.com/OpenWonderWorks/OpenPlotter/releases). Read the release notes and check the target board before installing. A successful CI build confirms compilation only; it does not verify your wiring, machine dimensions, driver settings, or safe operation.

Maintainers can build release candidates with the manual GitHub Actions workflow described in [RELEASING.md](RELEASING.md). Public releases require an explicit maintainer decision.
