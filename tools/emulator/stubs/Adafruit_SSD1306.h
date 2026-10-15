#pragma once
// Stand-in for the SSD1306 driver: the real Adafruit_GFX draws into the buffer,
// and display() hands the picture to the emulator. The buffer has the real
// layout (pages of 8 rows, 128 bytes per page, bit 0 at the top), so code that
// writes to getBuffer() directly, like the video player, works here too.
#include "Arduino.h"
#include "Wire.h"
#include <Adafruit_GFX.h>
#define SSD1306_WHITE 1
#define SSD1306_BLACK 0
#define SSD1306_INVERSE 2
#define SSD1306_SWITCHCAPVCC 2
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF

void emulatorPresent(const uint8_t fb[64][128], bool inverted);

class Adafruit_SSD1306 : public Adafruit_GFX {
public:
  uint8_t buffer[128 * 8];
  Adafruit_SSD1306(int16_t w, int16_t h, TwoWire*, int8_t) : Adafruit_GFX(w, h) { memset(buffer, 0, sizeof buffer); }
  bool begin(uint8_t = 0, uint8_t = 0, bool = true, bool = true) { return true; }
  uint8_t* getBuffer() { return buffer; }
  void clearDisplay() { memset(buffer, 0, sizeof buffer); }
  void display() {
    uint8_t fb[64][128];
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 128; x++) fb[y][x] = (buffer[(y >> 3) * 128 + x] >> (y & 7)) & 1;
    emulatorPresent(fb, inverted);
  }
  void invertDisplay(bool i) { inverted = i; }
  void ssd1306_command(uint8_t) {}   // contrast and power are not emulated
  bool getPixel(int16_t x, int16_t y) { return x >= 0 && y >= 0 && x < 128 && y < 64 && (buffer[(y >> 3) * 128 + x] >> (y & 7) & 1); }
  bool inverted = false;
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
    uint8_t& b = buffer[(y >> 3) * 128 + x];
    uint8_t bit = 1 << (y & 7);
    if (c == SSD1306_INVERSE) b ^= bit; else if (c) b |= bit; else b &= ~bit;
  }
};
