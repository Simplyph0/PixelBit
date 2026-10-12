#include "fx.h"
#include "display.h"

struct Pop {
  int16_t x, y;
  uint8_t life;   // frames left, 0 = free
  char text[7];
};

static const uint8_t POPS = 4;
static const uint8_t POP_FRAMES = 14;
static Pop pops[POPS];

void fxPopText(int x, int y, const char* text) {
  // Reuse a free slot, or the one closest to expiring
  int slot = 0;
  for (int i = 0; i < POPS; i++) {
    if (pops[i].life == 0) { slot = i; break; }
    if (pops[i].life < pops[slot].life) slot = i;
  }
  pops[slot].x = x;
  pops[slot].y = y;
  pops[slot].life = POP_FRAMES;
  strncpy(pops[slot].text, text, sizeof(pops[slot].text) - 1);
  pops[slot].text[sizeof(pops[slot].text) - 1] = 0;
}

void fxPop(int x, int y, int value) {
  char text[8];
  snprintf(text, sizeof(text), "+%d", value);
  fxPopText(x, y, text);
}

void fxReset() {
  for (int i = 0; i < POPS; i++) pops[i].life = 0;
}

void fxDraw() {
  display.setTextSize(1);
  for (int i = 0; i < POPS; i++) {
    Pop& p = pops[i];
    if (p.life == 0) continue;

    int w = strlen(p.text) * 6;
    int x = constrain((int)p.x - w / 2, 0, SCREEN_WIDTH - w);
    int y = constrain((int)p.y - (POP_FRAMES - p.life) / 2, 0, SCREEN_HEIGHT - 8);
    display.fillRect(x - 1, y - 1, w + 1, 9, SSD1306_BLACK);   // keeps the text readable over the game
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(x, y);
    display.print(p.text);
    p.life--;
  }
}

void fxShutter() {
  for (int step = 1; step <= 8; step++) {
    int h = step * 4;
    display.fillRect(0, 0, SCREEN_WIDTH, h, SSD1306_BLACK);
    display.fillRect(0, SCREEN_HEIGHT - h, SCREEN_WIDTH, h, SSD1306_BLACK);
    updateDisplay();
    delay(30);
  }
  fxReset();
}
