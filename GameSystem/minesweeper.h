#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include "config.h"
#include "game.h"
#include "ui.h"
#include "mines.h"

// Minesweeper on a 16 x 7 field. LEFT / RIGHT move the cursor along the rows (hold to
// run), UP opens a cell (on an open number whose flags match it, UP opens the cells
// around it), DOWN puts a flag or takes it away. The first cell you open is always
// safe. The score counts the cells you opened, times the level, plus a bonus for clearing the field.
class MinesweeperGame : public Game {
private:
  MineField field;
  uint8_t cursor = 0;
  uint8_t level = DIFFICULTY_DEFAULT;
  unsigned long startedAt = 0, endedAt = 0, repeatAt = 0;
  bool over = false;

  void step(int dir);

public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return over && millis() - endedAt > 2500; }
  bool isGameWon() override { return minesWon(field); }
  int getScore() override;
  const char* getName() override { return "MINESWEEPER"; }

  int optionCount() override { return 1; }
  const GameOption* option(int i) override { (void)i; return &OPT_DIFFICULTY; }
  void setOption(int i, int value) override { (void)i; level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern MinesweeperGame minesweeperGame;

#endif
