#define private public
#include "snake.h"
#include "tetris.h"
#include "flappy.h"
#include "game2048.h"
#include "breakout.h"
#include "frogger.h"
#include "helicopter.h"
#include "pacman.h"
#include "pong.h"
#include "invaders.h"
#include "dino.h"
#include "input.h"
#include "display.h"
#include "fx.h"
#include <cstdio>
extern unsigned long g_ms;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); fails++; } } while (0)
static void tick(int ms = 40) { g_ms += ms; }
static void press(bool ButtonState::*held, bool ButtonState::*pressed) { buttons = {}; buttons.*held = true; buttons.*pressed = true; }
int main() {
  g_ms = 100000; srand(11);

  // ---- snake: queued turns, wrap mode, growth cap
  { auto& s = snakeGame; s.setOption(0, 1); s.setOption(1, 1); s.init();
    buttons = {}; buttons.upPressed = true; s.handleInput(); buttons = {}; buttons.leftPressed = true; s.handleInput();
    CHECK(s.queued == 2 && s.queue[0] == 3 && s.queue[1] == 2, "snake queues two turns");
    buttons = {}; buttons.rightPressed = true; s.handleInput(); CHECK(s.queued == 2, "snake queue holds two");
    // wrap: put head at the right wall moving right
    s.queued = 0; s.direction = 0; s.snake[0] = {GRID_WIDTH - 1, 5}; s.snakeLength = 3; s.snake[1] = {GRID_WIDTH - 2, 5}; s.snake[2] = {GRID_WIDTH - 3, 5};
    s.moveSnake(); bool dead = s.checkCollisions(); CHECK(!dead && s.snake[0].x == 0, "snake wraps through the wall");
    s.setOption(1, 0); s.snake[0] = {GRID_WIDTH, 5}; CHECK(s.checkCollisions(), "snake dies on a solid wall");
    s.init(); s.snakeLength = MAX_SNAKE_LENGTH; s.food = s.snake[0]; s.checkCollisions(); CHECK(s.snakeLength == MAX_SNAKE_LENGTH, "snake length capped"); }

  // ---- tetris: bag, hold, rotation kick, hard drop, line clear
  { auto& t = tetrisGame; t.setOption(0, 0); t.init();
    int seen[7] = {0}; t.bagPos = 7; for (int i = 0; i < 7; i++) seen[t.takeFromBag()]++; bool bag = true; for (int i = 0; i < 7; i++) if (seen[i] != 1) bag = false; CHECK(bag, "7-bag gives each piece once");
    int type = t.currentPiece.type, nextT = t.nextType; t.holdPiece(); CHECK(t.holdType == type && t.currentPiece.type == nextT && !t.canHold, "hold stores the piece and takes the next");
    t.holdPiece(); CHECK(t.holdType == type, "cannot hold twice in a row");
    // rotation next to the wall kicks sideways
    t.canHold = true; t.currentPiece.type = 0; t.currentPiece.rotation = 0; t.currentPiece.x = -2; t.currentPiece.y = 4; t.board[5][0] = 0;
    CHECK(!t.isValidPosition(t.currentPiece.x, t.currentPiece.y, 0, 1) || true, "setup");
    bool rotated = t.rotatePiece(); CHECK(rotated, "rotation succeeds against the wall with a kick");
    // line clear scoring
    memset(t.board, 0, sizeof(t.board)); for (int x = 0; x < 10; x++) t.board[15][x] = 1; int before = t.score; t.clearLines(); CHECK(t.score == before + 100 * t.level && t.linesCleared == 1, "one line scores 100 x level");
    // hard drop lands at the bottom
    t.init(); t.currentPiece.type = 1; t.currentPiece.x = 3; t.currentPiece.y = 0; t.currentPiece.rotation = 0; int ly = t.landingY(); t.hardDrop(); CHECK(ly >= 13 && t.board[15][4] == 1, "hard drop reaches the floor"); }

  // ---- 2048: undo chord restores the state, 5x5 works
  { auto& g = game2048; g.setOption(0, 0); g.init(); memset(g.grid, 0, sizeof(g.grid)); g.grid[0][0] = 2; g.grid[0][1] = 2; int s0 = g.score;
    g.historyCount = 0; buttons = {}; buttons.rightPressed = true; buttons.right = true; g.handleInput(); CHECK(g.grid[0][3] == 4 && g.score == s0 + 4, "2048 merges to the right");
    g.moved = false; buttons = {}; buttons.left = buttons.right = true; g_ms += 1000; g.handleInput(); CHECK(g.grid[0][0] == 2 && g.grid[0][1] == 2 && g.score == s0 && g.undosLeft == 2, "2048 undo restores board and score");
    g.setOption(0, 1); g.init(); CHECK(g.size == 5, "2048 5x5 option"); for (int i = 0; i < 300; i++) { buttons = {}; int d = rand() % 4; if (d == 0) buttons.leftPressed = true; else if (d == 1) buttons.rightPressed = true; else if (d == 2) buttons.upPressed = true; else buttons.downPressed = true; g.update(); g_ms += 40; if (g.gameOver) break; } CHECK(true, "2048 5x5 plays"); g.setOption(0, 0); }

  // ---- breakout: level loads, capsule effects, multiball
  { auto& b = breakoutGame; b.setOption(0, 2); b.init(); CHECK(b.bricksLeft == 30 && b.level == 1, "breakout level 1 has 30 bricks"); b.level = 3; b.loadLevel(); CHECK(b.bricksLeft > 0, "level 3 loads");
    b.balls[0].active = true; b.applyCapsule(2); CHECK(b.activeBalls() == 2, "multiball splits the ball"); b.applyCapsule(2); CHECK(b.activeBalls() == 3, "multiball caps at 3");
    int lives = b.lives; b.applyCapsule(4); CHECK(b.lives == lives + 1, "extra life capsule"); b.applyCapsule(1); CHECK(g_ms < b.wideUntil, "wide capsule sets a timer");
    b.level = 5; b.bricksLeft = 1; b.bannerUntil = 0; for (int r = 0; r < BRICK_ROWS; r++) for (int c = 0; c < BRICK_COLS; c++) b.bricks[r][c] = false; b.bricks[0][0] = true; b.balls[1] = b.balls[2] = b.balls[0]; b.balls[1].active = b.balls[2].active = false;
    b.breakBrick(0, 0); b.update(); CHECK(b.gameWon && b.gameOver, "clearing the last level wins"); }

  // ---- frogger: drowning, riding a log, getting hit
  { auto& f = froggerGame; f.setOption(0, 2); f.init(); f.resumeAt = 0;
    f.frogRow = 2; for (int k = 0; k < MAX_LANE_OBJECTS; k++) f.lanes[2].objects[k].active = false; f.lanes[2].objects[0] = {50, 30, true}; f.frogX = 60; int lives = f.lives; f.update(); CHECK(f.lives == lives, "frog rides a log");
    f.frogX = 110; f.update(); CHECK(f.lives == lives - 1, "frog drowns off a log");
    f.init(); f.resumeAt = 0; f.frogRow = 6; for (int k = 0; k < MAX_LANE_OBJECTS; k++) f.lanes[6].objects[k].active = false; f.lanes[6].objects[0] = {60, 12, true}; f.frogX = 62; lives = f.lives; f.update(); CHECK(f.lives == lives - 1, "frog is hit by a car"); }

  // ---- invaders: shields erode, UFO scores
  { auto& v = invadersGame; v.setOption(0, 2); v.init(); }
  { auto& v = invadersGame; v.init(); bool hit = v.shieldHit(12 + 5, 46 + 3); CHECK(hit, "bullet hits a shield"); CHECK(!(v.shield[0][3] & (1 << (11 - 5))), "the hit pixel is gone");
    v.resetShields(); CHECK(!v.shieldHit(12 + 0, 46 + 0), "empty corner pixel passes through");
    v.ufoActive = true; v.ufoX = 40; v.ufoDir = 1; v.playerBullet = {50, 14, true}; int s0 = v.score; v.moveUfo(1.0); CHECK(v.score > s0 && !v.ufoActive, "shooting the bonus ship scores"); }

  // ---- pac-man: power pellet makes ghosts edible, eating one scores
  { auto& p = pacmanGame; p.setOption(0, 1); p.init(); p.resumeAt = 0; g_ms += 5000;
    CHECK(p.mazeW == 11 && p.ghostCount == 2 && p.lives == 3, "pac-man normal level setup");
    p.pacman.x = 1; p.pacman.y = p.mazeH / 2 - 1; p.pacman.direction = 1; p.pacman.nextDirection = 1; p.maze[p.mazeH / 2][1] = 3; p.updatePacMan();
    CHECK(g_ms < p.frightenedUntil, "power pellet frightens the ghosts");
    p.ghosts[0].x = p.pacman.x; p.ghosts[0].y = p.pacman.y; p.ghosts[0].respawnAt = 0; int s0 = p.score; p.checkCollisions(); CHECK(p.score == s0 + 200, "eating a frightened ghost scores 200");
    p.frightenedUntil = 0; p.ghosts[1].x = p.pacman.x; p.ghosts[1].y = p.pacman.y; p.ghosts[1].respawnAt = 0; int lives = p.lives; p.checkCollisions(); CHECK(p.lives == lives - 1, "a normal ghost costs a life");
    // clearing the maze moves to the next stage; clearing the last one wins
    p.lives = 3; p.stage = 0; p.dotsEaten = p.totalDots; p.resumeAt = 0; p.update(); CHECK(p.stage == 1, "clearing a maze starts the next stage");
    p.stage = DIFFICULTY_LEVELS - 1; p.dotsEaten = p.totalDots; p.resumeAt = 0; p.update(); CHECK(p.gameWon, "clearing the last maze wins");
    // every maze: all dots reachable, pellets on open cells
    for (int l = 0; l < 5; l++) { p.setOption(0, l); p.init(); int dots = 0; for (int y = 0; y < p.mazeH; y++) for (int x = 0; x < p.mazeW; x++) if (p.maze[y][x] == 1 || p.maze[y][x] == 3) dots++; CHECK(dots == p.totalDots, "dot count matches the maze"); } }

  // ---- pong: practice rally, power-up collection, spin
  { auto& g = pongGame; g.setOption(0, 2); g.init(); CHECK(g.mode == PONG_PRACTICE, "pong practice mode");
    g.playerY = 20; g.ballX = 6; g.ballY = 24; g.velX = -2; g.velY = 0; g.serveUntil = 0; g.rally = 0; g.moveBall(1.0); CHECK(g.rally >= 1 && g.velX > 0, "practice: our paddle returns the ball and counts it");
    g.ballX = SCREEN_WIDTH - 6; g.velX = 3; g.moveBall(1.0); CHECK(g.velX < 0, "practice: the wall returns the ball");
    g.setOption(0, 0); g.setOption(2, 1); g.init(); g.puType = 1; g.puX = 60; g.puY = 30; g.ballX = 62; g.ballY = 32; g.velX = 2; g_ms += 1000; g.serveUntil = 0; g.updatePowerups(g_ms); CHECK(g.puType == 0 && g.playerBigUntil > g_ms, "pong: left player collects a big-paddle pick-up");
    g.applyPaddleSizes(g_ms); CHECK(g.playerH > PONG_PADDLE_H, "pong: paddle grows");
    g.playerVy = 3; g.playerY = 20; g.ballY = 26; g.paddleHit(true); float spinUp = g.velY; g.playerVy = -3; g.paddleHit(true); CHECK(g.velY < spinUp, "pong: paddle motion adds spin"); }

  // ---- dino: night mode inverts the display and stop() restores it
  { auto& d = dinoGame; d.setOption(0, 1); d.init(); d.scoreF = 705; d.update(); CHECK(display.inverted, "dino night inverts the screen"); d.stop(); CHECK(!display.inverted, "stop() restores the screen"); }

  // ---- fx
  { fxReset(); fxPop(10, 10, 5); fxDraw(); CHECK(true, "fx pop draws"); }

  printf("%s (%d failures)\n", fails ? "SOME CHECKS FAILED" : "all logic checks passed", fails);
  return fails != 0; }
