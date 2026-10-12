#ifndef FLAPPY_H
#define FLAPPY_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define BIRD_SIZE 3
#define PIPE_WIDTH 8
#define PIPE_GAP_DEFAULT 20
#define PIPE_SPACING 96   // px between the left edges of two pipes
#define MAX_PIPES 3

struct Bird {
  float x, y;
  float velocity;
};

struct Pipe {
  float x;
  int gapY;
  bool passed;
  bool active;
};

class FlappyGame : public Game {
private:
  Bird bird;
  Pipe pipes[MAX_PIPES];
  int score;
  bool gameOver;
  uint8_t level = DIFFICULTY_DEFAULT;   // option: gap size and speed
  int pipeGap = PIPE_GAP_DEFAULT;
  float maxSpeed;
  float gravity;
  float jumpStrength;
  float gameSpeed;
  
  void spawnPipe();
  void updateBird();
  void updatePipes();
  bool checkCollisions();
  void drawBird();
  void drawPipes();
  void drawGround();
  
public:
  void init() override;
  void update() override;
  void draw() override;
  void handleInput();
  bool isGameOver() override { return gameOver; }
  int getScore() override { return score; }
  const char* getName() override { return "FLAPPY BIRD"; }

  int optionCount() override { return 1; }
  const GameOption* option(int) override { return &OPT_DIFFICULTY; }
  void setOption(int, int value) override { level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern FlappyGame flappyGame;

#endif
