#include "welcome.h"
#include "display.h"
#include "input.h"

Welcome welcome;

// Timeline (ms)
static const unsigned long LOGO_IN_END = 1300;      // PixelOS dissolves in
static const unsigned long LOGO_HOLD_END = 2200;
static const unsigned long LOGO_OUT_END = 2800;     // and dissolves out
static const unsigned long TYPE_START = 2800;       // the greeting types out
static const unsigned long TYPE_MS_PER_CHAR = 55;
static const unsigned long TOTAL_MS = 7000;
static const unsigned long SKIP_AFTER_MS = 400;     // ignore the button that was used to skip the splash

static const char* const LINE1 = "Welcome to";
static const char* const LINE2 = "your new";
static const char* const LINE3 = "PixelBit!";

void Welcome::begin() {
  startedAt = millis();
}

bool Welcome::update() {
  unsigned long t = millis() - startedAt;
  if (t >= TOTAL_MS) return true;
  if (t >= SKIP_AFTER_MS &&
      (buttons.upPressed || buttons.downPressed || buttons.leftPressed || buttons.rightPressed)) return true;
  return false;
}

// Keeps only a fraction (0..256) of the lit pixels, chosen by a fixed noise
// pattern, so the picture appears or fades out as a dissolve
static void dissolve(int keep) {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      if (!display.getPixel(x, y)) continue;
      uint32_t h = (uint32_t)(x + 1) * 2654435761u ^ (uint32_t)(y + 1) * 40503u;
      if (((h >> 11) & 255) >= keep) display.drawPixel(x, y, SSD1306_BLACK);
    }
  }
}

// A little star that twinkles: nothing, dot, small plus, big plus
static void sparkle(int x, int y, int phase) {
  switch (phase % 6) {
    case 1: case 5: display.drawPixel(x, y, SSD1306_WHITE); break;
    case 2: case 4:
      display.drawFastHLine(x - 1, y, 3, SSD1306_WHITE);
      display.drawFastVLine(x, y - 1, 3, SSD1306_WHITE);
      break;
    case 3:
      display.drawFastHLine(x - 2, y, 5, SSD1306_WHITE);
      display.drawFastVLine(x, y - 2, 5, SSD1306_WHITE);
      break;
    default: break;
  }
}

// Prints the first `count` characters of a line, centred as if it were complete
// so the text does not shift while it types
static void typed(const char* text, int y, int size, int count) {
  int full = strlen(text);
  if (count <= 0) return;
  int shown = count < full ? count : full;
  int x = (SCREEN_WIDTH - (full * 6 * size - size)) / 2;
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  char buf[16];
  strncpy(buf, text, shown);
  buf[shown] = 0;
  display.print(buf);
}

void Welcome::draw() {
  unsigned long t = millis() - startedAt;
  clearDisplay();

  if (t < LOGO_OUT_END) {
    // The OS logo, as on the normal splash screen
    drawCenteredText(OS_NAME, 14, 3);
    drawCenteredText(OS_VERSION, 44, 1);
    if (t < LOGO_IN_END) dissolve((int)(t * 256 / LOGO_IN_END));
    else if (t >= LOGO_HOLD_END) dissolve((int)(256 - (t - LOGO_HOLD_END) * 256 / (LOGO_OUT_END - LOGO_HOLD_END)));
    return;
  }

  // The greeting
  unsigned long since = t - TYPE_START;
  int chars = (int)(since / TYPE_MS_PER_CHAR);
  int n1 = strlen(LINE1), n2 = strlen(LINE2), n3 = strlen(LINE3);
  typed(LINE1, 6, 1, chars);
  typed(LINE2, 18, 1, chars - n1);
  typed(LINE3, 30, 2, chars - n1 - n2);

  bool done = chars >= n1 + n2 + n3;
  unsigned long doneAt = (unsigned long)(n1 + n2 + n3) * TYPE_MS_PER_CHAR;

  // A blinking block cursor while it types
  if (!done && (millis() / 200) % 2) {
    int line = chars < n1 ? 0 : (chars < n1 + n2 ? 1 : 2);
    int size = line == 2 ? 2 : 1;
    int shown = line == 0 ? chars : (line == 1 ? chars - n1 : chars - n1 - n2);
    const char* text = line == 0 ? LINE1 : (line == 1 ? LINE2 : LINE3);
    int x = (SCREEN_WIDTH - (strlen(text) * 6 * size - size)) / 2 + shown * 6 * size;
    int y = line == 0 ? 6 : (line == 1 ? 18 : 30);
    display.fillRect(x, y, 5 * size, 7 * size, SSD1306_WHITE);
  }

  if (done) {
    unsigned long after = since - doneAt;

    // An underline sweeps under "PixelBit!"
    int sweep = (int)min(after * 108UL / 500UL, 108UL);
    display.drawFastHLine((SCREEN_WIDTH - 108) / 2, 48, sweep, SSD1306_WHITE);

    // Sparkles around the name
    static const int8_t SPARKS[6][2] = {{14, 12}, {112, 10}, {10, 36}, {118, 38}, {64, 2}, {100, 24}};
    for (int i = 0; i < 6; i++) sparkle(SPARKS[i][0], SPARKS[i][1], (int)(millis() / 130 + i * 2));

    // And the OS gets a mention
    if (after > 700) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      char line[32];
      snprintf(line, sizeof(line), "powered by %s", OS_NAME);   // 18 characters: fits the width
      drawCenteredText(line, 55, 1);
    }
  }
}
