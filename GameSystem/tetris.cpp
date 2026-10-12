#include "tetris.h"
#include "display.h"
#include "input.h"
#include "fx.h"

TetrisGame tetrisGame;

// Tetris piece shapes: one 16-bit mask per rotation, a 4x4 grid read row by row
// with the top-left cell in the highest bit (bit 15 - row * 4 - column).
static const uint16_t tetrisPieces[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},   // I
  {0x0660, 0x0660, 0x0660, 0x0660},   // O
  {0x04E0, 0x0464, 0x00E4, 0x04C4},   // T
  {0x06C0, 0x0462, 0x006C, 0x08C4},   // S
  {0x0C60, 0x0264, 0x00C6, 0x04C8},   // Z
  {0x08E0, 0x0644, 0x00E2, 0x044C},   // J
  {0x02E0, 0x0446, 0x00E8, 0x0C44},   // L
};

static inline bool pieceCell(int type, int rotation, int row, int col) {
  return tetrisPieces[type][rotation] & (0x8000 >> (row * 4 + col));
}

static const char* const LEVEL_VALUES[] = {"1", "3", "5", "7", "9"};
static const GameOption OPT_START_LEVEL = {"START LEVEL", 5, LEVEL_VALUES, 0, true};

const GameOption* TetrisGame::option(int) { return &OPT_START_LEVEL; }

static const unsigned long DAS_DELAY_MS = 200;    // hold this long before a direction repeats
static const unsigned long DAS_REPEAT_MS = 70;
static const unsigned long HARD_DROP_TAP_MS = 280;

void TetrisGame::init() {
  memset(board, 0, sizeof(board));

  score = 0;
  level = 1 + startLevel * 2;
  linesCleared = 0;
  dropSpeed = max(50, 500 - 50 * (level - 1));
  lastDropTime = 0;
  dasAt = 0;
  lastDownTap = 0;
  chordLatched = false;
  holdType = -1;
  canHold = true;
  gameOver = false;
  bagPos = 7;   // empty: the first draw refills it

  nextType = takeFromBag();
  spawnNewPiece(takeFromBag());
}

// 7-bag: every piece shows up once per seven, in random order
uint8_t TetrisGame::takeFromBag() {
  if (bagPos >= 7) {
    for (int i = 0; i < 7; i++) bag[i] = i;
    for (int i = 6; i > 0; i--) {
      int j = random(0, i + 1);
      uint8_t t = bag[i]; bag[i] = bag[j]; bag[j] = t;
    }
    bagPos = 0;
  }
  return bag[bagPos++];
}

void TetrisGame::spawnNewPiece(int type) {
  currentPiece.x = TETRIS_WIDTH / 2 - 2;
  currentPiece.y = 0;
  currentPiece.type = type;
  currentPiece.rotation = 0;
  canHold = true;
  if (!isValidPosition(currentPiece.x, currentPiece.y, type, 0)) gameOver = true;
}

void TetrisGame::update() {
  handleInput();
  if (gameOver) return;

  // Holding DOWN speeds gravity up (and scores a point per row)
  bool soft = buttons.down && !buttons.up;
  unsigned long interval = soft ? min((unsigned long)dropSpeed, 55UL) : (unsigned long)dropSpeed;

  if (millis() - lastDropTime > interval) {
    if (movePiece(0, 1, 0)) {
      if (soft) score += 1;
    } else {
      lockPiece();
    }
    lastDropTime = millis();
  }
}

void TetrisGame::lockPiece() {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (pieceCell(currentPiece.type, currentPiece.rotation, py, px)) {
        int boardX = currentPiece.x + px;
        int boardY = currentPiece.y + py;
        if (boardY >= 0) board[boardY][boardX] = 1;
      }
    }
  }
  clearLines();
  spawnNewPiece(nextType);
  nextType = takeFromBag();
}

void TetrisGame::handleInput() {
  unsigned long now = millis();

  // LEFT + RIGHT together: hold / swap
  if (buttons.left && buttons.right) {
    if (!chordLatched) holdPiece();
    chordLatched = true;
    return;
  }
  if (!buttons.left && !buttons.right) chordLatched = false;
  if (chordLatched) return;   // wait until both are released

  // Move on the press, then repeat while held
  if (buttons.leftPressed || buttons.rightPressed) {
    movePiece(buttons.rightPressed ? 1 : -1, 0, 0);
    dasAt = now + DAS_DELAY_MS;
  } else if ((buttons.left || buttons.right) && now >= dasAt) {
    movePiece(buttons.right ? 1 : -1, 0, 0);
    dasAt = now + DAS_REPEAT_MS;
  }

  if (buttons.upPressed) rotatePiece();

  // Double-tap DOWN: hard drop
  if (buttons.downPressed) {
    if (now - lastDownTap < HARD_DROP_TAP_MS) {
      hardDrop();
      lastDownTap = 0;
    } else {
      lastDownTap = now;
    }
  }
}

void TetrisGame::hardDrop() {
  int rows = 0;
  while (movePiece(0, 1, 0)) rows++;
  score += rows * 2;
  lockPiece();
  lastDropTime = millis();
}

void TetrisGame::holdPiece() {
  if (!canHold || gameOver) return;
  int current = currentPiece.type;
  if (holdType < 0) {
    spawnNewPiece(nextType);
    nextType = takeFromBag();
  } else {
    spawnNewPiece(holdType);
  }
  holdType = current;
  canHold = false;
  lastDropTime = millis();
}

bool TetrisGame::movePiece(int dx, int dy, int dr) {
  int newX = currentPiece.x + dx;
  int newY = currentPiece.y + dy;
  int newRotation = (currentPiece.rotation + dr) % 4;

  if (isValidPosition(newX, newY, currentPiece.type, newRotation)) {
    currentPiece.x = newX;
    currentPiece.y = newY;
    currentPiece.rotation = newRotation;
    return true;
  }
  return false;
}

// Rotates clockwise; if that does not fit, nudges the piece sideways (a simple
// wall kick) so rotating next to a wall or the stack still works
bool TetrisGame::rotatePiece() {
  static const int8_t KICKS[] = {0, -1, 1, -2, 2};
  int newRotation = (currentPiece.rotation + 1) % 4;
  for (int8_t k : KICKS) {
    if (isValidPosition(currentPiece.x + k, currentPiece.y, currentPiece.type, newRotation)) {
      currentPiece.x += k;
      currentPiece.rotation = newRotation;
      return true;
    }
  }
  return false;
}

bool TetrisGame::isValidPosition(int x, int y, int type, int rotation) {
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (pieceCell(type, rotation, py, px)) {
        int boardX = x + px;
        int boardY = y + py;
        
        if (boardX < 0 || boardX >= TETRIS_WIDTH || 
            boardY >= TETRIS_HEIGHT || 
            (boardY >= 0 && board[boardY][boardX])) {
          return false;
        }
      }
    }
  }
  return true;
}

int TetrisGame::landingY() {
  int y = currentPiece.y;
  while (isValidPosition(currentPiece.x, y + 1, currentPiece.type, currentPiece.rotation)) y++;
  return y;
}

void TetrisGame::clearLines() {
  int linesClearedNow = 0;
  
  for (int y = TETRIS_HEIGHT - 1; y >= 0; y--) {
    bool fullLine = true;
    for (int x = 0; x < TETRIS_WIDTH; x++) {
      if (!board[y][x]) {
        fullLine = false;
        break;
      }
    }
    
    if (fullLine) {
      // Move all lines above down
      for (int moveY = y; moveY > 0; moveY--) {
        for (int x = 0; x < TETRIS_WIDTH; x++) {
          board[moveY][x] = board[moveY - 1][x];
        }
      }
      
      // Clear top line
      for (int x = 0; x < TETRIS_WIDTH; x++) {
        board[0][x] = 0;
      }
      
      linesClearedNow++;
      y++; // Check same line again
    }
  }
  
  if (linesClearedNow > 0) {
    static const int POINTS[5] = {0, 100, 300, 500, 800};
    int gained = POINTS[linesClearedNow] * level;
    score += gained;
    linesCleared += linesClearedNow;
    fxPop(TETRIS_OFFSET_X + TETRIS_FIELD_WIDTH / 2, TETRIS_OFFSET_Y + TETRIS_FIELD_HEIGHT / 2, gained);

    // Increase level every 10 lines
    if (linesCleared >= (level - startLevel * 2) * 10) {
      level++;
      dropSpeed = max(50, dropSpeed - 50);
    }
  }
}

// A 4x4 piece in a 12x12 box
void TetrisGame::drawPreview(int type, int x, int y) {
  if (type < 0) return;
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (pieceCell(type, 0, py, px)) {
        display.fillRect(x + px * BLOCK_SIZE, y + py * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, SSD1306_WHITE);
      }
    }
  }
}

void TetrisGame::draw() {
  clearDisplay();
  
  // Border
  display.drawRect(TETRIS_OFFSET_X - 1, TETRIS_OFFSET_Y - 1, 
                   TETRIS_FIELD_WIDTH + 2, TETRIS_FIELD_HEIGHT + 2, 
                   SSD1306_WHITE);
  
  // Board
  for (int y = 0; y < TETRIS_HEIGHT; y++) {
    for (int x = 0; x < TETRIS_WIDTH; x++) {
      if (board[y][x]) {
        display.fillRect(TETRIS_OFFSET_X + x * BLOCK_SIZE, TETRIS_OFFSET_Y + y * BLOCK_SIZE,
                         BLOCK_SIZE, BLOCK_SIZE, SSD1306_WHITE);
      }
    }
  }

  // Ghost: where the piece would land, drawn hollow
  if (!gameOver) {
    int gy = landingY();
    if (gy != currentPiece.y) {
      for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
          if (pieceCell(currentPiece.type, currentPiece.rotation, py, px) && gy + py >= 0) {
            display.drawRect(TETRIS_OFFSET_X + (currentPiece.x + px) * BLOCK_SIZE,
                             TETRIS_OFFSET_Y + (gy + py) * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, SSD1306_WHITE);
          }
        }
      }
    }
  }
  
  // Current piece
  for (int py = 0; py < 4; py++) {
    for (int px = 0; px < 4; px++) {
      if (pieceCell(currentPiece.type, currentPiece.rotation, py, px)) {
        int screenY = TETRIS_OFFSET_Y + (currentPiece.y + py) * BLOCK_SIZE;
        if (screenY >= TETRIS_OFFSET_Y) {
          display.fillRect(TETRIS_OFFSET_X + (currentPiece.x + px) * BLOCK_SIZE, screenY,
                           BLOCK_SIZE, BLOCK_SIZE, SSD1306_WHITE);
        }
      }
    }
  }
  
  // Side panel: next and hold, then the numbers
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  const int px0 = 60;
  display.setCursor(px0, 0);
  display.print(F("NEXT"));
  display.drawRect(px0, 9, 16, 16, SSD1306_WHITE);
  drawPreview(nextType, px0 + 2, 11);

  display.setCursor(px0 + 36, 0);
  display.print(F("HOLD"));
  display.drawRect(px0 + 36, 9, 16, 16, SSD1306_WHITE);
  if (!canHold) display.drawRect(px0 + 34, 7, 20, 20, SSD1306_WHITE);   // used this piece
  drawPreview(holdType, px0 + 38, 11);

  display.setCursor(px0, 30);
  display.print(F("SCORE"));
  display.setCursor(px0, 39);
  display.print(score);
  display.setCursor(px0, 50);
  display.print(F("LV "));
  display.print(level);
  display.print(F(" L "));
  display.print(linesCleared);
}
