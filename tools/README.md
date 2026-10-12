# Tools

## emulator/
Runs the real console firmware on your PC, so you can test games, menus and two-console play without hardware.

    cd tools/emulator && make
    ./emulator                                     # a window (needs SDL2), else the terminal
    ./emulator --udp 0    and    ./emulator --udp 1   # two emulated consoles talk to each other
    ./emulator --bridge /dev/ttyACM0               # an emulated console on the real radio, through the bridge board

Needs the Adafruit GFX library (installed by the Arduino IDE). SDL2 is optional: with it you get a window that sees real key presses and releases; without it, or with `--tty`, the screen is drawn in the terminal and a key counts as held for a moment because terminals send no key release. Scores and settings are not saved.

| Key | Action |
|---|---|
| arrows / WASD | the four buttons |
| P (or F12) | screenshot to `shots/` |
| G | start / stop a GIF recording, saved to `gifs/` |
| R | start / stop recording your button presses to `recordings/` |
| H | toggle the pixel grid (window only) |
| Q / Esc | quit |

Options: `--eeprom file` (keep scores and settings in a file between runs, so you can test saving and the first-boot greeting), `--no-tutorial` (skip the first-boot tour, which otherwise shows at every emulator start because nothing is saved), `--fast` (skip the delay that imitates the real display's refresh), `--speed N` (run N times faster than real time), `--seed N` (fixed random seed), `--headless` (no screen, for scripts), `--replay file`.

A replay file is plain text, times in milliseconds since the emulator started (the splash screen takes the first two seconds):

    seed 1
    2600 8        # at 2.6 s hold RIGHT (bits: 1 up, 2 down, 4 left, 8 right)
    2730 0        # release
    3500 shot menu   # save shots/menu.png
    9000 quit

Run `make test` in `tools/tests` to check the game rules, the AIs, the two-console games and that scores, settings and the first-boot flag are really saved and survive a restart.

`R` writes a file in this format, so a recording can be replayed with `./emulator --replay recordings/input-....txt --seed N` (start recording right after launching the emulator). With `--headless --fast --speed 8` this makes automatic screenshot tours and tests.

## bridge/
Firmware for an ESP32 board (tested target: ESP32-C3 SuperMini) that carries ESP-NOW
packets between the air and the PC over USB, so the emulator can play against a real
console. `micropython/main.py` is the quick way (flash MicroPython, copy the file);
`bridge/bridge.ino` is the Arduino version.
