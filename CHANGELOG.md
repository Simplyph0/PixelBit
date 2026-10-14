# Changelog

What changed in each version, newest first. The release notes on the
[Releases](https://github.com/Simplyph0/PixelBit/releases) page start with the
matching section from this file. Add your changes under **Unreleased**; when a
version is tagged, rename that heading to the tag (for example `## v4.3`) and start
a new **Unreleased** section above it. Earlier versions are described in their
release notes.

## Unreleased

- Every release now has a second file, `PixelBit-<version>-flipped.bin`: the same firmware built with `FLIPPED_BUTTONS` set to 1 for a board built upside down. `FLIPPED_BUTTONS` in `config.h` can now be overridden by the compiler (`-DFLIPPED_BUTTONS=1`), and the release notes list a checksum for each file. The button layout is part of the firmware, so a console-to-console update copies the sender's layout: update between consoles built the same way round.
- `pbv.py` handles GIFs properly: animated GIF, WebP and APNG files convert like any video, and their transparent parts are laid over a solid colour first (black by default, `--background white` to change). Before, ffmpeg dropped the transparency and whatever colour was hidden under a transparent pixel came out as the picture, so a sticker-style GIF could turn into a solid block. Other inputs convert exactly as before.

## v4.3

- Changed: the button layout is set in `config.h` again (`FLIPPED_BUTTONS`, `0` by default; `1` for a board turned over) and is fixed when the firmware is built. The v4.2 run-time layout is gone: the first-boot tour no longer learns it and test mode has no LAYOUT page. A console that saved a layout under v4.2 now follows `config.h`; the two settings bits it used are ignored.
- The video player no longer keeps its two picture buffers (about 2 KB) in RAM while it is not playing; they are taken when a clip opens. The clip list's JSON buffer (about 0.7 KB) moved off permanent RAM too.
- The firmware update tests use fixed version numbers (4.2 receiver, 4.3 newer sender, 4.0 older sender), so raising `OS_VERSION` no longer changes what they check.
- New: **VIDEOS** in the menu plays black and white videos with no sound. Pick a clip from the list (Up plays, Left/Right pick, Down leaves), pause with Up, jump 5 seconds with Left/Right, Down goes back to the list.
- Videos are uploaded over Wi-Fi from the last entry of the list (the console makes a password-protected network and a web page that also lists and deletes clips). They live in the console's flash file system (LittleFS): about 1.9 MB for up to 16 clips with the default `4MB (FS:2MB OTA:~1019KB)` flash setting, which the release builds now name explicitly. Keep the same Flash Size on every console you update from or to: where the file system sits is part of the firmware, so an update that moves it erases the videos.
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
