#ifndef DINO_H
#define DINO_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define DINO_GROUND_Y (SCREEN_HEIGHT - 6)
#define DINO_X 12
#define DINO_WIDTH 12
#define DINO_HEIGHT 14
#define DINO_DUCK_WIDTH 16
#define DINO_DUCK_HEIGHT 8
#define DINO_MAX_OBSTACLES 3

enum DinoObstacleType {
  DINO_CACTUS_SMALL,
  DINO_CACTUS_LARGE,
  DINO_CACTUS_DOUBLE,
  DINO_BIRD_LOW,    // skims the ground: jump it
  DINO_BIRD_MID     // head height: duck it (or jump)
};

struct DinoObstacle {
  float x;
  uint8_t type;
  bool active;
};

class DinoGame : public Game {
private:
  float height;      // pixels above the ground
  float velY;
  bool ducking;
  float speed;
  float scoreF;
  float groundScroll;
  float distSinceSpawn;
  float nextGap;
  bool gameOver;
  unsigned long lastUpdate;
  unsigned long animClock;
  uint8_t level = DIFFICULTY_DEFAULT;   // option: start and top speed
  bool night;                            // display inverted, every 700 points
  DinoObstacle obstacles[DINO_MAX_OBSTACLES];

  void spawnObstacle();
  void obstacleSize(const DinoObstacle& o, int& w, int& h, int& bottom);
  bool checkCollision();
  void drawDino();
  void drawObstacle(const DinoObstacle& o);

public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return gameOver; }
  int getScore() override { return (int)scoreF; }
  const char* getName() override { return "DINO"; }
  void stop() override;

  int optionCount() override { return 1; }
  const GameOption* option(int) override { return &OPT_DIFFICULTY; }
  void setOption(int, int value) override { level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern DinoGame dinoGame;

#endif
