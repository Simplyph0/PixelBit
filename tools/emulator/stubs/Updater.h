#pragma once
#include "Arduino.h"
#define U_FLASH 0
// Stand-in for the core's firmware updater: the "installed" image goes to a file
// (see --update-out in the emulator) instead of the flash chip.
struct UpdaterClass {
  bool begin(size_t size, int command = U_FLASH, int ledPin = -1, uint8_t ledOn = 0);
  size_t write(uint8_t* data, size_t len);
  bool end(bool evenIfRemaining = false);
  bool hasError() { return failed; }
  uint8_t getError() { return failed ? 1 : 0; }
  size_t total = 0, written = 0;
  bool open = false, failed = false;
};
extern UpdaterClass Update;
