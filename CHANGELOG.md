# Changelog

What changed in each version, newest first. The release notes on the
[Releases](https://github.com/Simplyph0/PixelBit/releases) page start with the
matching section from this file. Add your changes under **Unreleased**; when a
version is tagged, rename that heading to the tag (for example `## v4.3`) and start
a new **Unreleased** section above it. Earlier versions are described in their
release notes.

## Unreleased

- The emulator and the tests now run on Linux, macOS and Windows (MinGW-w64/MSYS2). Build them with CMake, see `tools/README.md`.
- GitHub Actions runs the tests on all three systems before every release build.
- The emulator opens its SDL window by default on Windows and macOS, and its `--bridge` option takes a Windows COM port or a macOS `/dev/cu.*` device.
- The tests are much quicker (about one minute instead of six): fewer Tic-Tac-Toe games for the AI check, the two-console games play side by side, and the firmware update test runs on a faster simulated clock.
- The first-boot saved-data test is now a Python script (`persistence_check.py`) instead of a shell script, so it runs on Windows too.
- Release notes now start with the matching section of this changelog.
