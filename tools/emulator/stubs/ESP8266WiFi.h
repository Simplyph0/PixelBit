#pragma once
#include "Arduino.h"
#define WIFI_AP 2
#define WIFI_STA 1
#define WIFI_OFF 0
struct WiFiClass {
  void mode(int) {} void persistent(bool) {} void disconnect() {}
  void softAP(const char*, const char*, int, int, int) {} void softAPdisconnect(bool) {} int softAPgetStationNum() { return 0; }
  void macAddress(uint8_t* m);
};
extern WiFiClass WiFi;
struct EspClass {
  uint32_t random(); uint32_t getChipId();
  uint32_t getFreeHeap() { return 41000; }
  uint32_t getFlashChipRealSize() { return 4 * 1024 * 1024; }
  uint32_t getFlashChipSize() { return 4 * 1024 * 1024; }
  String getResetReason() { return "Emulator"; }
  void restart();
};
extern EspClass ESP;
