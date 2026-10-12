#include "game2048.h"
#include "display.h"
#include "input.h"
#include "fx.h"

Game2048 game2048;

static const char* const BOARD_VALUES[] = {"4 X 4", "5 X 5"};
static const GameOption OPT_BOARD = {"BOARD", 2, BOARD_VALUES, 0, false};

const GameOption* Game2048::option(int) { return &OPT_BOARD; }

// Tile geometry: 4x4 is 14 px tiles with 2 px gaps; 5x5 is 11 px tiles with 1 px gaps
static int tileSize(int n)   { return n == 4 ? 14 : 11; }
static int tileMargin(int n) { return n == 4 ? 2 : 1; }
static int boardX(int n)     { return n == 4 ? 32 : 34; }

void Game2048::init() {
  memset(grid, 0, sizeof(grid));

  score = 0;
  gameOver = false;
  hasWon = false;
  moved = false;
  historyCount = 0;
  undosLeft = 3;
  lastMoveAt = 0;
  chordLatched = false;
  
  // Add two initial tiles
  addRandomTile();
  addRandomTile();
}

void Game2048::update() {
  handleInput();
  
  if (moved) {
    addRandomTile();
    moved = false;
    
    if (!canMove()) {
      gameOver = true;
    }
  }
}

void Game2048::draw() {
  clearDisplay();
  
  // Score and undo (left of the board)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("Score:"));
  display.setCursor(0, 10);
  display.print(score);
  display.setCursor(0, 34);
  display.print(F("UNDO"));
  display.setCursor(0, 43);
  display.print(F("x"));
  display.print(undosLeft);
  if (size == 5) {
    display.setCursor(0, 56);
    display.print(F("2^N"));
  }
  
  // Grid
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      drawTile(x, y, grid[y][x]);
    }
  }
  
  if (hasWon) {
    display.setCursor(0, 22);
    display.print(F("WIN!"));
  }
}

// ---------------------------------------------------------------------------
// Input: slide with the buttons; LEFT + RIGHT together undo. The first of the
// two buttons slides as usual before the second arrives, so a chord that comes
// within a moment of a slide takes that slide back as well.
// ---------------------------------------------------------------------------
void Game2048::handleInput() {
  unsigned long now = millis();

  if (buttons.left && buttons.right) {
    if (!chordLatched) {
      chordLatched = true;
      bool accidental = now - lastMoveAt < 250 && historyCount > 0;
      if (accidental) undo();                    // the slide the chord's first button caused
      if (undosLeft > 0 && historyCount > 0) {
        undosLeft--;
        undo();
        fxPopText(SCREEN_WIDTH - 24, 28, "UNDO");
      }
    }
    return;
  }
  if (!buttons.left && !buttons.right) chordLatched = false;
  if (chordLatched) return;

  int dir = -1;
  if (buttons.leftPressed) dir = 0;
  else if (buttons.rightPressed) dir = 1;
  else if (buttons.upPressed) dir = 2;
  else if (buttons.downPressed) dir = 3;
  if (dir < 0) return;

  remember();
  slide(dir);
  if (moved) {
    lastMoveAt = now;
  } else if (historyCount > 0) {
    historyCount--;   // nothing moved: forget the snapshot
  }
}

void Game2048::remember() {
  if (historyCount == 4) {
    memmove(&history[0], &history[1], 3 * sizeof(Snapshot));
    historyCount = 3;
  }
  memcpy(history[historyCount].grid, grid, sizeof(grid));
  history[historyCount].score = score;
  historyCount++;
}

void Game2048::undo() {
  if (historyCount == 0) return;
  historyCount--;
  memcpy(grid, history[historyCount].grid, sizeof(grid));
  score = history[historyCount].score;
  moved = false;   // the tile that would have been added is gone too
  gameOver = false;
}

void Game2048::addRandomTile() {
  // Find empty cells
  uint8_t emptyCells[MAX_GRID_2048 * MAX_GRID_2048][2];
  int count = 0;
  
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (grid[y][x] == 0) {
        emptyCells[count][0] = x;
        emptyCells[count][1] = y;
        count++;
      }
    }
  }
  
  if (count > 0) {
    int index = random(0, count);
    int x = emptyCells[index][0];
    int y = emptyCells[index][1];
    
    // 90% chance for 2, 10% chance for 4
    grid[y][x] = (random(0, 10) < 9) ? 2 : 4;
  }
}

bool Game2048::canMove() {
  // Check for empty cells
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (grid[y][x] == 0) return true;
    }
  }
  
  // Check for possible merges
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      int value = grid[y][x];
      if ((x < size - 1 && grid[y][x + 1] == value) ||
          (y < size - 1 && grid[y + 1][x] == value)) {
        return true;
      }
    }
  }
  
  return false;
}

// Slides every row (dir 0 = left, 1 = right) or column (dir 2 = up, 3 = down)
// towards that side. Each line is read starting at the wall the tiles move to,
// so one merge routine serves all four directions.
void Game2048::slide(int dir) {
  bool hasMoved = false;

  for (int i = 0; i < size; i++) {
    int line[MAX_GRID_2048];
    for (int j = 0; j < size; j++) line[j] = *cell(dir, i, j);

    if (mergeLine(line, size)) {
      hasMoved = true;
      for (int j = 0; j < size; j++) *cell(dir, i, j) = line[j];
    }
  }

  moved = hasMoved;
}

// The j-th cell of line i, counted from the wall that direction dir slides to.
int* Game2048::cell(int dir, int i, int j) {
  const int last = size - 1;
  switch (dir) {
    case 0:  return &grid[i][j];
    case 1:  return &grid[i][last - j];
    case 2:  return &grid[j][i];
    default: return &grid[last - j][i];
  }
}

// Compresses the line towards index 0 and merges equal neighbours once each.
// Returns true if anything changed.
bool Game2048::mergeLine(int line[], int n) {
  int out[MAX_GRID_2048] = {0};
  int writePos = 0;
  bool canMerge = false;   // out[writePos - 1] has not been merged yet

  for (int i = 0; i < n; i++) {
    int v = line[i];
    if (v == 0) continue;

    if (canMerge && out[writePos - 1] == v) {
      out[writePos - 1] = v * 2;
      score += v * 2;
      if (v * 2 == 2048) hasWon = true;
      canMerge = false;
    } else {
      out[writePos++] = v;
      canMerge = true;
    }
  }

  bool changed = false;
  for (int i = 0; i < n; i++) {
    if (line[i] != out[i]) changed = true;
    line[i] = out[i];
  }
  return changed;
}

void Game2048::drawTile(int x, int y, int value) {
  int t = tileSize(size), m = tileMargin(size);
  int screenX = boardX(size) + x * (t + m);
  int screenY = y * (t + m);
  
  // Draw tile background
  if (value == 0) {
    display.drawRect(screenX, screenY, t, t, SSD1306_WHITE);
    return;
  }

  display.fillRect(screenX, screenY, t, t, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);

  // 4x4 shows the value ("1k" from 1000); 5x5 shows the power of two so it fits
  char text[6];
  if (size == 5) {
    int power = 0;
    for (int v = value; v > 1; v >>= 1) power++;
    snprintf(text, sizeof(text), "%d", power);
  } else if (value >= 1000) {
    snprintf(text, sizeof(text), "%dk", value / 1000);
  } else {
    snprintf(text, sizeof(text), "%d", value);
  }

  int textWidth = strlen(text) * 6 - 1;
  display.setCursor(screenX + (t - textWidth) / 2, screenY + (t - 7) / 2);
  display.print(text);
}
