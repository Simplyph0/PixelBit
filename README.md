# PixelBit

PixelBit is a handheld arcade you can fit in your pocket. It's an ESP8266, a tiny OLED screen, and four buttons. It has 14 games, a video player, chat between consoles and a settings menu, and you can link two of them together and play each other. No wifi or router needed. It runs PixelOS v4.4.

![PixelBit](images/finished_console.jpg)

<p align="center"><img src="images/screens/menu.gif" width="420" alt="Scrolling through the PixelBit game menu"><br><sub>The menu on the OLED (screen capture from the PC emulator)</sub></p>

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
- [Videos](#videos)
- [Playing a friend](#playing-a-friend)
- [Chat](#chat)
- [Saved data](#saved-data)
- [Manufacturing test mode](#manufacturing-test-mode)
- [Console-to-console firmware update](#console-to-console-firmware-update)
- [For tinkerers](#for-tinkerers)
- [Troubleshooting](#troubleshooting)
- [License](#license)

## Games

<table>
<tr>
<td align="center"><img src="images/screens/snake.png" width="250" alt="Snake"><br><sub>Snake</sub></td>
<td align="center"><img src="images/screens/tetris.png" width="250" alt="Tetris"><br><sub>Tetris</sub></td>
<td align="center"><img src="images/screens/flappy.png" width="250" alt="Flappy Bird"><br><sub>Flappy Bird</sub></td>
</tr>
<tr>
<td align="center"><img src="images/screens/2048.png" width="250" alt="2048"><br><sub>2048</sub></td>
<td align="center"><img src="images/screens/breakout.png" width="250" alt="Breakout"><br><sub>Breakout</sub></td>
<td align="center"><img src="images/screens/frogger.png" width="250" alt="Frogger"><br><sub>Frogger</sub></td>
</tr>
<tr>
<td align="center"><img src="images/screens/helicopter.png" width="250" alt="Helicopter"><br><sub>Helicopter</sub></td>
<td align="center"><img src="images/screens/pacman.png" width="250" alt="Pac-Man"><br><sub>Pac-Man</sub></td>
<td align="center"><img src="images/screens/pong.png" width="250" alt="Pong"><br><sub>Pong (with a power-up)</sub></td>
</tr>
<tr>
<td align="center"><img src="images/screens/invaders.png" width="250" alt="Invaders"><br><sub>Invaders</sub></td>
<td align="center"><img src="images/screens/dino.png" width="250" alt="Dino"><br><sub>Dino</sub></td>
<td align="center"><img src="images/screens/tictactoe.png" width="250" alt="Tic-Tac-Toe"><br><sub>Tic-Tac-Toe</sub></td>
</tr>
<tr>
<td align="center"><img src="images/screens/rps.png" width="250" alt="Rock Paper Scissors"><br><sub>Rock Paper Scissors</sub></td>
<td align="center"><img src="images/screens/connect4.png" width="250" alt="Connect 4"><br><sub>Connect 4</sub></td>
</tr>
</table>

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
- High scores that actually save, with a Wi-Fi page (from SETTINGS) to download and restore them and to name your friends' consoles
- An animated menu with an icon and best score for every game
- A welcome animation and a short tour on the first boot
- Little screen effects like score pops and shutter wipes
- A manufacturing test mode for checking new units, see [Manufacturing test mode](#manufacturing-test-mode)
- Chat between consoles: ready-made messages ("Let's play Pong", "Good game!") over ESP-NOW with a notification blip on top of whatever you are doing, see [Chat](#chat)
- A settings menu on the console: brightness, screen-off timer, chat on/off, Wi-Fi page, firmware update, resets, intro and about, see [Settings](#settings)
- Console-to-console firmware updates over ESP-NOW, no cable or PC needed, see [Console-to-console firmware update](#console-to-console-firmware-update)
- Board built upside down? One setting (`FLIPPED_BUTTONS`) swaps the buttons, and swapped display pins are found on their own
- A video player: black and white clips, no sound, uploaded over Wi-Fi, made with a converter you can set for more detail or easier viewing, see [Videos](#videos)
- A one-page printable [quick start](docs/PixelBit-QuickStart.pdf) for new owners
- A PC emulator for Linux, macOS and Windows, and automated tests that run on all three, see [tools/](tools/README.md)
- Ready-made `.bin` files on the [Releases](https://github.com/Simplyph0/PixelBit/releases) page, built and tested automatically

<table>
<tr>
<td align="center"><img src="images/screens/menu-vset.png" width="250" alt="The menu with the vSET hint"><br><sub><code>vSET</code> under the game means it has settings</sub></td>
<td align="center"><img src="images/screens/settings-pong-mode.png" width="250" alt="Pong mode setting"><br><sub>One settings screen per option</sub></td>
<td align="center"><img src="images/screens/settings-pong-powerups.png" width="250" alt="Pong power-ups setting"><br><sub>Your choices are remembered</sub></td>
</tr>
</table>

## Build it

The full step by step build guide with photos is in **[Instructions.md](Instructions.md)** (also as a [PDF](Instructions.pdf)). If you already know your way around a soldering iron, this is the short version.

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

Built your board upside down, so every button is in the opposite spot? No need to rewire. Flash the `-flipped` `.bin` from the Releases page, or set `FLIPPED_BUTTONS` to `1` in `GameSystem/config.h` and upload again: Up and Down swap pins, and so do Left and Right. It is `0` (the wiring above) by default. If SDA and SCL are swapped, the firmware notices and uses them the other way round.

### Case
The 3D printed case is the original author's design: [Tiny Retro Arcade on MakerWorld](https://makerworld.com/en/models/1526306-tiny-retro-arcade-8-games). It has cutouts for the display and buttons, a USB port, and a snap-fit lid with screws.

## Install the firmware

1. Install the [Arduino IDE](https://www.arduino.cc/en/software/).
2. Add ESP8266 support. Go to File > Preferences and add `http://arduino.esp8266.com/stable/package_esp8266com_index.json` to "Additional Boards Manager URLs". Then install **esp8266** in Tools > Board > Boards Manager.
3. Install **Adafruit SSD1306** and **Adafruit GFX Library** (with their dependencies) from the Library Manager.
4. Open `GameSystem/GameSystem.ino`.
5. **Change `LINK_SECRET` in `GameSystem/config.h` to your own random text.** Two consoles can only play each other if they have the same secret, and it keys the encryption.
6. Pick the board **LOLIN(WEMOS) D1 R2 & mini**, pick your port, and hit Upload.
   Leave **Tools > Flash Size** at its default, `4MB (FS:2MB OTA:~1019KB)`: the videos are stored in that 2 MB file system, and the ready-made `.bin` files are built with it. A board with a smaller flash can still run everything else, but not the video player. Keep the same setting on every console you update from or to: where the file system sits is part of the firmware, so an update that moves it erases the videos.

No IDE? Every release on the [Releases](https://github.com/Simplyph0/PixelBit/releases) page has a ready `.bin`. Flash it with [esptool](https://github.com/espressif/esptool):

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x0 PixelBit-v4.4.bin
```

Prefer clicking? `tools/flasher/pixelbit_flasher.py` is a small window for Linux, macOS and Windows: pick the `.bin`, it finds the console's port (lowest-numbered first), flashes at 460800 baud and can flash several consoles at once. Chip erase is off by default because it also deletes scores and videos. It needs Python 3 with tkinter and esptool (`pipx install esptool` works). See [tools/](tools/README.md).

Each release has two files: `PixelBit-<version>.bin` for a board wired as above, and `PixelBit-<version>-flipped.bin` for one built upside down (every button in the opposite place). They are the same firmware with `FLIPPED_BUTTONS` set to 0 and 1.

The release builds use the `LINK_SECRET` that is in this repo, so they can play and update each other but anyone else's release build can join them too.

Heads up: `LINK_SECRET` is sitting in the source, so anyone with your firmware can read it. It keeps other people and other builds out of your games, but it won't stop someone who has your firmware.

## Using the console

New to it? Print the one-page, black and white **[quick start](docs/PixelBit-QuickStart.pdf)**: the four buttons, two-console games, chat, the Wi-Fi page, videos, settings, updating one console from another, and what to try when something is wrong.

### First boot
A new console plays a welcome animation ("Welcome to your new PixelBit!") and then a short tour. The tour waits for you to actually press each button.

<p align="center"><img src="images/screens/greeting.gif" width="420" alt="The welcome animation"><br><sub>The welcome animation</sub></p>

<table>
<tr>
<td align="center"><img src="images/screens/tour-welcome.png" width="250" alt="Tour welcome page"><br><sub>Tour start</sub></td>
<td align="center"><img src="images/screens/tour-menu.png" width="250" alt="A tour page asking to press right"><br><sub>Pages wait for you to press the button</sub></td>
<td align="center"><img src="images/screens/tour-leaving.png" width="250" alt="The hold up and down page"><br><sub>You try it for real</sub></td>
</tr>
</table>

It only counts as done when you acknowledge it. That means you either finish the tour, or skip it (hold any button for 1.5 seconds) and close the "TOUR SKIPPED" message with **Up**. If the power gets cut before that, it plays again next boot. Want to see it again later? Hold **Down** on the menu for a second.

### Menu

| Button | What it does |
|---|---|
| Left / Right | Scroll through the games (hold to keep scrolling) |
| Up | Start the selected game |
| Down (tap) | Change that game's settings. Games that have some show `vSET` under the carousel |
| Down (hold) | Replay the tour |

The first time you open a game with settings, you get one screen per setting. **Left/Right** changes it, **Up** goes to the next one, **Down** goes back. After that the game starts right away with your last choices.

### Settings
The last tile of the menu, **SETTINGS**, holds the things that need neither a computer nor typing (those stay on the Wi-Fi page). **Up/Down** scroll, **Right** changes an entry or opens it, **Left** goes back.

| Entry | What it does |
|---|---|
| BRIGHTNESS | Five steps, applied at once and kept. HIGH is the usual brightness |
| SCREEN OFF | The screen turns itself off after 1, 2 or 5 minutes without a button (NEVER by default). Any button wakes it, and that press does nothing else. It stays on while a video plays, during an update and on the Wi-Fi pages |
| CHAT | `ON` (the default) keeps the radio listening so messages reach you while you play, see [Chat](#chat). `OFF` saves a little power and the console is deaf until you open the CHAT tile |
| WI-FI PAGE | Starts a private Wi-Fi network and shows its name, a random password and the address on the screen. Join it with a phone or computer for the save backup and the chat contacts, see [Backing up your scores](#backing-up-your-scores). **Left** stops it |
| UPDATE FIRMWARE | Send or receive firmware between two consoles, see [Console-to-console firmware update](#console-to-console-firmware-update) |
| RESET SCORES | Erases every high score. Hold **Right** for 2 seconds to confirm |
| RESET OPTIONS | Every game forgets its remembered settings and asks again. Same confirmation |
| SHOW INTRO | Makes the welcome animation and the tour play again on the next boot (it clears the "tour seen" flag, nothing else) |
| FACTORY RESET | Back to a new console: scores, every game's settings, brightness, screen off, chat on, chat contacts and all videos are erased, and the console restarts into the welcome animation. Hold **Right** for 3 seconds to confirm |
| ABOUT | Version, this console's id (the one chat and the two-console games show), build date, button layout and free memory |

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

## Videos

The last entry in the menu, **VIDEOS**, plays black and white videos with no sound. A console ships with one demo clip, and you add more over Wi-Fi.

| Where | Button | What it does |
|---|---|---|
| Clip list | Up / Down | Scroll through the clips |
| Clip list | Right | Play it (the last entry, **+ UPLOAD VIDEOS**, opens the upload page) |
| Clip list | Left | Leave the player |
| Playing | Up | Pause and resume |
| Playing | Left / Right | Jump back or forward 5 seconds |
| Playing | Down | Back to the clip list |

There are no loop or speed settings. When a clip ends you are back at the list.

### Making a video
Videos are `.pbv` files, made on a computer with [`tools/video/pbv.py`](tools/video/pbv.py). It needs Python 3 and [ffmpeg](https://ffmpeg.org/download.html) (on the PATH, or `pip install imageio-ffmpeg`) and works the same on Windows, macOS and Linux.

```bash
python3 tools/video/pbv.py movie.mp4 --pixel 2 --fps 12
```

That writes `movie.pbv`. GIFs work too (and animated WebP and APNG): `python3 tools/video/pbv.py animation.gif --pixel 2`. Transparent parts of a GIF show as black, which is "off" on the screen; `--background white` shows them white instead. The screen is only 128x64, so you choose how much detail to trade for how easy it is to see with **`--pixel`**:

| `--pixel` | Video size | Good for |
|---|---|---|
| 1 | 128x64 | The most detail and the biggest files. Fine lines get hard to read on a 0.96" screen |
| 2 | 64x32 | A good middle: a quarter of the data, easy to follow |
| 3 | 42x21 | Chunky, small files |
| 4 | 32x16 | The chunkiest and tiniest: clear from across a desk |

Other options: `--fps` (1 to 30, default 15, and the console shows about 20 at most), `--dither bayer|threshold|floyd` (how greys become black and white: `threshold` keeps hard edges and suits line art, `floyd` is finest), `--threshold`, `--contrast`, `--brightness`, `--invert`, `--background black|white` (for transparent GIFs), `--fit letterbox|crop|stretch`, `--start` and `--duration`. `python3 tools/video/pbv.py --help` has all of them, and `--demo` makes a test pattern if you have no video at hand. The converter prints the size and warns if a clip will not fit: a console has about 1.9 MB of room for videos, and keeps up to 16.

### Putting it on the console
1. On the console open **VIDEOS**, go to **+ UPLOAD VIDEOS** and press **Right**. It starts its own Wi-Fi network and shows the name and a random password (new every time).
2. Join it with your phone or computer and open `http://192.168.4.1`.
3. Pick the `.pbv` file and press **Upload**. The console screen freezes until the upload is done. The same page lists your clips and deletes them.
4. Press **Left** on the console to stop the upload page.

### The clip in the firmware
Every console is built with one clip inside the firmware, [`GameSystem/demo_clip.h`](GameSystem/demo_clip.h), so a fresh console (and one updated from another console) has something to play. It is copied into the file system the first time the player opens, and is not copied back after you delete it. The one in this repo is a small test pattern. To ship your own, make a header and rebuild the firmware:

```bash
python3 tools/video/pbv.py movie.mp4 --pixel 2 --fps 10 --embed GameSystem/demo_clip.h
```

Keep it small, around 150 KB: it makes the firmware bigger, and so every console-to-console update longer. The converter warns above 200 KB.

## Playing a friend

Pong, Tic-Tac-Toe, Rock Paper Scissors and Connect 4 work between two consoles. No router, no wifi network.

1. On both consoles, open the game and pick **2 PLAYER** in its settings.
2. One console picks **HOST GAME**, the other picks **JOIN GAME**.
3. The joiner sees nearby hosts by their 5 character ID. Pick one with **Left/Right** and press **Up**.
4. The host sees the request. **Up** accepts it, **Down** denies it.

<table>
<tr>
<td align="center"><img src="images/screens/lobby-role.png" width="300" alt="Host or join choice"><br><sub>Host or join</sub></td>
<td align="center"><img src="images/screens/lobby-hosting.png" width="300" alt="The host waiting screen"><br><sub>The host waits and shows its ID</sub></td>
</tr>
<tr>
<td align="center"><img src="images/screens/lobby-find.png" width="300" alt="The joiner choosing a host"><br><sub>The joiner picks a host by ID</sub></td>
<td align="center"><img src="images/screens/lobby-request.png" width="300" alt="The host seeing a join request"><br><sub>The host accepts or denies</sub></td>
</tr>
</table>

Both consoles need the same firmware version and the same `LINK_SECRET`. Each console's ID comes from its chip, and the link uses the full 48 bit hardware address underneath, so two consoles can't get mixed up.

## Chat

The **CHAT** tile (after VIDEOS) sends ready-made messages to another console over the same radio as the two-console games: no Wi-Fi network, no router. Both consoles need this firmware.

- **NEW MESSAGE** asks who is in range, then who to send to: **EVERYONE**, a saved contact, or a console that just answered (marked `NEAR`, shown by its 5-letter id). Then pick the message: `LET'S PLAY...` (and the game, taken from the game list, so new games show up by themselves), `BEAT MY SCORE...` (your best score in that game goes along), `WANNA PLAY?`, `REMATCH?`, `GOOD GAME!`, `NICE ONE!`, `YES!`, `NO THANKS`, `MAYBE LATER`, `GIVE ME A MINUTE`, `HELLO!` and `BYE!`.
- A message to one console says **DELIVERED** when it arrived, or **NO REPLY** if the other console did not answer (out of range, chat OFF, or busy with the Wi-Fi pages). A message to everyone just says **SENT**.
- **INBOX** keeps the last 8 messages. **Up/Down** moves to the next one, **Right** answers the sender.
- A round **blip** flashes in the top right corner of the screen, over whatever you are doing (a game, the menu, a video), when a message arrives and stays until you have read it. It is drawn only when the screen is presented, so no game has to know about it.
- **Names.** Consoles show up as their id (the 5 letters on the CHAT screen, the same ones the two-console games use). Your own id is on the CHAT screen and in SETTINGS > ABOUT. Open SETTINGS > WI-FI PAGE (see [Backing up your scores](#backing-up-your-scores)) and add contacts there: type the id and a name. Names are 1 to 10 characters (letters, digits, space and `- _ . ! ? + *`), which always fits next to the rest of the line. Up to 8 contacts, kept in the EEPROM.

Messages are broadcast and checked with the link secret like the pairing packets, so only consoles built with the same `LINK_SECRET` take part. They are **not encrypted**: anyone with the same firmware in range can read them, which is fine for "Good game!". Turn the radio off completely with **SETTINGS > CHAT > OFF**. Listening keeps the radio on, which costs some battery.

## Saved data

High scores, each game's settings, the brightness, screen-off and chat settings, the chat contacts, and the "tour done" flag live in the ESP8266's emulated EEPROM. Everything has a checksum, so if a block gets damaged it just goes back to defaults. A normal firmware upload (or a console-to-console update) keeps your data. Picking "Erase Flash: All Flash Contents" in the Arduino IDE wipes it.

### Backing up your scores
Open **SETTINGS > WI-FI PAGE**. The console starts its own little Wi-Fi network and shows the name and a random password on screen (both are new every time). Join it with your phone and open `http://192.168.4.1`. From there you can:

- download your scores as a small text file,
- paste or upload one to restore it (keep the best of both, or overwrite),
- keep your **chat contacts** (an id and a name, see [Chat](#chat)),
- erase every score (you have to type RESET first).

Press **Left** on the console to switch it off again. Only one device can join at a time, and the network is gone once it's off. The save file has a checksum, so a damaged or edited one gets refused.

## Manufacturing test mode

This is for checking new units as you build them. Every new console shows the welcome animation and the tour on its first boot, and that only gets saved once the tour is acknowledged:

- you finish the tour (the last page, **Up: Finish**), or
- you skip it (hold any button for 1.5 seconds) and close the "TOUR SKIPPED" message with **Up**.

If the power gets cut during the animation, the tour, or the message, the unit greets its owner again next boot. The regular PixelOS splash only takes over after the tour has been acknowledged.

### Getting in
On the welcome animation or any page of the tour, press **Left three times** (each press within 3 seconds of the last), then **hold Up** for one second.

Test mode never marks the tour as acknowledged and saves nothing, so a tested unit still greets its new owner. Picking **EXIT** restarts the console into a normal first boot.

Buttons the wrong way round? Change `FLIPPED_BUTTONS` in `config.h` and upload again.

### The tests
Scroll with **Up/Down**, run a test with **Right**, and leave test mode with **Left**. The list shows OK, FAIL, or -- for each one.

<table>
<tr>
<td align="center"><img src="images/screens/test-list.png" width="300" alt="Test mode list"><br><sub>The test list</sub></td>
<td align="center"><img src="images/screens/test-buttons.png" width="300" alt="Button test screen"><br><sub>Button test</sub></td>
</tr>
<tr>
<td align="center"><img src="images/screens/test-display.png" width="300" alt="Display test pattern"><br><sub>Display test pattern</sub></td>
<td align="center"><img src="images/screens/test-system.png" width="300" alt="System info screen"><br><sub>System info</sub></td>
</tr>
</table>

| Test | What it does |
|---|---|
| DISPLAY | Full screen patterns: all white, all black, two checkerboards, lines, a border with diagonals, and a grid. **Up/Right** goes to the next one, **Left** goes back, **Down** leaves. It's marked OK once all eight have been shown. Look for dead or stuck pixels |
| BUTTONS | Shows each button live, and a dot stays once a button has been seen. OK when all four have been pressed. Hold **Up + Down** for a second to leave |
| STORAGE | Writes four bit patterns to a scratch area and reads them back from the EEPROM buffer and from the flash chip itself. FAIL shows the first bad byte. It doesn't touch scores or settings |
| RADIO | Starts the radio and scans for other consoles in test mode. **Up** switches between scanning and sending beacons, so two units can find each other (one scans, one beacons). OK once another unit has been seen. Shows the unit's MAC and short ID |
| SYSTEM | Firmware version, ID, firmware fingerprint, chip ID, MAC, real and configured flash size, free memory, uptime, and the last reset reason. If the flash sizes don't match, you probably have the wrong board setting or a fake chip |

The emulator has the same mode (run it without `--no-tutorial`). There the storage test only checks the memory buffer, and the radio test needs two emulators running with `--udp 0` and `--udp 1` to find anyone.

## Console-to-console firmware update

One console can copy its own firmware to another one over the air (ESP-NOW), so you can update a unit without a cable or a PC. On both consoles open **SETTINGS** (the last tile of the menu) and pick **UPDATE FIRMWARE**, then choose **SEND** on one and **RECEIVE** on the other. They pair the same way as the two-console games.

What happens:

1. The sender reads its own flash and works out a SHA-256 hash of its firmware.
2. It offers the version, size, and hash to the receiver.
3. The receiver checks the offer and shows a consent screen. **Up** installs, **Down** denies. Nothing is written until you press Up.
4. The firmware goes over in 192 byte chunks with acknowledgements, a window of 8 chunks in flight, and resends for anything lost. On two real consoles a 470 KB firmware took about 10 seconds; a poor radio link can make it take up to a minute.
5. The receiver hashes what it got and only installs it if the hash matches. Then it restarts.

Version rules: a newer version is offered normally. If the version number is the same, the receiver is told "Same version number as this build" and can still install it (handy for a unit that got a bad flash). An older version is refused.

Both kinds of console: the button layout is part of the firmware, so a console copies its sender's layout. Update between consoles built the same way round (or from a console with the matching `.bin`), or the buttons come out mirrored until you flash the right one.

Safety checks: the image has to fit in the free update space, has to start with a real firmware header, and has to match its hash. The receiver keeps its own flash settings (bytes 2 and 3 of the header) in case the two units have different chips. If anything fails, the link drops, or the sender walks away, the update is thrown away and the old firmware keeps running.

### If an update fails
A failed update never changes the receiver: the old firmware keeps running. The failure screen says why, and when a transfer stalls it also shows how far it got, in the line just above `UP: OK`. Press **Up** to leave it.

| Screen | Where | What it means |
|---|---|---|
| `TRANSFER STALLED` with `tx12 ack3 lost9` | Sender | Nothing moved for 10 seconds. `tx` is how many chunk packets it sent (repeats included), `ack` how many acknowledgements came back, `lost` how many packets the radio said the other console did not get. `ack0` with a high `lost` means the receiver is not hearing the sender; `ack0` with `lost0` means the sender's packets seem to be delivered but nothing answers |
| `TRANSFER STALLED` with `got 40 bad 0 at 38` | Receiver | `got` is how many chunk packets arrived, `bad` how many of them were not what the offer described, and `at` is the chunk it has written up to. `got0` means nothing reached it |
| `CANNOT START` with `updater error 2` | Receiver | The updater refused to open space for the image: usually not enough free update space, or a flash size set wrongly. The number is the updater's own error code |
| `UPDATE FAILED`, `Writing the image failed.`, with `chunk 12, error 3` | Receiver | Writing to the flash chip failed at that chunk |
| `UPDATE FAILED`, `did not match its checksum` | Both | The image arrived damaged. Try again, closer together |
| `REFUSED` | Sender | The receiver already has a newer version, has no room for the image, or is busy |
| `DENIED`, `CANCELLED` | Sender | The receiver pressed Down, or you cancelled |
| `NO ANSWER` | Sender | The receiver did not press Up within 40 seconds |
| `CONNECTION LOST`, `OTHER CONSOLE LEFT` | Either | The link dropped or the other console left test mode. Keep them within a few meters |
| `NO CONFIRMATION` | Sender | Everything was sent but no result came back; check the receiver, it may have installed and restarted already |

If an update still fails and you cannot tell why, build both consoles with `UPDATE_DEBUG` set to 1 (change `#define UPDATE_DEBUG 0` in `GameSystem/config.h`, or pass `-DUPDATE_DEBUG=1` to the compiler), open the serial monitor on each at 115200 baud, and try again. The update and the link then log every step: the offer, the accept, the first chunks, a line every 100 chunks, every dropped packet and why (wrong tag, wrong size, repeated sequence number), and any radio or updater error. A crash prints its stack trace there too. It is off in normal builds.

### What the hash looks like
The full SHA-256 is 64 hex characters, for example:

```
9def956fe2d9921e10120fdaa4c10e0d9ae27e5206b14ab07d68c9b4e1936999
```

The screens show the first 8 characters as two groups, so that example is **9DEF 956F**. You see it on the sender, on the receiver's consent screen, and on the test mode SYSTEM page (`FW 9DEF 956F 360K`). If both consoles show the same fingerprint, they have the same firmware.

### Security disclaimer
Read this before you use it. This is a hobby feature, not secure updating.

- The firmware is **not signed**. The hash only proves the file arrived in one piece. It does not prove who made it.
- The pairing secret (`LINK_SECRET` in `config.h`) is in this public repo, so anyone running this code can pair with your console. Radio packets are authenticated per session and numbered so they can't be replayed, but that is not a defence against someone who knows the secret.
- The only real gate is the consent screen. Someone nearby who opens SETTINGS > UPDATE FIRMWARE on their console and picks SEND could offer your console a malicious image, and if you press Up on it, you installed it. Only accept from a console you are looking at.
- The 8 character fingerprint is 32 bits. It is fine for "are these the same build", not for telling a good image from a crafted one. Compare the full hash if you care.
- A power cut in the middle of the final write can still brick a unit. If that happens, reflash over USB.
- Any console can open the receive screen from SETTINGS, so anyone holding yours can start it. A receiver only installs after someone presses Up on the consent screen. Turn the whole feature off with `ENABLE_P2P_UPDATE` below if that matters to you.

If you don't want the feature, set `ENABLE_P2P_UPDATE` to `0` in `GameSystem/config.h` and rebuild. The UPDATE FIRMWARE entry and the code are then left out.

In the emulator, add `--firmware file.bin` to load a real image to send, `--update-out file.bin` for where a received image is saved, and `--free-space N` to pretend the update space is smaller.

## For tinkerers

### What's in the repo

| Folder or file | What it is |
|---|---|
| `GameSystem/` | The firmware. Open `GameSystem.ino` |
| `tools/video/` | `pbv.py`, the converter that turns a video into a `.pbv` for the video player |
| `docs/` | `PixelBit-QuickStart.pdf`, the one-page black and white printable guide, and `make_quickstart.py` (needs LibreOffice) that rebuilds it from the emulator screenshots in `docs/screens/` |
| `tools/emulator/` | A PC emulator (Linux, macOS, Windows) that runs the real firmware in a window. Screenshots, GIFs, input recording and replay |
| `tools/tests/` | Automated checks for the game rules, the AIs, two-console play and saved data. Run `ctest --test-dir build/tools` (CMake) or `make test` in there |
| `tools/CMakeLists.txt` | Builds the emulator and the tests on Linux, macOS and Windows |
| `tools/bridge/` | Firmware for an ESP32 board so the emulator can play a real console |
| `Instructions.md`, `Instructions.pdf`, `images/` | The original build guide and photos |
| `.github/workflows/` | Builds, tests and publishes a release for every firmware change |

### How the firmware is laid out
- `gamemanager` runs the menu, the settings screens, the result screens, the tour and the games. Every game is a class that implements the `Game` interface in `game.h`.
- `display`, `input`, `ui`, `fx` and `frame` are the shared screen, button (layout and debouncing), effect and frame timing helpers.
- `gamelink`, `lobby` and `crypto` are the two-console link (ESP-NOW with SHA-256 based pairing and encryption) and the shared host and join screens. `GameLink::poll()` ignores a call made while it is already running, so a packet handler can wait for the radio (by calling `yield()` and `poll()`) without handling the next packet from inside itself and piling up stack.
- `highscore` and `settings` handle the saved data, and `savesync` is the Wi-Fi backup page.
- `fwupdate` is the console-to-console firmware update.
- `settingsmenu` is the SETTINGS tile (a `Game` like the others) and `device` keeps its brightness, screen-off and chat settings in the EEPROM.
- `chat` is the chat protocol, inbox and radio keep-alive, `chatmenu` is the CHAT tile and `contacts` keeps the aliases. `GameLink` carries chat packets in every state; `updateDisplay()` draws the notification blip.
- `videoplayer` and `pbvcodec` are the video player and its file format; the player stores clips in the flash file system (LittleFS).
- `tutorial`, `welcome` and `testmode` are the first boot stuff and the manufacturing test.

### Adding a game
1. Make a class that implements `Game`. That's init, update, draw, game over, and score. If it has settings, also `optionCount()`, `option()` and `setOption()`.
2. Add its ID to `GameID` and bump `MAX_GAMES` in `config.h`. A game with no score (like the video player) overrides `hasScore()` to return `false`.
3. Add it to the table in `gamemanager.cpp` and give it an icon in `menuicons.cpp`.

### Try it without the hardware
All the screenshots in this README came from the emulator, so you can make the same ones yourself.

```
cmake -S tools -B build/tools && cmake --build build/tools && build/tools/emulator
```
Check [tools/README.md](tools/README.md) for the keys, replay files, and how to run two emulators against each other.

## Troubleshooting

- **Blank display:** check the SDA and SCL wires and the display address (0x3C). Swapped SDA and SCL are found on their own. With `DEBUG_SERIAL` on, the serial monitor says what it found.
- **The buttons are mirrored (Left does Right, Up does Down):** change `FLIPPED_BUTTONS` in `GameSystem/config.h` (0 or 1) and upload again.
- **A button does nothing:** check its wire against the pin table.
- **Upload fails:** check the board and port, and make sure the USB cable can carry data and not just power.
- **Two consoles can't find each other:** both need the new firmware and the same `LINK_SECRET`. Keep them within a few meters, and have the host pick **HOST GAME** first.
- **The tour shows up every time:** it only stops once you acknowledge it (see First boot). If your scores and settings also don't survive a restart, reflash and open an issue.
- **A console-to-console update sticks at 0% and the receiver restarts with its old firmware:** Firmware from before this fix crashed the receiver a moment after the transfer started (the update packets were handled from inside each other until the stack ran out). A console running that older firmware cannot be updated over the air, so flash it once by cable; after that it can be. See [If an update fails](#if-an-update-fails) for what the failure screens mean.
- **Serial output:** 115200 baud. Set `DEBUG_SERIAL` to 1 in `config.h` for detailed logs. It's off in normal builds because it slows things down and prints the backup page's Wi-Fi password.

## License

GNU General Public License v3.0, see [LICENSE.md](LICENSE.md). The original project and its author are credited at the top. If you build on this, keep those notices and share your changes under the same license.

---

Have fun with it.
