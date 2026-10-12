#include "invaders.h"
#include "display.h"
#include "input.h"
#include "fx.h"

InvadersGame invadersGame;

// By option level: lives, how often the aliens fire (percent per frame, plus
// the wave number) and how much faster they march
static const int START_LIVES[DIFFICULTY_LEVELS] = {5, 4, 3, 3, 2};
static const int FIRE_BASE[DIFFICULTY_LEVELS]   = {1, 2, 3, 4, 6};
static const int MARCH_BIAS[DIFFICULTY_LEVELS]  = {30, 15, 0, -15, -30};   // ms added to the step interval

static const char* const UFO_SPRITE[INV_UFO_H] = {
  "....XXXXX....", "..XXXXXXXXX..", ".XX.XX.XX.XX.", "XXXXXXXXXXXXX", "..X.......X.."
};

// Shield shape, 12 x 6: a dome with an arch cut out of the bottom
static const uint16_t SHIELD_SHAPE[INV_SHIELD_H] = {
  0b001111111100, 0b011111111110, 0b111111111111, 0b111111111111, 0b111100001111, 0b111000000111
};
static const int SHIELD_X[INV_SHIELDS] = {12, 40, 68, 96};

static const char* const ALIEN_A[INV_ALIEN_H] = {
  "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", "..X..X..", ".X....X."
};
static const char* const ALIEN_B[INV_ALIEN_H] = {
  "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", ".X.XX.X.", "X......X"
};
static const char* const SHIP[INV_SHIP_H] = {
  "....X....", "..XXXXX..", "XXXXXXXXX", "XXXXXXXXX"
};

static void drawSprite(const char* const* rows, int h, int x, int y) {
  for (int r = 0; r < h; r++) {
    for (int c = 0; rows[r][c]; c++) {
      if (rows[r][c] == 'X') display.drawPixel(x + c, y + r, SSD1306_WHITE);
    }
  }
}

void InvadersGame::init() {
  shipX = (SCREEN_WIDTH - INV_SHIP_W) / 2;
  lives = START_LIVES[level];
  score = 0;
  wave = 1;
  gameOver = false;
  lastUpdate = millis();
  pauseUntil = 0;
  banner[0] = 0;
  startWave();
}

void InvadersGame::startWave() {
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      alive[r][c] = true;
  aliveCount = INV_ROWS * INV_COLS;
  alienX = 8;
  alienY = 18 + min(wave - 1, 3) * 3;   // below the bonus ship's lane
  resetShields();
  ufoActive = false;
  ufoNextAt = millis() + random(10000, 18000);
  alienDir = 1;
  animFrame = false;
  lastStep = millis();
  playerBullet.active = false;
  for (int i = 0; i < INV_MAX_ENEMY_BULLETS; i++) enemyBullets[i].active = false;
}

void InvadersGame::showBanner(const char* text, unsigned long ms) {
  strncpy(banner, text, sizeof(banner) - 1);
  banner[sizeof(banner) - 1] = 0;
  pauseUntil = millis() + ms;
}

void InvadersGame::update() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 40.0;   // 1.0 = one 40 ms frame
  lastUpdate = now;
  if (dt > 2.0) dt = 2.0;

  if (gameOver) return;

  if (now < pauseUntil) return;
  banner[0] = 0;

  // Ship: left / right move, up or down fires
  if (buttons.left)  shipX -= 3.0 * dt;
  if (buttons.right) shipX += 3.0 * dt;
  shipX = constrain(shipX, 0, SCREEN_WIDTH - INV_SHIP_W);

  if ((buttons.up || buttons.down) && !playerBullet.active) {
    playerBullet.x = shipX + INV_SHIP_W / 2;
    playerBullet.y = INV_SHIP_Y - 2;
    playerBullet.active = true;
  }

  movePlayerBullet(dt);
  moveUfo(dt);

  // Alien march: faster with fewer aliens and on later waves
  long interval = 120 + aliveCount * 30 - (wave - 1) * 40 + MARCH_BIAS[level];
  if (interval < 60) interval = 60;
  if (now - lastStep >= (unsigned long)interval) {
    lastStep = now;
    stepAliens();
  }

  maybeFire();
  moveEnemyBullets(dt);

  // Aliens reached the ship line
  for (int r = INV_ROWS - 1; r >= 0; r--) {
    bool any = false;
    for (int c = 0; c < INV_COLS; c++) any |= alive[r][c];
    if (any) {
      if (alienY + r * INV_CELL_H + INV_ALIEN_H >= INV_SHIP_Y - 1) gameOver = true;
      break;
    }
  }

  if (lives <= 0) gameOver = true;

  if (!gameOver && aliveCount == 0) {
    wave++;
    score += 50;
    startWave();
    showBanner("WAVE", 900);
  }
}

void InvadersGame::stepAliens() {
  animFrame = !animFrame;

  int minC = INV_COLS, maxC = -1;
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (alive[r][c]) { minC = min(minC, c); maxC = max(maxC, c); }
  if (maxC < 0) return;

  int left  = alienX + minC * INV_CELL_W;
  int right = alienX + maxC * INV_CELL_W + INV_ALIEN_W;

  if ((alienDir > 0 && right + 2 > SCREEN_WIDTH - 1) || (alienDir < 0 && left - 2 < 0)) {
    alienY += 4;
    alienDir = -alienDir;
  } else {
    alienX += 2 * alienDir;
  }

  // Aliens that reach the shields chew through them
  for (int r = 0; r < INV_ROWS; r++) {
    for (int c = 0; c < INV_COLS; c++) {
      if (!alive[r][c]) continue;
      int ax = alienX + c * INV_CELL_W, ay = alienY + r * INV_CELL_H;
      if (ay + INV_ALIEN_H < INV_SHIELD_Y || ay > INV_SHIELD_Y + INV_SHIELD_H) continue;
      for (int s = 0; s < INV_SHIELDS; s++) {
        for (int sy = 0; sy < INV_SHIELD_H; sy++) {
          int py = INV_SHIELD_Y + sy;
          if (py < ay || py >= ay + INV_ALIEN_H) continue;
          for (int sx = 0; sx < INV_SHIELD_W; sx++) {
            int px = SHIELD_X[s] + sx;
            if (px >= ax && px < ax + INV_ALIEN_W) shield[s][sy] &= ~(1 << (INV_SHIELD_W - 1 - sx));
          }
        }
      }
    }
  }
}

void InvadersGame::resetShields() {
  for (int s = 0; s < INV_SHIELDS; s++)
    for (int r = 0; r < INV_SHIELD_H; r++) shield[s][r] = SHIELD_SHAPE[r];
}

// True if (x, y) is a solid shield pixel; the shot wears a small hole in it
bool InvadersGame::shieldHit(float x, float y) {
  int px = (int)x, py = (int)y;
  if (py < INV_SHIELD_Y || py >= INV_SHIELD_Y + INV_SHIELD_H) return false;
  for (int s = 0; s < INV_SHIELDS; s++) {
    int sx = px - SHIELD_X[s], sy = py - INV_SHIELD_Y;
    if (sx < 0 || sx >= INV_SHIELD_W) continue;
    if (!(shield[s][sy] & (1 << (INV_SHIELD_W - 1 - sx)))) return false;

    // erode the hit pixel and some of its neighbours
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        int nx = sx + dx, ny = sy + dy;
        if (nx < 0 || nx >= INV_SHIELD_W || ny < 0 || ny >= INV_SHIELD_H) continue;
        if ((dx == 0 && dy == 0) || random(100) < 45) shield[s][ny] &= ~(1 << (INV_SHIELD_W - 1 - nx));
      }
    }
    return true;
  }
  return false;
}

// A bonus ship crosses the top now and then; shoot it for 50 to 300 points
void InvadersGame::moveUfo(float dt) {
  unsigned long now = millis();
  if (!ufoActive) {
    if (now >= ufoNextAt && aliveCount > 3) {
      ufoActive = true;
      ufoDir = random(2) ? 1 : -1;
      ufoX = ufoDir > 0 ? -INV_UFO_W : SCREEN_WIDTH;
    }
    return;
  }

  ufoX += ufoDir * 1.3 * dt;
  if (playerBullet.active && playerBullet.x >= ufoX && playerBullet.x < ufoX + INV_UFO_W &&
      playerBullet.y <= INV_UFO_Y + INV_UFO_H && playerBullet.y + 2 >= INV_UFO_Y) {
    static const int PRIZE[4] = {50, 100, 150, 300};
    int points = PRIZE[random(4)];
    score += points;
    fxPop((int)ufoX + INV_UFO_W / 2, INV_UFO_Y + 6, points);
    playerBullet.active = false;
    ufoActive = false;
    ufoNextAt = now + random(12000, 22000);
  } else if (ufoX < -INV_UFO_W - 2 || ufoX > SCREEN_WIDTH + 2) {
    ufoActive = false;
    ufoNextAt = now + random(12000, 22000);
  }
}

void InvadersGame::movePlayerBullet(float dt) {
  if (!playerBullet.active) return;

  float total = 4.0 * dt;
  while (total > 0 && playerBullet.active) {
    float step = min(total, 2.0f);
    total -= step;
    playerBullet.y -= step;

    if (playerBullet.y < 10) { playerBullet.active = false; return; }
    if (shieldHit(playerBullet.x, playerBullet.y)) { playerBullet.active = false; return; }

    for (int r = 0; r < INV_ROWS; r++) {
      for (int c = 0; c < INV_COLS; c++) {
        if (!alive[r][c]) continue;
        int ax = alienX + c * INV_CELL_W;
        int ay = alienY + r * INV_CELL_H;
        if (playerBullet.x >= ax && playerBullet.x < ax + INV_ALIEN_W &&
            playerBullet.y <= ay + INV_ALIEN_H - 1 && playerBullet.y + 2 >= ay) {
          alive[r][c] = false;
          aliveCount--;
          score += (INV_ROWS - r) * 10;
          fxPop(ax + INV_ALIEN_W / 2, ay, (INV_ROWS - r) * 10);
          playerBullet.active = false;
          return;
        }
      }
    }
  }
}

void InvadersGame::maybeFire() {
  if (random(100) >= FIRE_BASE[level] + wave) return;

  int slot = -1;
  for (int i = 0; i < INV_MAX_ENEMY_BULLETS; i++) {
    if (!enemyBullets[i].active) { slot = i; break; }
  }
  if (slot < 0) return;

  int col = random(INV_COLS);
  for (int r = INV_ROWS - 1; r >= 0; r--) {
    if (alive[r][col]) {
      enemyBullets[slot].x = alienX + col * INV_CELL_W + INV_ALIEN_W / 2;
      enemyBullets[slot].y = alienY + r * INV_CELL_H + INV_ALIEN_H;
      enemyBullets[slot].active = true;
      return;
    }
  }
}

void InvadersGame::moveEnemyBullets(float dt) {
  for (int i = 0; i < INV_MAX_ENEMY_BULLETS; i++) {
    InvBullet& b = enemyBullets[i];
    if (!b.active) continue;

    b.y += (1.8 + 0.2 * wave) * dt;
    if (b.y >= SCREEN_HEIGHT - 2) { b.active = false; continue; }
    if (shieldHit(b.x, b.y + 2)) { b.active = false; continue; }

    if (b.x >= shipX && b.x < shipX + INV_SHIP_W &&
        b.y + 3 >= INV_SHIP_Y && b.y <= INV_SHIP_Y + INV_SHIP_H) {
      lives--;
      for (int j = 0; j < INV_MAX_ENEMY_BULLETS; j++) enemyBullets[j].active = false;
      playerBullet.active = false;
      if (lives > 0) pauseUntil = millis() + 800;
      return;
    }
  }
}

void InvadersGame::draw() {
  clearDisplay();

  // HUD
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(score);
  display.setCursor(SCREEN_WIDTH / 2 - 9, 0);
  display.print('W');
  display.print(wave);
  for (int i = 0; i < lives; i++) {
    display.fillRect(SCREEN_WIDTH - 6 - i * 8, 1, 5, 4, SSD1306_WHITE);
  }
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastHLine(0, SCREEN_HEIGHT - 2, SCREEN_WIDTH, SSD1306_WHITE);

  // Aliens
  const char* const* sprite = animFrame ? ALIEN_B : ALIEN_A;
  for (int r = 0; r < INV_ROWS; r++)
    for (int c = 0; c < INV_COLS; c++)
      if (alive[r][c])
        drawSprite(sprite, INV_ALIEN_H, alienX + c * INV_CELL_W, alienY + r * INV_CELL_H);

  // Shields
  for (int s = 0; s < INV_SHIELDS; s++)
    for (int r = 0; r < INV_SHIELD_H; r++)
      for (int c = 0; c < INV_SHIELD_W; c++)
        if (shield[s][r] & (1 << (INV_SHIELD_W - 1 - c))) display.drawPixel(SHIELD_X[s] + c, INV_SHIELD_Y + r, SSD1306_WHITE);

  // Bonus ship
  if (ufoActive) drawSprite(UFO_SPRITE, INV_UFO_H, (int)ufoX, INV_UFO_Y);

  // Ship (blinks while frozen after a hit)
  bool hitPause = millis() < pauseUntil && banner[0] == 0;
  if (!hitPause || (millis() / 100) % 2) {
    drawSprite(SHIP, INV_SHIP_H, (int)shipX, INV_SHIP_Y);
  }

  // Bullets
  if (playerBullet.active) {
    display.drawFastVLine((int)playerBullet.x, (int)playerBullet.y, 3, SSD1306_WHITE);
  }
  for (int i = 0; i < INV_MAX_ENEMY_BULLETS; i++) {
    if (enemyBullets[i].active) {
      display.drawFastVLine((int)enemyBullets[i].x, (int)enemyBullets[i].y, 3, SSD1306_WHITE);
    }
  }

  if (banner[0]) {
    char text[16];
    snprintf(text, sizeof(text), "%s %d", banner, wave);
    drawCenteredText(text, 28, 1);
  }

}
