# OpenPlotter

OpenPlotter is a source repository for motion-control software for XY drawing and cutting plotters. It contains an Electron companion app, Arduino-style firmware, and an Android UI scaffold. The root [LICENSE](LICENSE) contains GPLv3 terms; the firmware source banner still says MIT, so confirm the scope of those notices before redistributing or reusing code.

**Project status:** active development. The code and board profiles are not a claim of broad hardware validation. Verify your wiring and configuration, test with the tool lifted, and keep a safe way to remove motor power before running a job.

## Repository map

| Path | Purpose |
| --- | --- |
| `app/` | Electron companion app and browser UI |
| `OpenPlotter/` | Firmware source, board configuration, and PlatformIO project |
| `docs/` | Setup, wiring, G-code, and troubleshooting guides |
| `android-app/` | Early Android UI scaffold; it does not currently control a machine |

## Get started

Install Node.js `22.12+`, then run the desktop app:

```sh
git clone https://github.com/OpenWonderWorks/OpenPlotter.git
cd OpenPlotter/app
npm ci
npm run electron:start
```

`electron:start` builds the web UI before opening Electron, so it works from a clean checkout. For the browser version, run `npm run dev` from `app/` and open the local URL printed by Vite in Chrome or another browser that supports Web Serial.

See the [setup guide](docs/SETUP_GUIDE.md) before connecting a controller or flashing firmware.

## Firmware and hardware

The checked-in configuration selects Mega 2560 with RAMPS 1.4 by default. Other configured profiles cover standalone Mega, ESP32 standalone, and Mega/ESP32 hybrid setups. Nano configurations remain experimental: current builds exceed the ATmega328P flash and RAM limits, so they are disabled in the companion app and omitted from downloadable assets. A configured profile does not mean that its board, driver, or wiring has been verified on hardware. Review the [wiring and safety guide](docs/WIRING_GUIDE.md), use the board manufacturer's pinout, and validate one axis at a time.

Build firmware from the repository root with PlatformIO:

```sh
cd OpenPlotter
pio run -e mega_ramps14
```

Other configured environments are listed in `OpenPlotter/platformio.ini`. Build only for the board and shield you have configured.

The [G-code reference](docs/GCODE_REFERENCE.md) documents commands implemented by the firmware. The companion app and firmware are still evolving; check the issue tracker for current limitations.

## Development checks

From `app/`:

```sh
npm ci
npm test
npm run build
```

Firmware build and test commands are documented in [CONTRIBUTING.md](CONTRIBUTING.md). GitHub Actions checks app tests, locked dependency advisories, the firmware build matrix, Android scaffold, and desktop packaging on pull requests. Passing a compile job confirms a build, not safe operation on a particular machine.

## Downloads

Check [GitHub Releases](https://github.com/OpenWonderWorks/OpenPlotter/releases) for versioned desktop and firmware assets. Read each release note and verify the board target before installing firmware. Builds are not signed or hardware-certified unless a release explicitly says otherwise.

## Contributing

Bug reports, focused fixes, documentation improvements, and reproducible build reports are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md) and use the issue templates for bugs or feature requests.

## License status

The root `LICENSE` contains GPLv3 terms, while the firmware banner still says MIT. See [docs/LICENSING.md](docs/LICENSING.md) for the evidence and remaining scope question; keep third-party notices separate.
