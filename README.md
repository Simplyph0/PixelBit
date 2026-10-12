# PixelBit

PixelBit is a handheld arcade you can fit in your pocket. It's an ESP8266, a tiny OLED screen, and four buttons. It has 14 games, and you can link two of them together and play each other. No wifi or router needed. It runs PixelOS v4.0.

![PixelBit](images/finished_console.jpg)

> ## Credit
> **This is a fork of [Arduino-Game-System](https://github.com/innif/Arduino-Game-System) by Finn Harms ([innif](https://github.com/innif)).** All credit for the hardware, the case, the build instructions and the first eight games goes to them. The case and the original project are on [MakerWorld](https://makerworld.com/en/models/1526306-tiny-retro-arcade-8-games). Go star the [original repo](https://github.com/innif/Arduino-Game-System) and thank them.
>
> **What I added:** six new games, rewrites of the original eight, a new menu, per-game settings, a welcome animation and tour for the first boot, two-console play, a manufacturing test mode, a PC emulator, and automated tests.
>
> **About the AI:** I used an AI coding assistant (Claude) to write most of the code in this fork. I told it what to build, tested everything, and ran it on a real console. I'm saying it up front because I'd rather you hear it from me.
>
> This is under the **GNU General Public License v3.0**, same as the original. See [LICENSE.md](LICENSE.md).

## Contents

- [Games](#games)
- [Features](#features)
- [Build it](#build-it)
- [Install the firmware](#install-the-firmware)
- [Using the console](#using-the-console)
- [Playing a friend](#playing-a-friend)
- [Saved data](#saved-data)
- [Manufacturing test mode](#manufacturing-test-mode)
- [For tinkerers](#for-tinkerers)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## Games

| Game | What it has |
|---|---|
| Snake | Five speeds, solid or wrap-around walls, queued turns |
| Tetris | Next piece, hold, ghost piece, wall kicks, hard drop, start level |
| Flappy Bird | Five difficulty levels (gap size and speed) |
| 2048 | 4x4 or 5x5 board, three undos |
| Breakout | Five levels, power-up capsules (wide paddle, multiball, slow ball, extra life) |
| Frogger | Traffic and river logs, three lives, gets faster every crossing |
| Helicopter | Scrolling cave, five difficulty levels |
| Pac-Man | Five mazes from small to full screen, power pellets, ghosts that each hunt differently, three lives |
| Pong | Vs the computer (five AI levels), vs a friend, or practice against a wall. Spin and optional power-ups |
| Invaders | Shields, a bonus ship, waves |
| Dino | Endless runner with day and night |
| Tic-Tac-Toe | Best of five vs the computer (five levels) or a friend |
| Rock Paper Scissors | First to three, vs an AI that learns your habits or a friend |
| Connect 4 | Best of three vs a computer that looks up to ten moves ahead (five levels) or a friend |

## Features

- Settings for each game (difficulty, mode, board size, etc) that get remembered between power-ups
- Two-console play with host and join, short unit IDs, the host can accept or deny, and encrypted packets
- High scores that actually save
- An animated menu with an icon and best score for every game
- A welcome animation and a short tour on the first boot
- Little screen effects like score pops and shutter wipes
- A manufacturing test mode for checking new units, see [Manufacturing test mode](#manufacturing-test-mode)
- A PC emulator and automated tests, see [tools/](tools/README.md)

## Build it

The full step by step build guide with photos is in **[Instructions.md](Instructions.md)**. If you already know your way around a soldering iron, this is the short version.

### Parts
- ESP8266 D1 Mini
- 0.96" I2C OLED (128x64, SSD1306, address 0x3C)
- 4 push buttons (6x6x5 mm)
- Perforated grid board
- 2 M3 countersunk screws (5 to 10 mm), thin wire, solder

### Wiring

| Part | ESP8266 pin | What it does |
|---|---|---|
| OLED SDA | D2 (GPIO4) | I2C data |
| OLED SCL | D1 (GPIO5) | I2C clock |
| OLED VCC | VIN | 5V power, same as the original build |
| OLED GND | GND | Ground |
| Button Up | D6 (GPIO12) | Input, other side to ground |
| Button Down | D7 (GPIO13) | Input, other side to ground |
| Button Left | D4 (GPIO2) | Input, other side to ground |
| Button Right | D5 (GPIO14) | Input, other side to ground |

If your buttons or display wires are flipped, change `FLIPPED_BUTTONS` or `FLIPPED_I2C` at the top of `GameSystem/config.h` instead of rewiring.

### Case
The 3D printed case is the original author's design: [Tiny Retro Arcade on MakerWorld](https://makerworld.com/en/models/1526306-tiny-retro-arcade-8-games). It has cutouts for the display and buttons, a USB port, and a snap-fit lid with screws.

## Install the firmware

1. Install the [Arduino IDE](https://www.arduino.cc/en/software/).
2. Add ESP8266 support. Go to File > Preferences and add `http://arduino.esp8266.com/stable/package_esp8266com_index.json` to "Additional Boards Manager URLs". Then install **esp8266** in Tools > Board > Boards Manager.
3. Install **Adafruit SSD1306** and **Adafruit GFX Library** (with their dependencies) from the Library Manager.
4. Open `GameSystem/GameSystem.ino`.
5. **Change `LINK_SECRET` in `GameSystem/config.h` to your own random text.** Two consoles can only play each other if they have the same secret, and it keys the encryption.
6. Pick the board **LOLIN(WEMOS) D1 R2 & mini**, pick your port, and hit Upload.

Heads up: `LINK_SECRET` is sitting in the source, so anyone with your firmware can read it. It keeps other people and other builds out of your games, but it won't stop someone who has your firmware.

## Using the console

### First boot
A new console plays a welcome animation ("Welcome to your new PixelBit!") and then a short tour. The tour waits for you to actually press each button.

It only counts as done when you acknowledge it. That means you either finish the tour, or skip it (hold any button for 1.5 seconds) and close the "TOUR SKIPPED" message with **Up**. If the power gets cut before that, it plays again next boot. Want to see it again later? Hold **Down** on the menu for a second.

### Menu

| Button | What it does |
|---|---|
| Left / Right | Scroll through the games (hold to keep scrolling) |
| Up | Start the selected game |
| Down (tap) | Change that game's settings. Games that have some show `vSET` under the carousel |
| Down (hold) | Replay the tour |

The first time you open a game with settings, you get one screen per setting. **Left/Right** changes it, **Up** goes to the next one, **Down** goes back. After that the game starts right away with your last choices.

### In any game
- Hold **Up + Down** for a second to leave.
- On the result screen, **Up** goes back to the menu and **Right** plays again.

### Controls

| Game | Controls |
|---|---|
| Snake | Arrows turn |
| Tetris | Left/Right move (hold to repeat), Up rotates, Down soft drops (double tap for a hard drop), Left + Right together holds a piece |
| Flappy Bird | Up or Right flaps |
| 2048 | Arrows slide, Left + Right together undo (three per game) |
| Breakout | Left/Right move the paddle, catch the capsules |
| Frogger | Arrows hop |
| Helicopter | Up or Right lifts |
| Pac-Man | Arrows steer |
| Pong | Up/Down move the paddle |
| Invaders | Left/Right move, Up or Down fires |
| Dino | Up or Right jumps, Down ducks |
| Tic-Tac-Toe | Left/Right step through the empty squares, Down jumps a row, Up places your mark |
| Rock Paper Scissors | Left/Right pick a hand, Up locks it in |
| Connect 4 | Left/Right pick a column, Up or Down drops the disc |

## Playing a friend

Pong, Tic-Tac-Toe, Rock Paper Scissors and Connect 4 work between two consoles. No router, no wifi network.

1. On both consoles, open the game and pick **2 PLAYER** in its settings.
2. One console picks **HOST GAME**, the other picks **JOIN GAME**.
3. The joiner sees nearby hosts by their 5 character ID. Pick one with **Left/Right** and press **Up**.
4. The host sees the request. **Up** accepts it, **Down** denies it.

Both consoles need the same firmware version and the same `LINK_SECRET`. Each console's ID comes from its chip, and the link uses the full 48 bit hardware address underneath, so two consoles can't get mixed up.

## Saved data

High scores, each game's settings, and the "tour done" flag live in the ESP8266's emulated EEPROM. Everything has a checksum, so if a block gets damaged it just goes back to defaults. A normal firmware upload keeps your data. Picking "Erase Flash: All Flash Contents" in the Arduino IDE wipes it.

## Manufacturing test mode

This is for checking new units as you build them. Every new console shows the welcome animation and the tour on its first boot, and that only gets saved once the tour is acknowledged:

- you finish the tour (the last page, **Up: Finish**), or
- you skip it (hold any button for 1.5 seconds) and close the "TOUR SKIPPED" message with **Up**.

If the power gets cut during the animation, the tour, or the message, the unit greets its owner again next boot. The regular PixelOS splash only takes over after the tour has been acknowledged.

### Getting in
On the welcome animation or any page of the tour, press **Left three times** (each press within 3 seconds of the last), then **hold Up** for one second.

Test mode never marks the tour as acknowledged, so a tested unit still greets its new owner. Picking **EXIT** restarts the console into a normal first boot.

### The tests
Move with **Left/Right** and run a test with **Up**. The list shows OK, FAIL, or -- for each one.

| Test | What it does |
|---|---|
| DISPLAY | Full screen patterns: all white, all black, two checkerboards, lines, a border with diagonals, and a grid. **Up/Right** goes to the next one, **Left** goes back, **Down** leaves. It's marked OK once all eight have been shown. Look for dead or stuck pixels |
| BUTTONS | Shows each button live, and a dot stays once a button has been seen. OK when all four have been pressed. Hold **Up + Down** for a second to leave |
| STORAGE | Writes four bit patterns to a scratch area and reads them back from the EEPROM buffer and from the flash chip itself. FAIL shows the first bad byte. It doesn't touch scores or settings |
| RADIO | Starts the radio and scans for other consoles in test mode. **Up** switches between scanning and sending beacons, so two units can find each other (one scans, one beacons). OK once another unit has been seen. Shows the unit's MAC and short ID |
| SYSTEM | Firmware version, ID, chip ID, MAC, real and configured flash size, free memory, uptime, and the last reset reason. If the flash sizes don't match, you probably have the wrong board setting or a fake chip |

The emulator has the same mode (run it without `--no-tutorial`). There the storage test only checks the memory buffer, and the radio test needs two emulators running with `--udp 0` and `--udp 1` to find anyone.

## For tinkerers

### What's in the repo

| Folder or file | What it is |
|---|---|
| `GameSystem/` | The firmware. Open `GameSystem.ino` |
| `tools/emulator/` | A PC emulator that runs the real firmware in a window. Screenshots, GIFs, input recording and replay |
| `tools/tests/` | Automated checks for the game rules, the AIs, two-console play and saved data. Run `make test` in there |
| `tools/bridge/` | Firmware for an ESP32 board so the emulator can play a real console |
| `Instructions.md`, `images/` | The original build guide and photos |

### How the firmware is laid out
- `gamemanager` runs the menu, the settings screens, the result screens, the tour and the games. Every game is a class that implements the `Game` interface in `game.h`.
- `display`, `input`, `ui` and `fx` are the shared screen, button and effect helpers.
- `gamelink`, `lobby` and `crypto` are the two-console link (ESP-NOW with SHA-256 based pairing and encryption) and the shared host and join screens.
- `highscore` and `settings` handle the saved data.
- `tutorial`, `welcome` and `testmode` are the first boot stuff and the manufacturing test.

### Adding a game
1. Make a class that implements `Game`. That's init, update, draw, game over, and score. If it has settings, also `optionCount()`, `option()` and `setOption()`.
2. Add its ID to `GameID` and bump `MAX_GAMES` in `config.h`.
3. Add it to the table in `gamemanager.cpp` and give it an icon in `menuicons.cpp`.

### Try it without the hardware
```
cd tools/emulator && make && ./emulator
```
Check [tools/README.md](tools/README.md) for the keys, replay files, and how to run two emulators against each other.

## Troubleshooting

- **Blank display:** check the SDA and SCL wires and the display address (0x3C).
- **A button does nothing or does the wrong thing:** check its wire against the pin table, or try `FLIPPED_BUTTONS` in `config.h`.
- **Upload fails:** check the board and port, and make sure the USB cable can carry data and not just power.
- **Two consoles can't find each other:** both need the new firmware and the same `LINK_SECRET`. Keep them within a few meters, and have the host pick **HOST GAME** first.
- **The tour shows up every time:** it only stops once you acknowledge it (see First boot). If your scores and settings also don't survive a restart, reflash and open an issue.
- **Serial output:** 115200 baud. Set `DEBUG_SERIAL` to 1 in `config.h` for detailed logs.

## License

GNU General Public License v3.0, see [LICENSE.md](LICENSE.md). The original project and its author are credited at the top. If you build on this, keep those notices and share your changes under the same license.

---

Have fun with it.
