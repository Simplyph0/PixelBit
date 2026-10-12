// Minimal Arduino environment for logic tests (no screen, controllable clock)
#include "Arduino.h"
#include "Wire.h"
#include "EEPROM.h"
#include "ESP8266WiFi.h"
#include "espnow.h"
#include "Adafruit_SSD1306.h"
#include <stdlib.h>
SerialT Serial; TwoWire Wire; EEPROMClass EEPROM; WiFiClass WiFi; EspClass ESP;
void emulatorEepromLoad(uint8_t*, size_t) {}
void emulatorEepromSave(const uint8_t*, size_t) {}
void emulatorPresent(const uint8_t[64][128], bool) {}
#ifndef USE_AIR
unsigned long g_ms = 0;
unsigned long millis() { return g_ms; }
void delay(unsigned long ms) { g_ms += ms; }
#endif
long random(long m) { return m > 0 ? rand() % m : 0; }
long random(long a, long b) { return b > a ? a + rand() % (b - a) : a; }
void pinMode(int, int) {} int digitalRead(int) { return 1; }
#ifndef USE_AIR
void WiFiClass::macAddress(uint8_t* m) { memset(m, 2, 6); }
uint32_t EspClass::random() { return rand(); } uint32_t EspClass::getChipId() { return 0x123456; }
int esp_now_init() { return 0; } int esp_now_deinit() { return 0; } int esp_now_set_self_role(u8) { return 0; }
int esp_now_register_recv_cb(esp_now_recv_cb_t) { return 0; } int esp_now_unregister_recv_cb() { return 0; } int esp_now_register_send_cb(esp_now_send_cb_t) { return 0; }
int esp_now_add_peer(u8*, u8, u8, u8*, u8) { return 0; } int esp_now_send(u8*, u8*, int) { return 0; }
#endif
