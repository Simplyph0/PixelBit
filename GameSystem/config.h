// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// Button Pins
// Set FLIPPED_BUTTONS to 1 when the perfboard is turned over (rotated 180
// degrees): every button is then in the opposite position, so UP and DOWN swap
// pins and LEFT and RIGHT swap pins. This is fixed when the firmware is built:
// change it and upload again to switch.
#define FLIPPED_BUTTONS 0

#if FLIPPED_BUTTONS
  #define PIN_BUTTON_UP 13
  #define PIN_BUTTON_DOWN 12
  #define PIN_BUTTON_LEFT 14
  #define PIN_BUTTON_RIGHT 2
#else
  #define PIN_BUTTON_UP 12
  #define PIN_BUTTON_DOWN 13
  #define PIN_BUTTON_LEFT 2
  #define PIN_BUTTON_RIGHT 14
#endif

// Display pins. These are tried first; if no display answers there, the two are
// swapped, so a display wired the other way round still works.
#define PIN_SDA 4
#define PIN_SCL 5

// Display Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// System identity
#define OS_NAME "PixelOS"
#ifndef OS_VERSION
  #define OS_VERSION "v4.2"
  #define OS_VERSION_NUM 402       // the same version as a number (major * 100 + minor); firmware updates compare this
#endif
#define SPLASH_MS 2000

// Firmware updates between two consoles (test mode > UPDATE). 1 = built in. Set to
// 0 if you never want a console to accept or send firmware over the radio; read
// the security notes in the README first.
#define ENABLE_P2P_UPDATE 1

// Frame timing (see frame.h): a frame that redraws the screen lasts FRAME_MS,
// any other frame waits at most FRAME_IDLE_MS before the next one.
#define FRAME_MS 45
#define FRAME_IDLE_MS 16

// Game Constants
#define MAX_GAMES 14
#define BUTTON_DELAY 150
#define MENU_REPEAT_DELAY_MS 350   // hold LEFT/RIGHT this long before the menu starts scrolling
#define MENU_REPEAT_MS 110         // then it steps this often
#define TOUR_HOLD_MS 1000          // hold DOWN on the menu this long to replay the tour
#define TEST_HOLD_MS 1000          // hold UP this long (after LEFT three times) for manufacturing test mode
#define TOUR_SKIP_MS 1500          // hold any button this long to skip the tour
#define EXIT_HOLD_MS 1000   // hold UP + DOWN together this long to leave a game

// Hidden save-transfer unlock: on the menu enter the code (each press
// within SECRET_STEP_MS of the last), then hold UP for SECRET_HOLD_MS before
// SECRET_ARM_MS runs out. Codes: 0 = LEFT, 1 = UP, 2 = DOWN, 3 = RIGHT.
// There is no on-screen hint until the link starts.
#define SECRET_LEN 8
#define SECRET_STEP_MS 4000
#define SECRET_DEBOUNCE_MS 60
#define SECRET_ARM_MS 8000
#define SECRET_HOLD_MS 2000

// Save link: 0 = visible in Wi-Fi lists, 1 = hidden (must be added by hand).
// Double-tap UP within these limits to stop it (the minimum rejects bounce).
#define SYNC_HIDDEN_SSID 0
#define SYNC_DOUBLE_MIN_MS 60
#define SYNC_DOUBLE_MAX_MS 500
static const uint8_t SECRET_SEQ[SECRET_LEN] = {0, 0, 3, 3, 1, 1, 0, 3};   // L L R R U U L R

#define DIFFICULTY_LEVELS 5
#define DIFFICULTY_DEFAULT 1   // NORMAL
#define MAX_GAME_OPTIONS 3

// Game IDs
enum GameID {
  GAME_SNAKE = 0,
  GAME_TETRIS = 1,
  GAME_FLAPPY = 2,
  GAME_2048 = 3,
  GAME_BREAKOUT = 4,
  GAME_FROGGER = 5,
  GAME_HELICOPTER = 6,
  GAME_PACMAN = 7,
  GAME_PONG = 8,
  GAME_INVADERS = 9,
  GAME_DINO = 10,
  GAME_TICTACTOE = 11,
  GAME_RPS = 12,
  GAME_CONNECT4 = 13
};

// Game States
enum GameState {
  STATE_MENU,
  STATE_OPTIONS,
  STATE_WELCOME,
  STATE_TEST,
  STATE_TUTORIAL,
  STATE_PLAYING,
  STATE_GAME_OVER,
  STATE_GAME_WON,
  STATE_SYNC
};

// Console-to-console link (versus play). Both consoles must be built with the
// same LINK_SECRET: it keys the pairing messages and the session encryption, so
// consoles flashed with a different secret cannot join or read your games.
// Change it to your own random text before you flash. It is in the source, so
// treat it as protection against other players and stray radios, not against
// someone who has your firmware.
#ifndef LINK_SECRET
  #define LINK_SECRET "7f7ed3fcb5cbc3298ac3fccdf3421cfc"
#endif
#define LINK_CHANNEL 1          // Wi-Fi channel both consoles use (1 - 13)

// Serial debug logging; set to 1 while developing. Off by default: button and
// menu logging costs time every frame, and the sync log would print the
// Wi-Fi password.
#define DEBUG_SERIAL 0
#if DEBUG_SERIAL
  #define LOG(...) Serial.printf(__VA_ARGS__)
#else
  #define LOG(...) ((void)0)
#endif

// Global display object
extern Adafruit_SSD1306 display;

#endif
