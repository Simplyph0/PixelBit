#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

void initDisplay();
void showSplash();
void clearDisplay();
void updateDisplay();

// The notification blip: a round dot in the top right corner, drawn over whatever is on the
// screen each time the frame is presented (and taken off the buffer again afterwards, so no
// game ever has to know about it). It sits in a black disc so it shows on bright and busy
// pictures alike. setNotifyBlip(true) keeps it on; notifyAttention() makes it flash quickly for
// a few seconds, as when a message has just arrived.
void setNotifyBlip(bool on);
void notifyAttention();
void drawCenteredText(const char* text, int y, int textSize = 1);

#endif