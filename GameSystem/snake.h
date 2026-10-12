#ifndef SNAKE_H
#define SNAKE_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define GRID_SIZE 4
#define GRID_WIDTH (SCREEN_WIDTH / GRID_SIZE)
#define GRID_HEIGHT (SCREEN_HEIGHT / GRID_SIZE)
#define MAX_SNAKE_LENGTH 200

struct Point {
  int8_t x, y;   // grid cells (-1 .. GRID_WIDTH while crossing a wall)
};

class SnakeGame : public Game {
private:
  Point snake[MAX_SNAKE_LENGTH];
  int snakeLength;
  Point food;
  int direction;                 // 0 right, 1 down, 2 left, 3 up
  int8_t queue[2];               // turns waiting for the next steps
  uint8_t queued;
  int score;
  unsigned long lastMoveTime;
  int gameSpeed;                 // ms per step
  int minSpeed;
  bool gameOver;
  uint8_t level = DIFFICULTY_DEFAULT;   // start speed and how fast it ramps
  uint8_t wrap = 0;                     // 1 = the snake comes out of the opposite wall

  void generateFood();
  void moveSnake();
  bool checkCollisions();
  void handleInput();

public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return gameOver; }
  int getScore() override { return score; }
  const char* getName() override { return "SNAKE"; }

  int optionCount() override { return 2; }
  const GameOption* option(int i) override;
  void setOption(int i, int value) override { if (i == 0) level = constrain(value, 0, DIFFICULTY_LEVELS - 1); else wrap = value; }
};

extern SnakeGame snakeGame;

#endif
