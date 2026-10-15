#ifndef DEVICE_H
#define DEVICE_H

#include "config.h"

// Console-wide settings chosen in SETTINGS (settingsmenu.h): how bright the
// screen is and when it turns itself off. Both are kept in the EEPROM and apply
// from power-on.
#define BRIGHTNESS_LEVELS 5
#define SLEEP_CHOICES 4

struct DeviceSettings {
  uint8_t brightness;   // 0..BRIGHTNESS_LEVELS-1
  uint8_t sleep;        // 0 = never, else an index into the idle times
  uint8_t chat;         // 1 = listen for chat messages in the background
};

extern const char* const BRIGHTNESS_NAMES[BRIGHTNESS_LEVELS];
extern const char* const SLEEP_NAMES[SLEEP_CHOICES];

void initDevice();                      // loads the settings and applies the brightness (after initHighscores)
const DeviceSettings& deviceSettings();
void setBrightness(uint8_t level);      // applies and saves
void setSleep(uint8_t choice);          // saves
void setChat(bool on);                  // saves
void resetDeviceSettings();             // factory defaults (HIGH, never off, chat on), saved
// Called once per frame after updateInput(). Turns the screen off after the idle
// time and wakes it on a button press (that press is swallowed so waking up
// does not also act). keepAwake: something is going on that needs no buttons.
void updateDevice(bool keepAwake);
// Turns the screen on (and restarts the idle timer): a message has arrived
void wakeScreen();

#endif
