#ifndef SAVESYNC_H
#define SAVESYNC_H

#include "config.h"

// Save transfer over a private, short-lived Wi-Fi access point.
// Every session gets a random 8-character SSID and a random 8-character WPA2
// password, shown only on the console's own screen. One client at a time;
// there is no time limit: double-tap UP on the console to stop it.

void syncInit();     // call once at boot
void syncBegin();
void syncEnd();
bool syncUpdate();   // false if the session is not running
void syncDraw();

#endif
