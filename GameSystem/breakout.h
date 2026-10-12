#ifndef BREAKOUT_H
#define BREAKOUT_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define PADDLE_HEIGHT 3
#define PADDLE_Y (SCREEN_HEIGHT - 8)
#define BALL_SIZE 2
#define BRICK_WIDTH 12
#define BRICK_HEIGHT 4
#define BRICK_ROWS 5
#define BRICK_COLS 10
#define BRICK_OFFSET_Y 10
#define BREAKOUT_LEVELS 5
#define MAX_BALLS 3
#define MAX_CAPSULES 3

struct Ball {
  float x, y;
  float velX, velY;
  bool active;
};

struct Capsule {
  float x, y;
  uint8_t type;   // 1 wide paddle, 2 multiball, 3 slow ball, 4 extra life; 0 = free
};

// Five levels of bricks. Some bricks drop a capsule when broken; catch it with
// the paddle: W wide paddle, M multiball, S slow ball, + extra life.
class BreakoutGame : public Game {
private:
  Ball balls[MAX_BALLS];
  Capsule capsules[MAX_CAPSULES];
  float paddleX;
  int paddleW;
  bool bricks[BRICK_ROWS][BRICK_COLS];
  int bricksLeft;
  int score;
  int lives;
  int level;                     // 1..BREAKOUT_LEVELS
  bool gameOver;
  bool gameWon;
  unsigned long bannerUntil;     // "LEVEL n" pause
  unsigned long wideUntil, slowUntil;
  float baseSpeed;
  uint8_t difficulty = DIFFICULTY_DEFAULT;   // option: paddle size, lives, ball speed

  void loadLevel();
  void launchBall(int index);
  void updateBalls();
  void updateCapsules();
  void breakBrick(int row, int col);
  bool collideBrick(Ball& b);
  void spawnCapsule(float x, float y);
  void applyCapsule(uint8_t type);
  int activeBalls();
  void drawBricks();
  void drawUI();

public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return gameOver; }
  bool isGameWon() override { return gameWon; }
  int getScore() override { return score; }
  const char* getName() override { return "BREAKOUT"; }

  int optionCount() override { return 1; }
  const GameOption* option(int) override { return &OPT_DIFFICULTY; }
  void setOption(int, int value) override { difficulty = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern BreakoutGame breakoutGame;

#endif
