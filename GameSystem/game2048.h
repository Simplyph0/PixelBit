#ifndef GAME2048_H
#define GAME2048_H

#include "config.h"
#include "game.h"

#define MAX_GRID_2048 5

// Slide with the four buttons. LEFT + RIGHT together undo the last move (three
// undos per game). The BOARD option picks 4x4 or 5x5; on 5x5 the tiles show the
// power of two (3 = 8) so they fit.
class Game2048 : public Game {
private:
  struct Snapshot {
    int grid[MAX_GRID_2048][MAX_GRID_2048];
    int score;
  };

  int grid[MAX_GRID_2048][MAX_GRID_2048];
  int size = 4;                // option
  int score;
  bool gameOver;
  bool hasWon;
  bool moved;
  Snapshot history[4];         // positions before the latest moves
  uint8_t historyCount;
  uint8_t undosLeft;
  unsigned long lastMoveAt;
  bool chordLatched;

  void addRandomTile();
  bool canMove();
  void slide(int dir);
  int* cell(int dir, int i, int j);
  bool mergeLine(int line[], int size);
  void remember();
  void undo();
  void drawTile(int x, int y, int value);
  
public:
  void init() override;
  void update() override;
  void draw() override;
  void handleInput();
  bool isGameOver() override { return gameOver; }
  bool isGameWon() override { return hasWon; }
  int getScore() override { return score; }
  const char* getName() override { return "2048"; }

  int optionCount() override { return 1; }
  const GameOption* option(int) override;
  void setOption(int, int value) override { size = value == 1 ? 5 : 4; }
};

extern Game2048 game2048;

#endif
