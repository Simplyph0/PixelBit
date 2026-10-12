#ifndef FROGGER_H
#define FROGGER_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define FROG_W 5
#define FROG_H 5
#define ROW_H 7                 // nine rows of 7 px fill the screen
#define ROWS 9                  // 0 goal, 1-3 river, 4 median, 5-7 road, 8 start
#define MAX_LANE_OBJECTS 3

struct LaneObject {
  float x;
  int8_t w;
  bool active;
};

struct Lane {
  int8_t dir;                   // +1 right, -1 left
  float speed;                  // px per frame at level 1
  uint8_t minGap, maxGap;       // gap between objects, px
  uint8_t minW, maxW;           // object width, px
  LaneObject objects[MAX_LANE_OBJECTS];
  float sinceSpawn;             // px travelled since the last object entered
  float nextGap;
};

// Cross three lanes of traffic and three of river (hop on the logs) to reach
// the far bank. Each crossing makes everything faster. Three lives.
class FroggerGame : public Game {
private:
  float frogX;
  int frogRow;                  // 0 .. ROWS - 1
  Lane lanes[ROWS];             // rows 1-3 river, 5-7 road; others unused
  int score;
  int lives;
  int level;
  int highestRow;               // best row reached since the last respawn
  bool gameOver;
  unsigned long resumeAt;       // short freeze after a death or a crossing
  uint8_t difficulty = DIFFICULTY_DEFAULT;   // option: base speed and traffic density

  void resetFrog();
  void setupLanes();
  void updateLanes();
  void spawn(Lane& lane);
  int gapFor(int row);
  bool onRiver();
  bool hitByCar();
  void die();
  void drawLanes();
  void drawFrog();
  void drawUI();
  
public:
  void init() override;
  void update() override;
  void draw() override;
  void handleInput();
  bool isGameOver() override { return gameOver; }
  int getScore() override { return score; }
  const char* getName() override { return "FROGGER"; }

  int optionCount() override { return 1; }
  const GameOption* option(int) override { return &OPT_DIFFICULTY; }
  void setOption(int, int value) override { difficulty = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern FroggerGame froggerGame;

#endif
