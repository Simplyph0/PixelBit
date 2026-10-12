#include "ui.h"
#include "display.h"

const char* const DIFFICULTY_NAMES[DIFFICULTY_LEVELS] = {"EASY", "NORMAL", "HARD", "EXPERT", "INSANE"};

static void drawArrows(bool leftArrow, bool rightArrow, int y) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  if (leftArrow) {
    display.setCursor(2, y);
    display.print('<');
  }
  if (rightArrow) {
    display.setCursor(SCREEN_WIDTH - 8, y);
    display.print('>');
  }
}

static const char* const MODE_AI_VALUES[] = {"VS AI", "2 PLAYER"};
static const char* const MODE_PRACTICE_VALUES[] = {"VS AI", "2 PLAYER", "PRACTICE"};
const GameOption OPT_DIFFICULTY = {"DIFFICULTY", DIFFICULTY_LEVELS, DIFFICULTY_NAMES, DIFFICULTY_DEFAULT, true};
const GameOption OPT_MODE_AI = {"MODE", 2, MODE_AI_VALUES, 0, false};
const GameOption OPT_MODE_PRACTICE = {"MODE", 3, MODE_PRACTICE_VALUES, 0, false};

void drawOptionScreen(const char* title, const GameOption& option, int value, const char* hint) {
  clearDisplay();
  drawCenteredText(title, 0, 1);
  drawCenteredText(option.name, 10, 1);
  bool big = strlen(option.values[value]) * 12 <= SCREEN_WIDTH - 24;
  drawCenteredText(option.values[value], big ? 24 : 28, big ? 2 : 1);
  drawArrows(value > 0, value < option.count - 1, 28);

  if (option.strength) {
    // Bar: filled up to the chosen level
    const int gap = 4;
    int segW = (SCREEN_WIDTH - 32 - (option.count - 1) * gap) / option.count;
    int x0 = (SCREEN_WIDTH - (option.count * segW + (option.count - 1) * gap)) / 2;
    for (int i = 0; i < option.count; i++) {
      int x = x0 + i * (segW + gap);
      if (i <= value) display.fillRect(x, 44, segW, 6, SSD1306_WHITE);
      else            display.drawRect(x, 44, segW, 6, SSD1306_WHITE);
    }
  } else {
    // Dots: one per value
    int x0 = (SCREEN_WIDTH - option.count * 8) / 2;
    for (int i = 0; i < option.count; i++) {
      if (i == value) display.fillRect(x0 + i * 8 + 1, 46, 5, 5, SSD1306_WHITE);
      else            display.drawRect(x0 + i * 8 + 1, 46, 5, 5, SSD1306_WHITE);
    }
  }

  drawCenteredText(hint, 56, 1);
}

void drawChoiceScreen(const char* title, const char* value, const char* line,
                      const char* hint, bool leftArrow, bool rightArrow) {
  clearDisplay();
  drawCenteredText(title, 0, 1);
  bool big = strlen(value) * 12 <= SCREEN_WIDTH - 36;
  drawCenteredText(value, big ? 20 : 24, big ? 2 : 1);
  drawArrows(leftArrow, rightArrow, 24);
  if (line && line[0]) drawCenteredText(line, 42, 1);
  drawCenteredText(hint, 56, 1);
}

void drawMessageScreen(const char* title, const char* big, const char* line1,
                       const char* line2, const char* hint) {
  clearDisplay();
  drawCenteredText(title, 0, 1);
  if (big && big[0]) drawCenteredText(big, 15, 2);
  if (line1 && line1[0]) drawCenteredText(line1, 38, 1);
  if (line2 && line2[0]) drawCenteredText(line2, 47, 1);
  drawCenteredText(hint, 56, 1);
}
