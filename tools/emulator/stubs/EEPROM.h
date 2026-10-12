#pragma once
#include "Arduino.h"
// Behaves like the ESP8266's EEPROM emulation: reads give 0 and writes are
// ignored until begin() has been called (a firmware that forgets begin() saves
// nothing, as on the real board). With --eeprom file the contents are loaded at
// begin() and written back by commit(); otherwise they live in memory only.
void emulatorEepromLoad(uint8_t* mem, size_t size);
void emulatorEepromSave(const uint8_t* mem, size_t size);
struct EEPROMClass {
  uint8_t mem[1024];
  size_t size = 0;
  void begin(size_t n) { size = n > sizeof(mem) ? sizeof(mem) : n; memset(mem, 0, sizeof(mem)); emulatorEepromLoad(mem, size); }
  uint8_t read(int a) { return (a >= 0 && (size_t)a < size) ? mem[a] : 0; }
  void write(int a, uint8_t v) { if (a >= 0 && (size_t)a < size) mem[a] = v; }
  bool commit() { if (!size) return false; emulatorEepromSave(mem, size); return true; }
};
extern EEPROMClass EEPROM;
