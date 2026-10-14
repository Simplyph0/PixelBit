# Changelog

What changed in each version, newest first. The release notes on the
[Releases](https://github.com/Simplyph0/PixelBit/releases) page start with the
matching section from this file. Add your changes under **Unreleased**; when a
version is tagged, rename that heading to the tag (for example `## v4.3`) and start
a new **Unreleased** section above it. Earlier versions are described in their
release notes.

## Unreleased

- **Fixed: the console-to-console firmware update crashed the receiving console.** On real consoles the transfer sat at 0% on both screens, the sender then gave up with a stalled-transfer message, and the receiver restarted a few seconds later with its old firmware. The USB serial log of the receiver showed `Panic core_esp8266_main.cpp:191 __yield` right after the fifth chunk arrived, with this call chain: `sendAck()` called `poll()`, which handled the next queued chunk, which called `sendAck()` again, and so on. When the radio is still sending the previous packet, `sendAck()` waits for it, and it used to wait by calling `poll()`. The sender sends a burst of 8 chunks, so the receiver nested one level deeper for each queued chunk, each level costing a few hundred bytes of the 4 KB stack, until the stack overflowed. `GameLink::poll()` now does nothing when it is already running, so a packet handler can wait for the radio without handling more packets from inside itself. Checked on two real consoles with different flash chips: a 470 KB image went over in about 10 seconds (2,449 chunks, one packet lost and resent), the receiver checked the hash and restarted into the new firmware.
- Why the tests did not catch it: the simulated radio in the emulator and the test programs finishes a send at once, so a console never has to wait for the radio. The crash needs a real radio that is still busy when the next packet is ready.
- **New: a failed update says on the screen how far it got.** A stalled transfer shows `tx12 ack3 lost9` on the sender (chunk packets sent, acknowledgements received, packets the radio reported lost) and `got 40 bad 0 at 38` on the receiver (chunk packets received, chunk packets rejected, chunk written up to). "Cannot start" and "Writing the image failed" show the updater's own error code. The README has a new section, "If an update fails", explaining every failure screen and these numbers.
- **New: update debugging over USB serial.** Build with `UPDATE_DEBUG` set to 1 (in `config.h`, or `-DUPDATE_DEBUG=1`) and the update and the link log, at 115200 baud: the offer, the accept, the first chunks, a line every 100 chunks, every refused packet and why (wrong tag, wrong size, repeated sequence number), radio send errors, and the updater's result and free memory. It is off in normal builds and costs nothing then.
- Fixed: a receiver counted every chunk packet as proof that the sender was alive, even one it rejected, which could hide a stalled transfer. Only a valid chunk counts now, so a receiver that keeps getting bad packets gives up after the usual 10 seconds.
- Emulator and tests: the emulator's updater stand-in has `getError()` like the real one, so the new failure screens build there too.
- README: the update section now gives a realistic transfer time, lists the failure screens, explains how to capture a serial log, and the troubleshooting list has an entry for an update that sticks at 0%. The firmware layout notes explain the rule that `poll()` ignores calls from inside itself.
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
