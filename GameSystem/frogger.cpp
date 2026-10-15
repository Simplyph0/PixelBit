#include "frogger.h"
#include "display.h"
#include "input.h"
#include "fx.h"

FroggerGame froggerGame;

// Speed multiplier and extra gap tightness by option level
static const float SPEED_MULT[DIFFICULTY_LEVELS] = {0.7, 0.9, 1.0, 1.2, 1.4};
static const int GAP_TIGHTEN[DIFFICULTY_LEVELS]  = {-8, -3, 0, 5, 10};   // positive: sparser logs, denser traffic

// A random gap for a lane. Harder levels mean fewer cars (roomier road gaps are
// easier, so they shrink) and sparser logs (their gaps grow).
int FroggerGame::gapFor(int row) {
  Lane& lane = lanes[row];
  int gap = random(lane.minGap, lane.maxGap + 1);
  return gap + (row <= 3 ? GAP_TIGHTEN[difficulty] : -GAP_TIGHTEN[difficulty]);
}

void FroggerGame::init() {
  score = 0;
  lives = 3;
  level = 1;
  gameOver = false;
  setupLanes();
  resetFrog();
}

// Lane layout: alternating directions, a different speed for each
void FroggerGame::setupLanes() {
  memset(lanes, 0, sizeof(lanes));

  // river: logs (rows 1..3), road: cars (rows 5..7)
  const float riverSpeed[3] = {0.45, 0.65, 0.5};
  const float roadSpeed[3]  = {0.7, 1.1, 0.85};
  for (int i = 0; i < 3; i++) {
    Lane& river = lanes[1 + i];
    river.dir = (i % 2 == 0) ? -1 : 1;
    river.speed = riverSpeed[i];
    river.minGap = 14; river.maxGap = 30;
    river.minW = 20; river.maxW = 32;

    Lane& road = lanes[5 + i];
    road.dir = (i % 2 == 0) ? 1 : -1;
    road.speed = roadSpeed[i];
    road.minGap = 26; road.maxGap = 60;
    road.minW = 9; road.maxW = 15;
  }

  // Fill each lane with objects already on the way
  for (int r = 1; r < ROWS; r++) {
    Lane& lane = lanes[r];
    if (lane.speed == 0) continue;
    float x = random(0, 24);
    for (int k = 0; k < MAX_LANE_OBJECTS && x < SCREEN_WIDTH; k++) {
      lane.objects[k].active = true;
      lane.objects[k].w = random(lane.minW, lane.maxW + 1);
      lane.objects[k].x = x;
      x += lane.objects[k].w + gapFor(r);
    }
    lane.sinceSpawn = 0;
    lane.nextGap = lane.minGap;
  }
}

void FroggerGame::resetFrog() {
  frogX = SCREEN_WIDTH / 2 - FROG_W / 2;
  frogRow = ROWS - 1;
  highestRow = ROWS - 1;
  resumeAt = millis() + 500;
}

void FroggerGame::update() {
  handleInput();
  if (gameOver || millis() < resumeAt) return;

  updateLanes();

  // Drifting with a log
  if (frogRow >= 1 && frogRow <= 3) {
    Lane& lane = lanes[frogRow];
    if (onRiver()) frogX += lane.dir * lane.speed * SPEED_MULT[difficulty] * (1 + 0.08 * (level - 1));
  }

  if (frogRow >= 1 && frogRow <= 3 && (!onRiver() || frogX < -FROG_W / 2 || frogX > SCREEN_WIDTH - FROG_W / 2)) {
    die();
    return;
  }
  if (frogRow >= 5 && frogRow <= 7 && hitByCar()) {
    die();
    return;
  }

  // Reached the far bank
  if (frogRow == 0) {
    int gained = 100 * level;
    score += gained;
    fxPop((int)frogX + 2, 6, gained);
    level++;
    resetFrog();
  }
}

void FroggerGame::die() {
  lives--;
  fxPopText((int)frogX + 2, frogRow * ROW_H, "SPLAT");
  if (lives <= 0) {
    gameOver = true;
  } else {
    resetFrog();
    resumeAt = millis() + 900;
  }
}

void FroggerGame::handleInput() {
  if (gameOver || millis() < resumeAt) return;

  if (buttons.upPressed && frogRow > 0) {
    frogRow--;
    if (frogRow < highestRow) {
      highestRow = frogRow;
      score += 10;
    }
  }
  else if (buttons.downPressed && frogRow < ROWS - 1) {
    frogRow++;
  }
  else if (buttons.leftPressed) {
    frogX = max(0.0f, frogX - FROG_W);
  }
  else if (buttons.rightPressed) {
    frogX = min((float)(SCREEN_WIDTH - FROG_W), frogX + FROG_W);
  }
}

void FroggerGame::spawn(Lane& lane) {
  for (int k = 0; k < MAX_LANE_OBJECTS; k++) {
    LaneObject& o = lane.objects[k];
    if (o.active) continue;
    o.active = true;
    o.w = random(lane.minW, lane.maxW + 1);
    o.x = lane.dir > 0 ? -o.w : SCREEN_WIDTH;
    lane.sinceSpawn = 0;
    lane.nextGap = o.w + gapFor(&lane - lanes);
    return;
  }
}

void FroggerGame::updateLanes() {
  float mult = SPEED_MULT[difficulty] * (1 + 0.08 * (level - 1));
  for (int r = 1; r < ROWS; r++) {
    Lane& lane = lanes[r];
    if (lane.speed == 0) continue;
    float step = lane.dir * lane.speed * mult;

    for (int k = 0; k < MAX_LANE_OBJECTS; k++) {
      LaneObject& o = lane.objects[k];
      if (!o.active) continue;
      o.x += step;
      if ((lane.dir > 0 && o.x > SCREEN_WIDTH) || (lane.dir < 0 && o.x + o.w < 0)) o.active = false;
    }

    lane.sinceSpawn += fabs(step);
    if (lane.sinceSpawn >= max(8.0f, lane.nextGap)) spawn(lane);
  }
}

// The frog is on a log when its middle is over one
bool FroggerGame::onRiver() {
  Lane& lane = lanes[frogRow];
  float mid = frogX + FROG_W / 2.0;
  for (int k = 0; k < MAX_LANE_OBJECTS; k++) {
    LaneObject& o = lane.objects[k];
    if (o.active && mid >= o.x && mid <= o.x + o.w) return true;
  }
  return false;
}

bool FroggerGame::hitByCar() {
  Lane& lane = lanes[frogRow];
  for (int k = 0; k < MAX_LANE_OBJECTS; k++) {
    LaneObject& o = lane.objects[k];
    if (o.active && frogX + FROG_W > o.x + 1 && frogX < o.x + o.w - 1) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void FroggerGame::draw() {
  clearDisplay();
  drawLanes();
  drawFrog();
  drawUI();
}

void FroggerGame::drawLanes() {
  // Banks: lines above the goal row, the median and the start row
  display.drawFastHLine(0, 1 * ROW_H, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastHLine(0, 4 * ROW_H, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastHLine(0, 5 * ROW_H, SCREEN_WIDTH, SSD1306_WHITE);
  display.drawFastHLine(0, 8 * ROW_H, SCREEN_WIDTH, SSD1306_WHITE);

  // Water: a dotted pattern that drifts
  int ripple = (millis() / 300) % 2;
  for (int r = 1; r <= 3; r++) {
    for (int x = (r + ripple) % 4; x < SCREEN_WIDTH; x += 8) {
      display.drawPixel(x, r * ROW_H + 5, SSD1306_WHITE);
    }
  }

  // Dashed lane dividers on the road
  for (int r = 6; r <= 7; r++) {
    for (int x = 0; x < SCREEN_WIDTH; x += 8) display.drawPixel(x, r * ROW_H, SSD1306_WHITE);
  }

  for (int r = 1; r < ROWS; r++) {
    Lane& lane = lanes[r];
    if (lane.speed == 0) continue;
    int y = r * ROW_H;
    for (int k = 0; k < MAX_LANE_OBJECTS; k++) {
      LaneObject& o = lane.objects[k];
      if (!o.active) continue;
      int x = (int)o.x;
      if (r <= 3) {
        // Log with bark marks
        display.fillRect(x, y + 1, o.w, 5, SSD1306_WHITE);
        for (int bx = x + 3; bx < x + o.w - 2; bx += 5) display.drawPixel(bx, y + 3, SSD1306_BLACK);
      } else {
        // Car or truck, windows towards its direction of travel
        display.fillRect(x, y + 1, o.w, 5, SSD1306_WHITE);
        int wx = lane.dir > 0 ? x + o.w - 4 : x + 1;
        display.fillRect(wx, y + 2, 3, 2, SSD1306_BLACK);
        display.drawPixel(x + 2, y + 6, SSD1306_WHITE);
        display.drawPixel(x + o.w - 3, y + 6, SSD1306_WHITE);
      }
    }
  }
}

void FroggerGame::drawFrog() {
  if (millis() < resumeAt && (millis() / 120) % 2) return;   // blinks while respawning
  int x = (int)frogX, y = frogRow * ROW_H + 1;
  // Drawn by inverting what is behind it, so the frog shows on anything: white on the dark water
  // and road, black on a log or a car (which are solid white). The eyes are inverted back, so
  // they always show the background.
  for (int j = 1; j < FROG_H; j++)
    for (int i = 0; i < FROG_W; i++) display.drawPixel(x + i, y + j, SSD1306_INVERSE);
  display.drawPixel(x, y, SSD1306_INVERSE);
  display.drawPixel(x + FROG_W - 1, y, SSD1306_INVERSE);
  display.drawPixel(x + 1, y + 1, SSD1306_INVERSE);
  display.drawPixel(x + FROG_W - 2, y + 1, SSD1306_INVERSE);
}

void FroggerGame::drawUI() {
  // The goal row is free for the score line
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(score);
  display.setCursor(SCREEN_WIDTH / 2 - 9, 0);
  display.print(F("L"));
  display.print(level);
  for (int i = 0; i < lives; i++) {
    display.fillRect(SCREEN_WIDTH - 7 - i * 7, 1, 5, 4, SSD1306_WHITE);
  }
}
