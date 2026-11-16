# Licensing review

This is a repository-level review, not a legal opinion. It records evidence from the checked-in files and direct dependency declarations; it does not replace a release-specific software bill of materials (SBOM) review.

## Current first-party license status

- There is no top-level `LICENSE` file.
- `OpenPlotter/OpenPlotter.ino` has a source banner that says `License: MIT`.
- `OpenPlotter/LICENSE.txt`, `Releases/LICENSE.txt`, and `test_cli/LICENSE.txt` contain GNU GPL version 3 text. These files are next to bundled `arduino-cli` binaries and archives, so they may be third-party notices. Their intended scope is not identified.
- The first-party app and Android scaffold do not have a clear repository-level license declaration.

These signals conflict or have unclear scope. Do not treat the firmware banner or a nearby full license text as proof that the entire repository is licensed under that license.

## Dependency snapshot

As of 2026-11-14, the checked-in app lockfile resolves 345 npm package entries; every entry has a license identifier in its metadata. The recorded identifiers and expressions include MIT, ISC, BSD, Apache-2.0, MPL-2.0 (including LightningCSS platform packages), BlueOak-1.0.0, 0BSD, WTFPL variants, Python-2.0, and `(MIT OR CC0-1.0)`. This metadata inventory is not a complete legal compatibility determination; review exact versions, expressions, and transitive notices before each release.

Firmware's declared PlatformIO dependencies include:

| Dependency | Declared use | Upstream license evidence |
| --- | --- | --- |
| TMCStepper | Stepper-driver control | [MIT](https://github.com/teemuatlut/TMCStepper/blob/master/LICENSE) |
| ESPAsyncWebServer | ESP32 web and WebSocket server | [LGPL-3.0](https://github.com/ESP32Async/ESPAsyncWebServer/blob/main/LICENSE) |
| AsyncTCP | ESP32 asynchronous networking | [LGPL-3.0](https://github.com/ESP32Async/AsyncTCP/blob/main/LICENSE) |
| ArduinoJson | ESP32 JSON support | [MIT](https://github.com/bblanchon/ArduinoJson/blob/7.x/LICENSE.txt) |

The desktop app also downloads Arduino CLI and Arduino libraries through its flash workflow. Those downloaded versions and their notices must be included in release review. Bundled executables and firmware binaries in `Releases/` should retain their upstream notices; do not relicense third-party files as first-party code.

## Recommendation for maintainers

For broad reuse of the original project code, MIT is a practical project-wide recommendation, consistent with the firmware source banner. Before applying it, maintainers must confirm they have the rights to license all first-party files, clarify whether the nested GPLv3 texts are only Arduino CLI notices, and preserve separate notices for third-party components. If the project intends to require source sharing for derivative firmware, GPL-3.0-only is a reasonable alternative, but it must be chosen explicitly rather than inferred from the adjacent binary license files.

No repository-wide license has been applied by this review. Maintainer approval and rights confirmation are still required before adding or changing a license file.

## Release checklist

- Generate and review an SBOM for the exact desktop and firmware build inputs.
- Include third-party license notices for packaged dependencies and downloaded tools.
- Preserve the upstream license and source offer where a distributed component requires it.
- Re-run the audit when `package-lock.json`, `platformio.ini`, Gradle dependencies, or bundled tool versions change.
