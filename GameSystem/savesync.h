#ifndef SAVESYNC_H
#define SAVESYNC_H

#include "config.h"

// Save transfer over a private, short-lived Wi-Fi access point.
// Every session gets a random 8-character SSID and a random 8-character WPA2
// password, shown only on the console's own screen. One client at a time;
// there is no time limit: it runs until it is stopped on the console
// (SETTINGS > WI-FI PAGE, LEFT stops it).

void syncInit();     // call once at boot
void syncBegin();
void syncEnd();
bool syncUpdate();   // false if the session is not running
bool syncDraw();     // draws the status screen; false when nothing changed (it refreshes twice a second), so the caller can skip presenting

#endif
