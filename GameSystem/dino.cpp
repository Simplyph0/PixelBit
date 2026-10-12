#include "dino.h"
#include "display.h"
#include "input.h"

DinoGame dinoGame;

// Start and top speed (px per frame), by option level
static const float START_SPEED[DIFFICULTY_LEVELS] = {2.4, 3.0, 3.4, 3.8, 4.2};
static const float TOP_SPEED[DIFFICULTY_LEVELS]   = {4.5, 5.5, 6.0, 6.5, 7.0};

// Back to normal colours when the game is left
void DinoGame::stop() {
  display.invertDisplay(false);
  night = false;
}

void DinoGame::init() {
  height = 0;
  velY = 0;
  ducking = false;
  speed = START_SPEED[level];
  night = false;
  display.invertDisplay(false);
  scoreF = 0;
  groundScroll = 0;
  distSinceSpawn = 0;
  nextGap = 70;
  gameOver = false;
  lastUpdate = millis();
  animClock = 0;
  for (int i = 0; i < DINO_MAX_OBSTACLES; i++) obstacles[i].active = false;
}

void DinoGame::update() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 40.0;   // 1.0 = one 40 ms frame
  lastUpdate = now;
  if (dt > 2.0) dt = 2.0;
  if (gameOver) return;

  animClock = now;
  bool onGround = height <= 0;

  // Jump with UP or RIGHT, duck with DOWN
  if ((buttons.up || buttons.right) && onGround) {
    velY = 4.6;
    onGround = false;
  }
  ducking = buttons.down && onGround;

  // Gravity; DOWN in mid-air fast-falls
  float g = 0.45 + ((buttons.down && !onGround) ? 0.9 : 0.0);
  velY -= g * dt;
  height += velY * dt;
  if (height <= 0) { height = 0; velY = 0; }

  speed = min(START_SPEED[level] + scoreF / 150.0f, TOP_SPEED[level]);
  scoreF += speed * dt * 0.12;

  // Day and night: the screen inverts every 700 points
  bool wantNight = ((int)scoreF / 700) % 2 == 1;
  if (wantNight != night) {
    night = wantNight;
    display.invertDisplay(night);
  }
  groundScroll += speed * dt;

  distSinceSpawn += speed * dt;
  for (int i = 0; i < DINO_MAX_OBSTACLES; i++) {
    if (!obstacles[i].active) continue;
    obstacles[i].x -= speed * dt;
    if (obstacles[i].x < -20) obstacles[i].active = false;
  }
  if (distSinceSpawn >= nextGap) spawnObstacle();

  if (checkCollision()) gameOver = true;
}

void DinoGame::spawnObstacle() {
  for (int i = 0; i < DINO_MAX_OBSTACLES; i++) {
    if (obstacles[i].active) continue;
    int maxType = (scoreF >= 200) ? DINO_BIRD_MID : DINO_CACTUS_DOUBLE;
    obstacles[i].type = random(0, maxType + 1);
    obstacles[i].x = SCREEN_WIDTH;
    obstacles[i].active = true;
    distSinceSpawn = 0;
    // Leave room to land and jump again; the gap grows with speed
    nextGap = min(40.0f + speed * 9.0f + random(0, 40), 118.0f);
    return;
  }
}

void DinoGame::obstacleSize(const DinoObstacle& o, int& w, int& h, int& bottom) {
  bottom = 0;
  switch (o.type) {
    case DINO_CACTUS_SMALL:  w = 7;  h = 12; break;
    case DINO_CACTUS_LARGE:  w = 9;  h = 17; break;
    case DINO_CACTUS_DOUBLE: w = 16; h = 12; break;
    case DINO_BIRD_LOW:      w = 12; h = 8;  bottom = 2;  break;
    default:                 w = 12; h = 8;  bottom = 10; break;
  }
}

bool DinoGame::checkCollision() {
  int dw = ducking ? DINO_DUCK_WIDTH : DINO_WIDTH;
  int dh = ducking ? DINO_DUCK_HEIGHT : DINO_HEIGHT;
  // Hitboxes are a little smaller than the sprites so near misses feel fair
  float dl = DINO_X + 2, dr = DINO_X + dw - 2;
  float db = height + 1, dtop = height + dh - 2;

  for (int i = 0; i < DINO_MAX_OBSTACLES; i++) {
    if (!obstacles[i].active) continue;
    int w, h, bottom;
    obstacleSize(obstacles[i], w, h, bottom);
    float ol = obstacles[i].x + 1, or_ = obstacles[i].x + w - 1;
    float ob = bottom + 1, otop = bottom + h - 1;
    if (dr > ol && dl < or_ && dtop > ob && db < otop) return true;
  }
  return false;
}

void DinoGame::drawDino() {
  bool legA = (animClock / 120) % 2;
  bool air = height > 0;
  int baseY = DINO_GROUND_Y - (int)height;

  if (ducking) {
    int y = baseY - DINO_DUCK_HEIGHT;
    display.fillRect(DINO_X, y + 3, 12, 5, SSD1306_WHITE);        // body
    display.fillRect(DINO_X + 10, y, 6, 5, SSD1306_WHITE);        // head
    display.drawPixel(DINO_X + 13, y + 1, SSD1306_BLACK);         // eye
    display.fillRect(DINO_X + 2, baseY - (legA ? 1 : 0), 2, legA ? 1 : 2, SSD1306_WHITE);
    display.fillRect(DINO_X + 7, baseY - (legA ? 2 : 1), 2, legA ? 2 : 1, SSD1306_WHITE);
  } else {
    int y = baseY - DINO_HEIGHT;
    display.fillRect(DINO_X + 5, y, 7, 6, SSD1306_WHITE);         // head
    display.drawPixel(DINO_X + 9, y + 1, SSD1306_BLACK);          // eye
    display.fillRect(DINO_X + 2, y + 6, 8, 6, SSD1306_WHITE);     // body
    display.fillRect(DINO_X, y + 6, 3, 3, SSD1306_WHITE);         // tail
    display.fillRect(DINO_X + 10, y + 8, 2, 2, SSD1306_WHITE);    // arm
    if (air) {
      display.fillRect(DINO_X + 3, y + 12, 2, 2, SSD1306_WHITE);
      display.fillRect(DINO_X + 7, y + 12, 2, 2, SSD1306_WHITE);
    } else {
      display.fillRect(DINO_X + 3, y + 12, 2, legA ? 2 : 1, SSD1306_WHITE);
      display.fillRect(DINO_X + 7, y + 12, 2, legA ? 1 : 2, SSD1306_WHITE);
    }
  }
}

void DinoGame::drawObstacle(const DinoObstacle& o) {
  int w, h, bottom;
  obstacleSize(o, w, h, bottom);
  int x = (int)o.x;
  int top = DINO_GROUND_Y - bottom - h;

  if (o.type <= DINO_CACTUS_DOUBLE) {
    int count = (o.type == DINO_CACTUS_DOUBLE) ? 2 : 1;
    int cw = (o.type == DINO_CACTUS_LARGE) ? 9 : 7;
    for (int k = 0; k < count; k++) {
      int cx = x + k * 9;
      display.fillRect(cx + cw / 2 - 1, top, 3, h, SSD1306_WHITE);          // trunk
      display.fillRect(cx, top + 3, 2, 4, SSD1306_WHITE);                   // left arm
      display.fillRect(cx, top + 6, cw / 2, 1, SSD1306_WHITE);
      display.fillRect(cx + cw - 2, top + 2, 2, 4, SSD1306_WHITE);          // right arm
      display.fillRect(cx + cw / 2 + 1, top + 5, cw / 2 - 1, 1, SSD1306_WHITE);
    }
  } else {
    bool up = (animClock / 150) % 2;
    display.fillRect(x + 2, top + 3, 9, 3, SSD1306_WHITE);                  // body
    display.fillRect(x, top + 3, 3, 2, SSD1306_WHITE);                      // beak
    if (up) display.fillRect(x + 5, top, 3, 4, SSD1306_WHITE);              // wing up
    else    display.fillRect(x + 5, top + 5, 3, 3, SSD1306_WHITE);          // wing down
  }
}

void DinoGame::draw() {
  clearDisplay();

  // Ground with scrolling bumps
  display.drawFastHLine(0, DINO_GROUND_Y, SCREEN_WIDTH, SSD1306_WHITE);
  int off = (int)groundScroll;
  for (int x = -(off % 23); x < SCREEN_WIDTH; x += 23) {
    display.drawFastHLine(x + 5, DINO_GROUND_Y + 2, 3, SSD1306_WHITE);
    display.drawPixel(x + 15, DINO_GROUND_Y + 3, SSD1306_WHITE);
  }

  for (int i = 0; i < DINO_MAX_OBSTACLES; i++) {
    if (obstacles[i].active) drawObstacle(obstacles[i]);
  }
  drawDino();

  char buf[8];
  snprintf(buf, sizeof(buf), "%05d", (int)scoreF);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_WIDTH - 31, 0);
  display.print(buf);

}
