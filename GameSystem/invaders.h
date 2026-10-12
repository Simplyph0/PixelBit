#ifndef INVADERS_H
#define INVADERS_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define INV_COLS 5
#define INV_ROWS 3
#define INV_CELL_W 14
#define INV_CELL_H 9
#define INV_ALIEN_W 8
#define INV_ALIEN_H 6
#define INV_SHIP_W 9
#define INV_SHIP_H 4
#define INV_SHIP_Y 57
#define INV_MAX_ENEMY_BULLETS 3
#define INV_SHIELDS 4
#define INV_SHIELD_W 12
#define INV_SHIELD_H 6
#define INV_SHIELD_Y 46
#define INV_UFO_W 13
#define INV_UFO_H 5
#define INV_UFO_Y 11

struct InvBullet {
  float x, y;
  bool active;
};

class InvadersGame : public Game {
private:
  bool alive[INV_ROWS][INV_COLS];
  int aliveCount;
  int alienX, alienY, alienDir;
  bool animFrame;
  unsigned long lastStep;

  float shipX;
  int lives, score, wave;
  InvBullet playerBullet;
  InvBullet enemyBullets[INV_MAX_ENEMY_BULLETS];

  bool gameOver;
  unsigned long lastUpdate;
  unsigned long pauseUntil;   // freeze after a hit / between waves
  char banner[12];

  uint16_t shield[INV_SHIELDS][INV_SHIELD_H];   // one bit per pixel, bit 11 = leftmost
  bool ufoActive;
  float ufoX;
  int8_t ufoDir;
  unsigned long ufoNextAt;
  uint8_t level = DIFFICULTY_DEFAULT;           // option: lives, enemy fire rate, march speed

  void startWave();
  void stepAliens();
  void movePlayerBullet(float dt);
  void moveEnemyBullets(float dt);
  void maybeFire();
  void showBanner(const char* text, unsigned long ms);
  void resetShields();
  bool shieldHit(float x, float y);
  void moveUfo(float dt);

public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return gameOver; }
  int getScore() override { return score; }
  const char* getName() override { return "INVADERS"; }

  int optionCount() override { return 1; }
  const GameOption* option(int) override { return &OPT_DIFFICULTY; }
  void setOption(int, int value) override { level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern InvadersGame invadersGame;

#endif
