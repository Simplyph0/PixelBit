#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"

// The option each game was last started with, and which games the player has
// already seen the settings screens for, kept in EEPROM next to the high
// scores. Unknown or damaged data falls back to the games' defaults.
// flags: SETTING_* bits below
#define SETTING_TUTORIAL_DONE 0x01
// 0x02 and 0x04 held a saved button layout in v4.2; the layout now comes from
// config.h only, so those bits are ignored (and left unused)
// seen: bit n is set once game n's settings screens have been shown (21 games at most, see settings.cpp)
void loadGameOptions(uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint32_t* seen, uint8_t* flags);
void saveGameOptions(const uint8_t values[MAX_GAMES][MAX_GAME_OPTIONS], uint32_t seen, uint8_t flags);

// True until the first-boot greeting has been shown (nothing saved yet, or an older save)
bool isFirstBoot();

#endif
