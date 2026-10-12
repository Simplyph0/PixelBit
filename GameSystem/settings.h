#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"

// The option each game was last started with, and which games the player has
// already seen the settings screens for, kept in EEPROM next to the high
// scores. Unknown or damaged data falls back to the games' defaults.
// flags: SETTING_TUTORIAL_DONE once the first-boot tour has been shown
#define SETTING_TUTORIAL_DONE 0x01
void loadGameOptions(uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint16_t* seen, uint8_t* flags);
void saveGameOptions(const uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint16_t seen, uint8_t flags);

// True until the first-boot greeting has been shown (nothing saved yet, or an older save)
bool isFirstBoot();

#endif
