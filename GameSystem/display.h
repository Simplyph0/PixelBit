#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

void initDisplay();
void showSplash();
void clearDisplay();
void updateDisplay();
void drawCenteredText(const char* text, int y, int textSize = 1);

#endif