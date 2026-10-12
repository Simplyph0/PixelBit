#ifndef HELICOPTER_H
#define HELICOPTER_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define HELI_SIZE 4
#define CAVE_SEGMENTS 32
#define HELI_X 15

struct Helicopter {
  float y;
  float velocity;
};

struct CaveSegment {
  int topHeight;
  int bottomHeight;
  int gapHeight;
};

class HelicopterGame : public Game {
private:
  Helicopter heli;
  CaveSegment cave[CAVE_SEGMENTS];
  int score;
  bool gameOver;
  unsigned long lastUpdate;
  float gameSpeed;
  float gravity;
  float lift;
  float caveOffset;
  uint8_t level = DIFFICULTY_DEFAULT;   // option: cave width and scroll speed
  int minGap, maxGap;
  
  void generateCave();
  void updateHelicopter();
  void updateCave();
  bool checkCollisions();
  void drawHelicopter();
  void drawCave();
  void drawUI();
  
public:
  void init() override;
  void update() override;
  void draw() override;
  void handleInput();
  bool isGameOver() override { return gameOver; }
  int getScore() override { return score; }
  const char* getName() override { return "HELICOPTER"; }

  int optionCount() override { return 1; }
  const GameOption* option(int) override { return &OPT_DIFFICULTY; }
  void setOption(int, int value) override { level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern HelicopterGame helicopterGame;

#endif