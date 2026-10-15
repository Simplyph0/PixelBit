# Tools

## emulator/
Runs the real console firmware on your PC, so you can test games, menus and two-console play without hardware.

Runs on Linux, macOS and Windows. Pick one way to build it:

**CMake (all three systems, recommended).** Downloads Adafruit GFX by itself if the Arduino IDE's copy is not found, and builds the tests too.

    cmake -S tools -B build/tools -G Ninja       # or leave out -G Ninja to use your default generator
    cmake --build build/tools
    build/tools/emulator                          # build\tools\emulator.exe on Windows
    ctest --test-dir build/tools --output-on-failure   # the tests

- **Linux:** `sudo apt install build-essential cmake ninja-build libsdl2-dev python3` (or your distribution's names).
- **macOS:** `xcode-select --install`, then `brew install cmake ninja sdl2`.
- **Windows:** install [MSYS2](https://www.msys2.org), open the "MSYS2 UCRT64" shell, then `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-python git` and run the commands above in that shell. Visual Studio's own compiler is not supported (the firmware sources use GCC extensions). To run the finished `emulator.exe` outside MSYS2, add `C:\msys64\ucrt64\bin` to your PATH (it holds `SDL2.dll`).

SDL2 is optional; without it the emulator only has the terminal screen. Pass `-DGFX=/path/to/Adafruit_GFX_Library` to use a copy you already have.

**Make (Linux, macOS, or MSYS2).** `cd tools/emulator && make`, and `make test` in `tools/tests`. It looks for Adafruit GFX in the Arduino IDE's folder (`~/Arduino/libraries`, or `~/Documents/Arduino/libraries` on macOS and Windows); set `GFX=...` if yours is elsewhere.

Run it:

    ./emulator                                     # a window (needs SDL2), else the terminal
    ./emulator --udp 0    and    ./emulator --udp 1   # two emulated consoles talk to each other
    ./emulator --bridge /dev/ttyACM0               # an emulated console on the real radio, through the bridge board

The bridge's port name is `/dev/ttyACM0` or similar on Linux, `/dev/cu.usbmodem...` on macOS and `COM3` or similar on Windows.

Needs the Adafruit GFX library (installed by the Arduino IDE, or downloaded by CMake). SDL2 is optional: with it you get a window that sees real key presses and releases; without it, or with `--tty`, the screen is drawn in the terminal and a key counts as held for a moment because terminals send no key release. The terminal screen uses UTF-8 braille characters and ANSI escapes: on Windows use Windows Terminal or the Windows 10 console. Scores and settings are not saved.

| Key | Action |
|---|---|
| arrows / WASD | the four buttons |
| P (or F12) | screenshot to `shots/` |
| G | start / stop a GIF recording, saved to `gifs/` |
| R | start / stop recording your button presses to `recordings/` |
| H | toggle the pixel grid (window only) |
| Q / Esc | quit |

Options: `--eeprom file` (keep scores and settings in a file between runs, so you can test saving and the first-boot greeting), `--no-tutorial` (skip the first-boot tour, which otherwise shows at every emulator start because nothing is saved), `--fast` (skip the delay that imitates the real display's refresh), `--speed N` (run N times faster than real time), `--seed N` (fixed random seed), `--headless` (no screen, for scripts), `--replay file`, and for the firmware update: `--firmware file.bin` (the image this emulator sends), `--update-out file.bin` (where a received image is saved) and `--free-space N` (pretend the update space is N bytes).

A replay file is plain text, times in milliseconds since the emulator started (the splash screen takes the first two seconds):

    seed 1
    2600 8        # at 2.6 s hold RIGHT (bits: 1 up, 2 down, 4 left, 8 right)
    2730 0        # release
    3500 shot menu   # save shots/menu.png
    9000 quit

`ctest -R firmware_update` (or `make update_test` followed by `python3 update_check.py` in `tools/tests`) runs two simulated consoles through the firmware update (newer, same, older, denied, bad hash, no space, sender vanishing, lossy radio) and checks what gets installed.

`ctest -R chat` (or `python3 chat_check.py .` in `tools/tests`) runs two simulated consoles through chat: the ping, a message that is acknowledged, one to everyone, one to a console that is not there, and repeats being dropped, with and without lost packets. To try chat by hand, run two emulators: `./emulator --udp 0 --eeprom a.bin` and `./emulator --udp 1 --eeprom b.bin`.

Run `ctest --test-dir build/tools` (or `make test` in `tools/tests`) to check the game rules, the AIs, the two-console games and that scores, settings and the first-boot flag are really saved and survive a restart. GitHub Actions runs the same tests on Linux, macOS and Windows before every release build (`.github/workflows/build.yml`).

The emulator's keys are always the logical buttons: the arrow you press is the button the game sees, whatever `FLIPPED_BUTTONS` is set to.

`R` writes a file in this format, so a recording can be replayed with `./emulator --replay recordings/input-....txt --seed N` (start recording right after launching the emulator). With `--headless --fast --speed 8` this makes automatic screenshot tours and tests.

## bridge/
Firmware for an ESP32 board (tested target: ESP32-C3 SuperMini) that carries ESP-NOW
packets between the air and the PC over USB, so the emulator can play against a real
console. `micropython/main.py` is the quick way (flash MicroPython, copy the file);
`bridge/bridge.ino` is the Arduino version.

## video/
`pbv.py` turns a video into a `.pbv` file for the console's video player (black and white, no sound). It needs Python 3 and ffmpeg and works on Windows, macOS and Linux. See [Videos](../README.md#videos) in the main README for the options (`--pixel 1` to `4` sets how much detail you trade for how easy the picture is to see), and `GameSystem/pbvcodec.h` for the file format.

    python3 video/pbv.py movie.mp4 --pixel 2 --fps 12
    python3 video/pbv.py --demo -o demo.pbv

In the emulator the console's flash file system is a folder: `pixelbit-fs` in the current directory, or the one named by the `PB_FS` environment variable. Put a `.pbv` in its `clips/` folder and the VIDEOS entry plays it. The upload web page is not emulated.

## flasher/
`pixelbit_flasher.py` is a point-and-click firmware flasher for Linux, macOS and Windows (Python 3 with tkinter; it finds esptool on PATH (pipx works) or offers to install it). Choose a `.bin`, and it flashes the lowest-numbered `ttyACM*`/`ttyUSB*` (macOS `cu.usb*`, Windows `COMx`) port at 460800 baud. The port list updates as you plug consoles in; select several to flash them in parallel. "Erase chip first" is off by default (it also deletes scores, settings and videos). "Flash new consoles as they appear" is for batches. A failed run is retried once at 115200.

    python3 tools/flasher/pixelbit_flasher.py
