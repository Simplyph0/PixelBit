#include "breakout.h"
#include "display.h"
#include "input.h"
#include "fx.h"

BreakoutGame breakoutGame;

// Brick layouts, top row first ('#' = brick)
static const char* const LAYOUTS[BREAKOUT_LEVELS][BRICK_ROWS] = {
  {"##########", "##########", "##########", "..........", ".........."},
  {"#.#.#.#.#.", ".#.#.#.#.#", "#.#.#.#.#.", ".#.#.#.#.#", ".........."},
  {"....##....", "...####...", "..######..", ".########.", "##########"},
  {"##.##.##.#", "##.##.##.#", "##.##.##.#", "##.##.##.#", ".........."},
  {"##########", "##########", "##.####.##", "##########", "##########"}
};

// By option level: paddle width, lives, ball speed
static const int PADDLE_WIDTHS[DIFFICULTY_LEVELS] = {24, 20, 16, 14, 12};
static const int START_LIVES[DIFFICULTY_LEVELS]    = {5, 4, 3, 3, 2};
static const float BALL_SPEEDS[DIFFICULTY_LEVELS]  = {1.6, 1.8, 2.0, 2.2, 2.5};

static const unsigned long POWER_MS = 12000;

void BreakoutGame::init() {
  score = 0;
  lives = START_LIVES[difficulty];
  level = 1;
  gameOver = false;
  gameWon = false;
  wideUntil = slowUntil = 0;
  baseSpeed = BALL_SPEEDS[difficulty];
  paddleW = PADDLE_WIDTHS[difficulty];
  paddleX = (SCREEN_WIDTH - paddleW) / 2;
  for (int i = 0; i < MAX_CAPSULES; i++) capsules[i].type = 0;
  loadLevel();
}

void BreakoutGame::loadLevel() {
  bricksLeft = 0;
  for (int row = 0; row < BRICK_ROWS; row++) {
    for (int col = 0; col < BRICK_COLS; col++) {
      bricks[row][col] = LAYOUTS[level - 1][row][col] == '#';
      if (bricks[row][col]) bricksLeft++;
    }
  }
  for (int i = 0; i < MAX_BALLS; i++) balls[i].active = false;
  for (int i = 0; i < MAX_CAPSULES; i++) capsules[i].type = 0;
  launchBall(0);
  bannerUntil = millis() + 1200;
}

void BreakoutGame::launchBall(int index) {
  Ball& b = balls[index];
  b.active = true;
  b.x = SCREEN_WIDTH / 2;
  b.y = PADDLE_Y - 10;

  // Random angle upward; each level is a little faster
  float speed = baseSpeed + 0.12 * (level - 1);
  float angle = random(-45, 46) * PI / 180.0;
  b.velX = sin(angle) * speed;
  b.velY = -fabs(cos(angle)) * speed;
}

int BreakoutGame::activeBalls() {
  int n = 0;
  for (int i = 0; i < MAX_BALLS; i++) if (balls[i].active) n++;
  return n;
}

void BreakoutGame::update() {
  if (gameOver) return;
  unsigned long now = millis();

  // Paddle: continuous input for smooth control
  int base = PADDLE_WIDTHS[difficulty];
  paddleW = now < wideUntil ? base + 10 : base;
  if (buttons.left && paddleX > 0) paddleX = max(0.0f, paddleX - 2);
  if (buttons.right && paddleX < SCREEN_WIDTH - paddleW) paddleX = min((float)(SCREEN_WIDTH - paddleW), paddleX + 2);
  paddleX = constrain(paddleX, 0, SCREEN_WIDTH - paddleW);

  if (now < bannerUntil) return;   // "LEVEL n"

  updateBalls();
  updateCapsules();

  if (activeBalls() == 0) {
    lives--;
    if (lives <= 0) {
      gameOver = true;
      return;
    }
    wideUntil = slowUntil = 0;
    for (int i = 0; i < MAX_CAPSULES; i++) capsules[i].type = 0;
    launchBall(0);
    bannerUntil = millis() + 600;
  }

  if (bricksLeft == 0) {
    if (level >= BREAKOUT_LEVELS) {
      gameWon = true;
      gameOver = true;
    } else {
      level++;
      score += 50;
      fxPopText(SCREEN_WIDTH / 2, 40, "LEVEL UP");
      loadLevel();
    }
  }
}

void BreakoutGame::updateBalls() {
  float slow = millis() < slowUntil ? 0.65 : 1.0;

  for (int i = 0; i < MAX_BALLS; i++) {
    Ball& b = balls[i];
    if (!b.active) continue;

    b.x += b.velX * slow;
    b.y += b.velY * slow;

    // Wall collisions (left/right)
    if (b.x <= 0 || b.x >= SCREEN_WIDTH - BALL_SIZE) {
      b.velX = -b.velX;
      b.x = constrain(b.x, 0, SCREEN_WIDTH - BALL_SIZE);
    }

    // Top wall collision
    if (b.y <= 0) {
      b.velY = -b.velY;
      b.y = 0;
    }

    // Bottom: this ball is lost
    if (b.y >= SCREEN_HEIGHT) {
      b.active = false;
      continue;
    }

    // Paddle: the hit position sets the angle
    if (b.velY > 0 &&
        b.x + BALL_SIZE >= paddleX && b.x <= paddleX + paddleW &&
        b.y + BALL_SIZE >= PADDLE_Y && b.y <= PADDLE_Y + PADDLE_HEIGHT) {
      float speed = sqrt(b.velX * b.velX + b.velY * b.velY);
      float hitPos = (b.x + BALL_SIZE / 2 - paddleX) / paddleW;   // 0 .. 1
      float velX = (hitPos - 0.5) * 3.0;
      velX = constrain(velX, -2.5, 2.5);
      b.velX = velX;
      b.velY = -sqrt(max(0.5f, speed * speed - velX * velX));
    }

    collideBrick(b);
  }
}

bool BreakoutGame::collideBrick(Ball& b) {
  for (int row = 0; row < BRICK_ROWS; row++) {
    for (int col = 0; col < BRICK_COLS; col++) {
      if (!bricks[row][col]) continue;

      int brickX = col * BRICK_WIDTH + (SCREEN_WIDTH - BRICK_COLS * BRICK_WIDTH) / 2;
      int brickY = BRICK_OFFSET_Y + row * (BRICK_HEIGHT + 1);

      if (b.x + BALL_SIZE >= brickX && b.x <= brickX + BRICK_WIDTH &&
          b.y + BALL_SIZE >= brickY && b.y <= brickY + BRICK_HEIGHT) {

        // Bounce on the side that was hit
        float deltaX = (b.x + BALL_SIZE / 2) - (brickX + BRICK_WIDTH / 2);
        float deltaY = (b.y + BALL_SIZE / 2) - (brickY + BRICK_HEIGHT / 2);
        if (fabs(deltaX) > fabs(deltaY)) b.velX = -b.velX;
        else                             b.velY = -b.velY;

        breakBrick(row, col);
        return true;
      }
    }
  }
  return false;
}

void BreakoutGame::breakBrick(int row, int col) {
  bricks[row][col] = false;
  bricksLeft--;
  int points = (BRICK_ROWS - row) * 10;
  score += points;

  int x = col * BRICK_WIDTH + (SCREEN_WIDTH - BRICK_COLS * BRICK_WIDTH) / 2 + BRICK_WIDTH / 2;
  int y = BRICK_OFFSET_Y + row * (BRICK_HEIGHT + 1);
  fxPop(x, y, points);
  if (random(100) < 16) spawnCapsule(x - 3, y);
}

// ---------------------------------------------------------------------------
// Capsules
// ---------------------------------------------------------------------------
void BreakoutGame::spawnCapsule(float x, float y) {
  for (int i = 0; i < MAX_CAPSULES; i++) {
    if (capsules[i].type) continue;
    capsules[i].x = x;
    capsules[i].y = y;
    int r = random(100);
    capsules[i].type = r < 35 ? 1 : (r < 60 ? 2 : (r < 90 ? 3 : 4));
    return;
  }
}

void BreakoutGame::updateCapsules() {
  for (int i = 0; i < MAX_CAPSULES; i++) {
    Capsule& c = capsules[i];
    if (!c.type) continue;
    c.y += 1.1;
    if (c.y > SCREEN_HEIGHT) { c.type = 0; continue; }
    if (c.y + 7 >= PADDLE_Y && c.y <= PADDLE_Y + PADDLE_HEIGHT &&
        c.x + 7 >= paddleX && c.x <= paddleX + paddleW) {
      applyCapsule(c.type);
      c.type = 0;
    }
  }
}

void BreakoutGame::applyCapsule(uint8_t type) {
  unsigned long now = millis();
  switch (type) {
    case 1:
      wideUntil = now + POWER_MS;
      fxPopText((int)paddleX + paddleW / 2, PADDLE_Y - 6, "WIDE");
      break;
    case 2: {
      // Split every ball that is in play now in two (up to MAX_BALLS)
      bool original[MAX_BALLS];
      for (int i = 0; i < MAX_BALLS; i++) original[i] = balls[i].active;
      for (int i = 0; i < MAX_BALLS; i++) {
        if (!original[i]) continue;
        for (int j = 0; j < MAX_BALLS; j++) {
          if (balls[j].active) continue;
          balls[j] = balls[i];
          balls[j].velX = -balls[i].velX + (random(2) ? 0.4 : -0.4);
          break;
        }
      }
      fxPopText((int)paddleX + paddleW / 2, PADDLE_Y - 6, "MULTI");
      break;
    }
    case 3:
      slowUntil = now + POWER_MS;
      fxPopText((int)paddleX + paddleW / 2, PADDLE_Y - 6, "SLOW");
      break;
    default:
      lives = min(lives + 1, 9);
      fxPopText((int)paddleX + paddleW / 2, PADDLE_Y - 6, "+LIFE");
      break;
  }
  score += 25;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void BreakoutGame::draw() {
  clearDisplay();

  drawBricks();
  display.fillRect((int)paddleX, PADDLE_Y, paddleW, PADDLE_HEIGHT, SSD1306_WHITE);
  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls[i].active) display.fillRect((int)balls[i].x, (int)balls[i].y, BALL_SIZE, BALL_SIZE, SSD1306_WHITE);
  }

  // Capsules: a small box with the letter
  static const char LETTERS[] = {'?', 'W', 'M', 'S', '+'};
  for (int i = 0; i < MAX_CAPSULES; i++) {
    if (!capsules[i].type) continue;
    display.drawRect((int)capsules[i].x, (int)capsules[i].y, 7, 8, SSD1306_WHITE);
    display.setCursor((int)capsules[i].x + 1, (int)capsules[i].y + 1);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.print(LETTERS[capsules[i].type]);
  }

  drawUI();

  if (millis() < bannerUntil && activeBalls() > 0) {
    char text[12];
    snprintf(text, sizeof(text), "LEVEL %d", level);
    display.fillRect(34, 34, 60, 11, SSD1306_BLACK);
    drawCenteredText(text, 36, 1);
  }
  if (gameWon) drawCenteredText("YOU WIN!", 30, 1);
}

void BreakoutGame::drawBricks() {
  for (int row = 0; row < BRICK_ROWS; row++) {
    for (int col = 0; col < BRICK_COLS; col++) {
      if (bricks[row][col]) {
        int x = col * BRICK_WIDTH + (SCREEN_WIDTH - BRICK_COLS * BRICK_WIDTH) / 2;
        int y = BRICK_OFFSET_Y + row * (BRICK_HEIGHT + 1);
        display.fillRect(x, y, BRICK_WIDTH - 1, BRICK_HEIGHT, SSD1306_WHITE);
      }
    }
  }
}

void BreakoutGame::drawUI() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(score);
  display.setCursor(SCREEN_WIDTH / 2 - 15, 0);
  display.print(F("L"));
  display.print(level);

  // Lives as small paddles
  for (int i = 0; i < lives && i < 9; i++) {
    display.fillRect(SCREEN_WIDTH - 8 - i * 6, 2, 4, 3, SSD1306_WHITE);
  }
}
