#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"

// The option each game was last started with, and which games the player has
// already seen the settings screens for, kept in EEPROM next to the high
// scores. Unknown or damaged data falls back to the games' defaults.
// flags: SETTING_* bits below
#define SETTING_TUTORIAL_DONE 0x01
#define SETTING_LAYOUT_SAVED 0x02      // the button layout below has been stored
#define SETTING_BUTTONS_FLIPPED 0x04   // the console is built upside down
void loadGameOptions(uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint16_t* seen, uint8_t* flags);
void saveGameOptions(const uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint16_t seen, uint8_t flags);

// True until the first-boot greeting has been shown (nothing saved yet, or an older save)
bool isFirstBoot();

#endif
