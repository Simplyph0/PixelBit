#ifndef UI_H
#define UI_H

#include "config.h"
#include "game.h"

// Shared screens for pickers and notices (128x64, 21 characters per line).

extern const char* const DIFFICULTY_NAMES[DIFFICULTY_LEVELS];

// Ready-made options. OPT_DIFFICULTY has the five levels with the strength bar;
// OPT_MODE_AI is "VS AI" / "2 PLAYER", OPT_MODE_PRACTICE adds "PRACTICE".
extern const GameOption OPT_DIFFICULTY;
extern const GameOption OPT_MODE_AI;
extern const GameOption OPT_MODE_PRACTICE;

// Option screen: game name, option name, value big with arrows, a strength bar
// or a position indicator, and a hint line.
void drawOptionScreen(const char* title, const GameOption& option, int value, const char* hint);

// One option out of several, shown big with arrows on the sides.
void drawChoiceScreen(const char* title, const char* value, const char* line,
                      const char* hint, bool leftArrow, bool rightArrow);

// Title, an optional big line, up to two small lines and a hint at the bottom.
void drawMessageScreen(const char* title, const char* big, const char* line1,
                       const char* line2, const char* hint);

#endif
