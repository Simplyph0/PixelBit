#include "pacman.h"
#include "display.h"
#include "input.h"
#include "fx.h"

PacManGame pacmanGame;

// Easiest to hardest: the maze grows and more ghosts join, faster and less
// random. Mazes are open grids with a pillar on every even cell (no dead ends),
// so they are generated from the size alone. Widths and heights include the
// border wall; the largest maze fills the screen under the score line.
struct Level {
  uint8_t w, h;
  uint8_t ghosts;
  uint16_t ghostExtraDelay;   // ghost step = Pac-Man step + this (ms)
  uint8_t randomPct;          // chance a ghost ignores its target and moves randomly
  uint16_t frightenedMs;      // how long a power pellet lasts
};
static const int STAGES = DIFFICULTY_LEVELS;
static const Level LEVELS[STAGES] = {
  {9,  5, 1, 200, 45, 7000},   // EASY
  {11, 5, 2, 150, 30, 6500},   // NORMAL
  {13, 7, 2, 100, 20, 6000},   // HARD
  {15, 7, 3,  60, 12, 5000},   // EXPERT
  {16, 7, 4,  30,  6, 4000},   // INSANE
};

static const int DIR_DX[4] = {1, 0, -1, 0};
static const int DIR_DY[4] = {0, 1, 0, -1};

void PacManGame::init() {
  stage = firstStage;
  lives = 3;
  score = 0;
  gameOver = false;
  gameWon = false;
  startStage();
}

void PacManGame::startStage() {
  const Level& lv = LEVELS[stage];
  mazeW = lv.w;
  mazeH = lv.h;
  offsetX = (SCREEN_WIDTH - mazeW * CELL_SIZE) / 2;
  offsetY = MAZE_TOP + (SCREEN_HEIGHT - MAZE_TOP - mazeH * CELL_SIZE) / 2;
  ghostCount = lv.ghosts;
  ghostRandomPct = lv.randomPct;
  moveDelay = 300;
  ghostDelay = moveDelay + lv.ghostExtraDelay;

  generateMaze();

  const int cornerX[MAX_GHOSTS] = {1, mazeW - 2, 1, mazeW - 2};
  const int cornerY[MAX_GHOSTS] = {1, 1, mazeH - 2, mazeH - 2};
  for (int i = 0; i < MAX_GHOSTS; i++) {
    ghosts[i].startX = cornerX[i];
    ghosts[i].startY = cornerY[i];
    ghosts[i].active = i < ghostCount;
  }
  pacman.x = mazeW / 2;
  pacman.y = mazeH - 2;
  maze[pacman.y][pacman.x] = 2;   // no dot under the start
  totalDots--;

  frightenedUntil = 0;
  bannerUntil = millis() + 1400;
  resetPositions();
}

// Everyone back to their start, with a short pause
void PacManGame::resetPositions() {
  pacman.x = mazeW / 2;
  pacman.y = mazeH - 2;
  pacman.direction = 0;
  pacman.nextDirection = 0;
  for (int i = 0; i < MAX_GHOSTS; i++) {
    ghosts[i].x = ghosts[i].startX;
    ghosts[i].y = ghosts[i].startY;
    ghosts[i].direction = random(0, 4);
    ghosts[i].respawnAt = 0;
  }
  frightenedUntil = 0;
  ghostChain = 0;
  lastMove = lastGhostMove = 0;
  resumeAt = millis() + 1500;
}

void PacManGame::update() {
  handleInput();
  if (gameOver) return;

  unsigned long now = millis();
  if (now < resumeAt) return;   // "ready" pause

  if (now - lastMove > (unsigned long)moveDelay) {
    updatePacMan();
    lastMove = now;
    checkCollisions();   // checked after each mover: both could swap cells and slip past one check
  }

  bool frightened = now < frightenedUntil;
  unsigned long delay = ghostDelay + (frightened ? 120 : 0);
  if (now - lastGhostMove > delay) {
    updateGhosts();
    lastGhostMove = now;
    checkCollisions();
  }

  if (lives <= 0) {
    gameOver = true;
    return;
  }

  if (dotsEaten >= totalDots) {
    score += 100 * (stage + 1);
    fxPop(SCREEN_WIDTH / 2, 30, 100 * (stage + 1));
    if (stage >= STAGES - 1) {
      gameWon = true;
      gameOver = true;
    } else {
      stage++;
      startStage();
    }
  }
}

void PacManGame::draw() {
  clearDisplay();
  
  drawMaze();
  drawGhosts();
  drawPacMan();
  drawUI();

  if (millis() < bannerUntil) {
    char text[16];
    snprintf(text, sizeof(text), "STAGE %d", stage - firstStage + 1);
    display.fillRect(34, 24, 60, 11, SSD1306_BLACK);
    drawCenteredText(text, 26, 1);
  } else if (millis() < resumeAt) {
    display.fillRect(40, 24, 48, 11, SSD1306_BLACK);
    drawCenteredText("READY!", 26, 1);
  }
}

void PacManGame::handleInput() {
  if (buttons.upPressed) {
    pacman.nextDirection = 3;
  }
  else if (buttons.downPressed) {
    pacman.nextDirection = 1;
  }
  else if (buttons.leftPressed) {
    pacman.nextDirection = 2;
  }
  else if (buttons.rightPressed) {
    pacman.nextDirection = 0;
  }
}

void PacManGame::generateMaze() {
  dotsEaten = 0;
  totalDots = 0;

  for (int y = 0; y < MAZE_MAX_H; y++) {
    for (int x = 0; x < MAZE_MAX_W; x++) {
      bool border = x == 0 || y == 0 || x == mazeW - 1 || y == mazeH - 1;
      bool pillar = x % 2 == 0 && y % 2 == 0;
      bool inside = x < mazeW && y < mazeH;
      maze[y][x] = (!inside || border || pillar) ? 0 : 1;
      if (maze[y][x] == 1) totalDots++;
    }
  }

  // Power pellets: left and right middle, and top centre
  maze[mazeH / 2][1] = 3;
  maze[mazeH / 2][mazeW - 2] = 3;
  maze[1][mazeW / 2] = 3;
}

void PacManGame::updatePacMan() {
  // Try to change direction if requested
  int nextX = pacman.x + DIR_DX[pacman.nextDirection];
  int nextY = pacman.y + DIR_DY[pacman.nextDirection];
  if (isValidMove(nextX, nextY)) {
    pacman.direction = pacman.nextDirection;
  }

  // Move in the current direction
  nextX = pacman.x + DIR_DX[pacman.direction];
  nextY = pacman.y + DIR_DY[pacman.direction];
  if (!isValidMove(nextX, nextY)) return;

  pacman.x = nextX;
  pacman.y = nextY;

  uint8_t& cell = maze[pacman.y][pacman.x];
  if (cell == 1) {
    cell = 2;
    score += 10;
    dotsEaten++;
  } else if (cell == 3) {
    cell = 2;
    score += 50;
    dotsEaten++;
    frightenedUntil = millis() + LEVELS[stage].frightenedMs;
    ghostChain = 0;
    fxPop(offsetX + pacman.x * CELL_SIZE, offsetY + pacman.y * CELL_SIZE, 50);
  }
}

// A ghost steps to a neighbouring cell; it never turns straight back unless it
// has no other way. Normal ghosts head for their target (see the class comment),
// sometimes at random; frightened ghosts run away.
void PacManGame::moveGhost(int i) {
  Ghost& g = ghosts[i];
  bool frightened = millis() < frightenedUntil;

  // Where this ghost wants to go
  int tx = pacman.x, ty = pacman.y;
  bool wander = false;
  switch (i) {
    case 1:   // cut you off: three cells ahead
      tx = pacman.x + 3 * DIR_DX[pacman.direction];
      ty = pacman.y + 3 * DIR_DY[pacman.direction];
      break;
    case 2:   // wanderer
      wander = random(100) < 50;
      break;
    case 3:   // patrol: the far corner, unless you are close
      if (abs(pacman.x - g.x) + abs(pacman.y - g.y) > 4) {
        tx = mazeW - 1 - g.startX;
        ty = mazeH - 1 - g.startY;
      }
      break;
  }

  int valid[4], n = 0;
  int back = (g.direction + 2) % 4;
  for (int d = 0; d < 4; d++) {
    if (d == back) continue;
    if (isValidMove(g.x + DIR_DX[d], g.y + DIR_DY[d])) valid[n++] = d;
  }
  if (n == 0 && isValidMove(g.x + DIR_DX[back], g.y + DIR_DY[back])) valid[n++] = back;
  if (n == 0) return;

  int choice;
  if (wander || random(100) < ghostRandomPct || (frightened && random(100) < 30)) {
    choice = valid[random(0, n)];
  } else {
    // Closest to the target, or farthest from Pac-Man when frightened
    choice = valid[0];
    int bestScore = frightened ? -1 : 1000;
    for (int k = 0; k < n; k++) {
      int d = valid[k];
      int nx = g.x + DIR_DX[d], ny = g.y + DIR_DY[d];
      if (frightened) {
        int dist = abs(nx - pacman.x) + abs(ny - pacman.y);
        if (dist > bestScore) { bestScore = dist; choice = d; }
      } else {
        int dist = abs(nx - tx) + abs(ny - ty);
        if (dist < bestScore) { bestScore = dist; choice = d; }
      }
    }
  }

  g.direction = choice;
  g.x += DIR_DX[choice];
  g.y += DIR_DY[choice];
}

void PacManGame::updateGhosts() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_GHOSTS; i++) {
    if (!ghosts[i].active || now < ghosts[i].respawnAt) continue;
    moveGhost(i);
  }
}

// Pac-Man meets a ghost: a normal ghost costs a life, a frightened one is eaten
void PacManGame::checkCollisions() {
  unsigned long now = millis();
  bool frightened = now < frightenedUntil;
  for (int i = 0; i < MAX_GHOSTS; i++) {
    Ghost& g = ghosts[i];
    if (!g.active || now < g.respawnAt || g.x != pacman.x || g.y != pacman.y) continue;

    if (frightened) {
      int points = 200 << min(ghostChain, 3);
      ghostChain++;
      score += points;
      fxPop(offsetX + g.x * CELL_SIZE, offsetY + g.y * CELL_SIZE, points);
      g.x = g.startX;
      g.y = g.startY;
      g.respawnAt = now + 2500;
    } else {
      lives--;
      fxPopText(offsetX + pacman.x * CELL_SIZE, offsetY + pacman.y * CELL_SIZE, "OUCH");
      if (lives > 0) resetPositions();
      return;
    }
  }
}

bool PacManGame::isValidMove(int x, int y) {
  if (x < 0 || x >= mazeW || y < 0 || y >= mazeH) {
    return false;
  }
  return maze[y][x] != 0; // Not a wall
}

void PacManGame::drawMaze() {
  for (int y = 0; y < mazeH; y++) {
    for (int x = 0; x < mazeW; x++) {
      int screenX = offsetX + x * CELL_SIZE;
      int screenY = offsetY + y * CELL_SIZE;
      
      if (maze[y][x] == 0) { // Wall
        display.fillRect(screenX, screenY, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
      }
      else if (maze[y][x] == 1) { // Dot
        display.fillCircle(screenX + CELL_SIZE/2, screenY + CELL_SIZE/2, 1, SSD1306_WHITE);
      }
      else if (maze[y][x] == 3) { // Power pellet, blinking
        if ((millis() / 250) % 2) display.fillCircle(screenX + CELL_SIZE/2, screenY + CELL_SIZE/2, 3, SSD1306_WHITE);
      }
    }
  }
}

void PacManGame::drawPacMan() {
  int screenX = offsetX + pacman.x * CELL_SIZE + CELL_SIZE/2;
  int screenY = offsetY + pacman.y * CELL_SIZE + CELL_SIZE/2;
  
  display.fillCircle(screenX, screenY, 3, SSD1306_WHITE);
  
  // Draw mouth based on direction
  switch (pacman.direction) {
    case 0: // right
      display.drawLine(screenX, screenY, screenX + 2, screenY - 1, SSD1306_BLACK);
      display.drawLine(screenX, screenY, screenX + 2, screenY + 1, SSD1306_BLACK);
      break;
    case 1: // down
      display.drawLine(screenX, screenY, screenX - 1, screenY + 2, SSD1306_BLACK);
      display.drawLine(screenX, screenY, screenX + 1, screenY + 2, SSD1306_BLACK);
      break;
    case 2: // left
      display.drawLine(screenX, screenY, screenX - 2, screenY - 1, SSD1306_BLACK);
      display.drawLine(screenX, screenY, screenX - 2, screenY + 1, SSD1306_BLACK);
      break;
    case 3: // up
      display.drawLine(screenX, screenY, screenX - 1, screenY - 2, SSD1306_BLACK);
      display.drawLine(screenX, screenY, screenX + 1, screenY - 2, SSD1306_BLACK);
      break;
  }
}

void PacManGame::drawGhosts() {
  unsigned long now = millis();
  bool frightened = now < frightenedUntil;
  bool ending = frightened && frightenedUntil - now < 1500;

  for (int i = 0; i < MAX_GHOSTS; i++) {
    if (!ghosts[i].active || now < ghosts[i].respawnAt) continue;
    int screenX = offsetX + ghosts[i].x * CELL_SIZE + CELL_SIZE/2;
    int screenY = offsetY + ghosts[i].y * CELL_SIZE + CELL_SIZE/2;

    // Frightened ghosts are hollow; they flicker back to solid at the end
    bool hollow = frightened && !(ending && (now / 150) % 2);
    display.drawCircle(screenX, screenY - 1, 3, SSD1306_WHITE);
    if (!hollow) {
      display.fillCircle(screenX, screenY - 1, 3, SSD1306_WHITE);
      display.fillRect(screenX - 3, screenY, 7, 3, SSD1306_WHITE);
    } else {
      display.drawRect(screenX - 3, screenY, 7, 3, SSD1306_WHITE);
    }

    // Wavy bottom
    display.drawPixel(screenX - 2, screenY + 3, SSD1306_WHITE);
    display.drawPixel(screenX, screenY + 2, SSD1306_WHITE);
    display.drawPixel(screenX + 2, screenY + 3, SSD1306_WHITE);

    // Eyes (a frightened ghost shows a worried mouth instead)
    if (hollow) {
      display.drawPixel(screenX - 1, screenY - 2, SSD1306_WHITE);
      display.drawPixel(screenX + 1, screenY - 2, SSD1306_WHITE);
    } else {
      display.drawPixel(screenX - 1, screenY - 2, SSD1306_BLACK);
      display.drawPixel(screenX + 1, screenY - 2, SSD1306_BLACK);
    }
  }
}

void PacManGame::drawUI() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(score);

  // Stage in the middle, lives as little Pac-Men on the right
  char text[8];
  snprintf(text, sizeof(text), "S%d", stage - firstStage + 1);
  display.setCursor(SCREEN_WIDTH / 2 - 6, 0);
  display.print(text);
  for (int i = 0; i < lives; i++) {
    display.fillCircle(SCREEN_WIDTH - 5 - i * 9, 3, 3, SSD1306_WHITE);
    display.drawPixel(SCREEN_WIDTH - 3 - i * 9, 3, SSD1306_BLACK);
  }
}
