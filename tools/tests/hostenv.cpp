// Minimal Arduino environment for logic tests (no screen, controllable clock)
#include "Arduino.h"
#include "Wire.h"
#include "EEPROM.h"
#include "ESP8266WiFi.h"
#include "espnow.h"
#include "Adafruit_SSD1306.h"
#include "Updater.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
void (*g_yieldHook)() = nullptr;
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

// ---- pretend flash image and updater, for the firmware update tests ----
// FW_IMAGE: file with the firmware this console "has"; FW_OUT: where an installed
// image is written; FREE_SPACE: bytes available for an update.
static std::vector<uint8_t> g_flash;
static std::string g_outPath = "received-firmware.bin";
static size_t g_free = 1019 * 1024;
bool g_restarted = false;
static void ensureFlash() {
  static bool loaded = false;
  if (loaded) return;
  loaded = true;
  if (const char* p = getenv("FW_OUT")) g_outPath = p;
  if (const char* p = getenv("FREE_SPACE")) g_free = atol(p);
  if (const char* p = getenv("FW_IMAGE")) if (FILE* f = fopen(p, "rb")) { uint8_t b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) g_flash.insert(g_flash.end(), b, b + n); fclose(f); }
  if (g_flash.empty()) { g_flash.assign(300 * 1024, 0x5A); g_flash[0] = 0xE9; g_flash[1] = 4; g_flash[2] = 2; g_flash[3] = 0x40; }
}
uint32_t EspClass::getSketchSize() { ensureFlash(); return g_flash.size(); }
uint32_t EspClass::getFreeSketchSpace() { ensureFlash(); return g_free; }
bool EspClass::flashRead(uint32_t a, uint8_t* d, size_t n) { ensureFlash(); if (a + n > g_flash.size()) return false; memcpy(d, g_flash.data() + a, n); return true; }
void EspClass::restart() { g_restarted = true; printf("RESTART requested\n"); fflush(stdout); }
UpdaterClass Update;
static FILE* g_uf = nullptr;
bool UpdaterClass::begin(size_t size, int, int, uint8_t) { ensureFlash(); if (open || !size || size > g_free) { failed = true; return false; } g_uf = fopen((g_outPath + ".partial").c_str(), "wb"); total = size; written = 0; open = true; failed = false; return g_uf != nullptr; }
size_t UpdaterClass::write(uint8_t* d, size_t n) { if (!open || written + n > total) { failed = true; return 0; } fwrite(d, 1, n, g_uf); written += n; return n; }
bool UpdaterClass::end(bool even) { if (!open) return false; fclose(g_uf); open = false; if (written != total) { remove((g_outPath + ".partial").c_str()); failed = true; return false; } plat::replaceFile((g_outPath + ".partial").c_str(), g_outPath.c_str()); return true; }
