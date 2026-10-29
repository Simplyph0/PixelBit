#ifndef HIGHSCORE_H
#define HIGHSCORE_H

#include "config.h"
#include <EEPROM.h>

#define EEPROM_SIZE 512
#define HIGHSCORE_BASE_ADDR 0
#define SAVE_SLOTS 32          // upper bound on games a save file can describe
#define SAVE_MAX_SCORE 999999

// EEPROM map: the scores at 0, the settings (settings.cpp) at SETTINGS_ADDR, the
// device settings (device.cpp) at DEVICE_ADDR, and
// the storage test's scratch bytes (test mode) at TEST_SCRATCH_ADDR. The checks
// below stop a build where adding games would make one run into the next.
#define SETTINGS_ADDR 128
#define DEVICE_ADDR 200          // 8 bytes: screen and chat settings (device.cpp)
#define CONTACTS_ADDR 220        // 4 + 8 * 14 + 1 bytes: chat aliases (contacts.cpp)
#define TEST_SCRATCH_ADDR 400
#define TEST_SCRATCH_LEN 64
static_assert(HIGHSCORE_BASE_ADDR + 8 + 4 * MAX_GAMES + 4 <= SETTINGS_ADDR, "the scores would run into the settings");
static_assert(SETTINGS_ADDR + 10 + MAX_GAMES * MAX_GAME_OPTIONS + 2 <= DEVICE_ADDR, "the settings would run into the device settings");
static_assert(DEVICE_ADDR + 8 <= CONTACTS_ADDR, "the device settings would run into the contacts");
static_assert(TEST_SCRATCH_ADDR + TEST_SCRATCH_LEN <= EEPROM_SIZE, "the test area is past the end of the EEPROM");
static_assert(MAX_GAMES <= 32, "the scores and settings tables are sized for 32 games at most");
static_assert(MAX_GAMES <= 21, "the settings keep 'options seen' bits for 21 games (settings.cpp)");

// Save format (version 1), little endian, identical in EEPROM and in exported files:
//   "GSAV" | version u8 | count u8 | reserved u16 | score i32 x count | crc32 u32
// The CRC covers everything before it. Because the file states its own game
// count, saves keep working when games are added in a later firmware.
class HighscoreManager {
private:
  int scores[MAX_GAMES];

  bool loadFromEEPROM();
  bool migrateLegacy();
  void resetToDefaults();

public:
  void init();
  void saveHighscore(int gameId, int score);
  int getHighscore(int gameId);
  bool isNewHighscore(int gameId, int score);
  void saveToEEPROM();
  void resetAllHighscores();

  // Transplantable saves: a text blob ("GSAV1-" + hex) safe to copy and paste.
  String exportText();
  // merge = keep the better score per game, otherwise overwrite from the file.
  // Returns nullptr on success or a short error message.
  const char* importText(const String& text, bool merge);
};

extern HighscoreManager highscoreManager;

void initHighscores();
int getGameHighscore(int gameId);
bool checkNewHighscore(int gameId, int score);
void saveGameHighscore(int gameId, int score);

#endif
