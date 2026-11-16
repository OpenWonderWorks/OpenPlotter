# Release preparation

OpenPlotter releases should attach reproducible desktop and firmware assets to a versioned GitHub release. Do not commit generated installers or firmware build outputs to the source tree.

## Before preparing a release

1. Choose a version and update `app/package.json` if the desktop version changes.
2. Review the exact source commit, dependency lockfiles, third-party notices, and [licensing status](LICENSING.md).
3. Confirm CI passed for the exact commit and every firmware environment intended for the release.
4. Review the firmware configuration and release notes. A successful compile is not a hardware validation report.
5. Create and push a version tag only after the maintainers agree on the source commit.

## Build candidate assets

The manual `Release builds` GitHub Actions workflow accepts an existing tag and builds desktop targets on their native runners plus the configured PlatformIO firmware environments. Firmware candidate packages are uploaded only when that environment compiles; the primary `mega_ramps14` build and native firmware tests are required for a release candidate. It uploads workflow artifacts only. Review every artifact and its target name before publishing.

Artifacts should include:

- Windows portable desktop build
- macOS disk image
- Linux AppImage
- Firmware packages named for the PlatformIO environment they were built from
- SHA-256 checksums

Only include firmware environments whose individual release build jobs passed for this source commit. Note any required manual flashing steps and explicitly state that compile success does not establish physical board compatibility.

## Publish a public release

Publishing is a separate maintainer decision. After reviewing the `OpenPlotter-release-candidate` artifact, obtain explicit approval for its exact tag and source commit. Then manually run the `Publish reviewed release candidate` workflow with that tag and the run ID of the successful `Release builds` workflow. The publisher checks the artifact tag, source commit, and checksums before creating a release. Verify the resulting release page, assets, names, checksums, and notes before announcing it.

The old `release.js` publisher was removed because it contained a hard-coded old version and created a public release directly through the GitHub API without validating the current source tag or target assets.

## Release notes

For each release, record:

- Source tag and commit
- Desktop OS/architecture and any signing or notarization status
- Exact firmware environment for each binary
- Build workflow URL and checksum file
- Changes, known limitations, and required settings migration
- Hardware validation actually completed, if any; otherwise state `not hardware validated`
- Third-party license notices and any unresolved dependency or licensing items
