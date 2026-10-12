#ifndef MENUICONS_H
#define MENUICONS_H

#include "config.h"

// Draws the 16x16 menu icon of a game at (x, y), magnified by an integer scale
// (1 = 16 px, 2 = 32 px). dim = checkerboard (half the pixels) for icons that
// sit in the background of the carousel.
void drawGameIcon(int gameId, int x, int y, int scale, bool dim);

#endif
