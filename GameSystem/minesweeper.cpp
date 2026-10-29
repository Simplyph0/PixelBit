#include "minesweeper.h"
#include "display.h"
#include "input.h"

MinesweeperGame minesweeperGame;

static const uint8_t MINE_COUNT[DIFFICULTY_LEVELS] = {12, 16, 20, 25, 30};   // of 112 cells
static const int FIELD_Y = 8, CELL = 8;
static const unsigned long REPEAT_FIRST_MS = 350, REPEAT_MS = 70;

static uint32_t rnd(uint32_t n) { return (uint32_t)random((long)n); }

void MinesweeperGame::init() {
  minesStart(field, MINE_COUNT[level]);
  cursor = (MINES_ROWS / 2) * MINES_COLS + MINES_COLS / 2;
  over = false;
  startedAt = 0;
  repeatAt = 0;
}

int MinesweeperGame::getScore() {
  return (field.opened + (minesWon(field) ? 100 : 0)) * (level + 1);
}

void MinesweeperGame::step(int dir) {
  cursor = (cursor + MINES_CELLS + dir) % MINES_CELLS;
}

void MinesweeperGame::update() {
  if (over) return;
  unsigned long now = millis();

  if (buttons.rightPressed) { step(1); repeatAt = now + REPEAT_FIRST_MS; }
  else if (buttons.leftPressed) { step(-1); repeatAt = now + REPEAT_FIRST_MS; }
  else if ((buttons.right || buttons.left) && repeatAt && now >= repeatAt) {
    step(buttons.right ? 1 : -1);
    repeatAt = now + REPEAT_MS;
  }

  if (buttons.downPressed) minesFlag(field, cursor);
  if (buttons.upPressed) {
    bool open = field.cell[cursor] & MC_OPEN;
    if (!field.placed) startedAt = now;
    if (!(open ? minesChord(field, cursor, rnd) : minesOpen(field, cursor, rnd)) || minesWon(field)) {
      over = true;
      endedAt = now;
    }
  }
}

static void drawNumber(int x, int y, int n) {
  display.setCursor(x + 1, y);
  display.print(n);
}

void MinesweeperGame::draw() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header: mines left (flags can be wrong, so it can go below zero) and the time
  display.setCursor(0, 0);
  if (over) {
    display.print(minesWon(field) ? F("CLEARED!") : F("BOOM"));
  } else {
    display.print(F("MINES "));
    display.print((int)field.mines - (int)field.flags);
  }
  unsigned long end = over ? endedAt : millis();
  unsigned long secs = field.placed ? (end - startedAt) / 1000 : 0;
  char text[8];
  snprintf(text, sizeof(text), "%lu", secs > 999 ? 999 : secs);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(text), 0);
  display.print(text);

  bool showMines = over && !minesWon(field);
  for (int i = 0; i < MINES_CELLS; i++) {
    int x = (i % MINES_COLS) * CELL, y = FIELD_Y + (i / MINES_COLS) * CELL;
    uint8_t c = field.cell[i];
    if (c & MC_OPEN) {
      if (c & MC_MINE) {                        // the one that blew up
        display.fillRect(x, y, CELL, CELL, SSD1306_WHITE);
        display.fillCircle(x + 4, y + 4, 2, SSD1306_BLACK);
      } else if (uint8_t n = minesAround(field, i)) {
        drawNumber(x, y, n);
      }
    } else if (showMines && (c & MC_MINE) && !(c & MC_FLAG)) {
      display.fillCircle(x + 4, y + 4, 2, SSD1306_WHITE);
    } else {
      display.fillRect(x + 1, y + 1, CELL - 2, CELL - 2, SSD1306_WHITE);
      if (c & MC_FLAG) {
        display.drawFastVLine(x + 3, y + 2, 4, SSD1306_BLACK);
        display.drawFastVLine(x + 4, y + 2, 2, SSD1306_BLACK);
      }
    }
  }

  if (!over && (millis() / 250) % 2) {
    display.drawRect((cursor % MINES_COLS) * CELL, FIELD_Y + (cursor / MINES_COLS) * CELL, CELL, CELL, SSD1306_WHITE);
  }
}
