# Contributing

Thanks for helping improve OpenPlotter. Small, reproducible changes are easier to review, especially for firmware and hardware behavior.

## Before opening an issue or pull request

- Check existing issues and the current `main` branch.
- For a bug, include exact reproduction steps, expected behavior, actual behavior, and relevant logs. Use the bug report form.
- For a hardware report, include board and revision, shield, driver, tool, firmware profile, connection type, and a wiring/config summary. Do not post passwords, tokens, or serial numbers.
- Clearly mark hardware that you have not personally tested. A successful build is not evidence that a wiring combination is safe or works on a physical machine.

## Development setup

### Companion app

Install Node.js `22.12+`, then:

```sh
cd app
npm ci
npm test
npm run build
npm run electron:dev
```

`electron:dev` builds the local UI before launching Electron. To work on the browser UI, run `npm run dev` and use the local URL printed by Vite.

### Firmware

Install PlatformIO Core to build firmware. A C++17 compiler is also needed for the native safety/parser tests.

```sh
cd OpenPlotter
pio run -e mega_ramps14
cd ..
sh OpenPlotter/test/run-native.sh
```

The available environments are listed in `OpenPlotter/platformio.ini`. Build only for a board profile you can identify and configure. Do not flash or test motion on hardware until pin mapping, power, driver settings, homing direction, and limits have been reviewed.

### Android scaffold

`android-app/` is an experimental UI scaffold, not a machine controller. Its Gradle checks are run in CI; feature work should not imply mobile control support until a working communication and safety path exists.

## Change guidelines

- Keep each pull request focused and explain the user-visible behavior it changes.
- Add or update a regression test for behavior changes. Run the focused test first, then the relevant suite.
- Keep hardware-facing behavior conservative. Prefer rejecting unsafe or unsupported commands over silently accepting them.
- Document the board, profile, wiring, and configuration assumptions next to hardware-specific changes.
- Use the repository's existing JavaScript and C++ style. Avoid formatting unrelated files.
- Update setup, G-code, or troubleshooting docs when behavior changes.
- Do not add credentials, compiled artifacts, generated `node_modules`, or private hardware logs.
- Preserve third-party license notices. The project-level license is unresolved; see [docs/LICENSING.md](docs/LICENSING.md) and do not add or change license text without maintainer approval.

## Pull request checklist

- [ ] I described the behavior and any limitations.
- [ ] I added or updated tests for behavior changes and ran them.
- [ ] I ran the app build or the relevant firmware build.
- [ ] I updated docs if setup, commands, safety, or supported profiles changed.
- [ ] I stated whether physical hardware was tested; I made no unsupported compatibility claims.
- [ ] I checked that no secrets, binaries, or unrelated files were added.
