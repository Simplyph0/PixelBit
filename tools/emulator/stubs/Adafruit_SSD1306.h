#pragma once
// Stand-in for the SSD1306 driver: the real Adafruit_GFX draws into a 128x64
// buffer, and display() hands it to the emulator's terminal.
#include "Arduino.h"
#include "Wire.h"
#include <Adafruit_GFX.h>
#define SSD1306_WHITE 1
#define SSD1306_BLACK 0
#define SSD1306_INVERSE 2
#define SSD1306_SWITCHCAPVCC 2

void emulatorPresent(const uint8_t fb[64][128], bool inverted);

class Adafruit_SSD1306 : public Adafruit_GFX {
public:
  uint8_t fb[64][128];
  Adafruit_SSD1306(int16_t w, int16_t h, TwoWire*, int8_t) : Adafruit_GFX(w, h) { memset(fb, 0, sizeof fb); }
  bool begin(uint8_t = 0, uint8_t = 0, bool = true, bool = true) { return true; }
  void clearDisplay() { memset(fb, 0, sizeof fb); }
  void display() { emulatorPresent(fb, inverted); }
  void invertDisplay(bool i) { inverted = i; }
  bool getPixel(int16_t x, int16_t y) { return x >= 0 && y >= 0 && x < 128 && y < 64 && fb[y][x]; }
  bool inverted = false;
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
    if (c == SSD1306_INVERSE) fb[y][x] ^= 1; else fb[y][x] = c ? 1 : 0;
  }
};
