# Changelog

What changed in each version, newest first. The release notes on the
[Releases](https://github.com/Simplyph0/PixelBit/releases) page start with the
matching section from this file. Add your changes under **Unreleased**; when a
version is tagged, rename that heading to the tag (for example `## v4.3`) and start
a new **Unreleased** section above it. Earlier versions are described in their
release notes.

## Unreleased

- New: **VIDEOS** in the menu plays black and white videos with no sound. Pick a clip from the list (Up plays, Left/Right pick, Down leaves), pause with Up, jump 5 seconds with Left/Right, Down goes back to the list.
- Videos are uploaded over Wi-Fi from the last entry of the list (the console makes a password-protected network and a web page that also lists and deletes clips). They live in the console's flash file system (LittleFS), about 900 KB for up to 16 clips.
- New converter `tools/video/pbv.py` (Windows, macOS, Linux; needs Python 3 and ffmpeg) turns any video into a `.pbv`. `--pixel 1` to `4` trades detail for how easy the picture is to see and for file size, and there are dither, contrast, brightness, invert, fit and fps options.
- Every console ships with one demo clip inside the firmware, copied into the file system the first time the player opens (the one in the repo is a test pattern; `pbv.py --embed` replaces it).
- The menu shows "NO SOUND" under VIDEOS where games show the best score.
- Tests: the video converter and the console's decoder are checked against each other, and the real player runs in the emulator.
- The emulator and the tests now run on Linux, macOS and Windows (MinGW-w64/MSYS2). Build them with CMake, see `tools/README.md`.
- GitHub Actions runs the tests on all three systems before every release build.
- The emulator opens its SDL window by default on Windows and macOS, and its `--bridge` option takes a Windows COM port or a macOS `/dev/cu.*` device.
- The tests are much quicker (about one minute instead of six): fewer Tic-Tac-Toe games for the AI check, the two-console games play side by side, and the firmware update test runs on a faster simulated clock.
- The first-boot saved-data test is now a Python script (`persistence_check.py`) instead of a shell script, so it runs on Windows too.
- Release notes now start with the matching section of this changelog.
