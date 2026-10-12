#ifndef FX_H
#define FX_H

#include "config.h"

// Small screen effects shared by the games.

// A "+10" that floats up from (x, y) for about half a second. Call it from
// update(); the manager draws it on top of the game's frame.
void fxPop(int x, int y, int value);
void fxPopText(int x, int y, const char* text);

void fxReset();   // forget all effects (a game starts)
void fxDraw();    // draws and ages the effects; called by the manager each frame

// Shutter wipe over the last frame, run before a result screen (about 0.3 s).
void fxShutter();

#endif
