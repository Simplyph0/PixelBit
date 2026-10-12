#ifndef TETRIS_H
#define TETRIS_H

#include "config.h"
#include "game.h"

#define TETRIS_WIDTH 10
#define TETRIS_HEIGHT 16
#define BLOCK_SIZE 3
#define TETRIS_FIELD_WIDTH (TETRIS_WIDTH * BLOCK_SIZE)
#define TETRIS_FIELD_HEIGHT (TETRIS_HEIGHT * BLOCK_SIZE)
#define TETRIS_OFFSET_X 20
#define TETRIS_OFFSET_Y ((SCREEN_HEIGHT - TETRIS_FIELD_HEIGHT) / 2)

struct TetrisPiece {
  int x, y;
  int type;
  int rotation;
};

// Controls: LEFT / RIGHT move (hold to repeat), UP rotates, DOWN soft-drops
// (double-tap for a hard drop), LEFT + RIGHT together swap in the held piece.
class TetrisGame : public Game {
private:
  uint8_t board[TETRIS_HEIGHT][TETRIS_WIDTH];
  TetrisPiece currentPiece;
  uint8_t bag[7];
  uint8_t bagPos;
  int8_t nextType;
  int8_t holdType;
  bool canHold;
  unsigned long lastDropTime;
  unsigned long dasAt;           // when a held LEFT / RIGHT repeats next
  unsigned long lastDownTap;
  bool chordLatched;
  int dropSpeed;
  int score;
  int level;
  int linesCleared;
  bool gameOver;
  uint8_t startLevel = 0;        // option: 0..4 -> level 1, 3, 5, 7, 9

  uint8_t takeFromBag();
  void spawnNewPiece(int type);
  bool movePiece(int dx, int dy, int dr);
  bool rotatePiece();
  bool isValidPosition(int x, int y, int type, int rotation);
  int landingY();
  void lockPiece();
  void clearLines();
  void holdPiece();
  void hardDrop();
  void handleInput();
  void drawPreview(int type, int x, int y);

public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return gameOver; }
  int getScore() override { return score; }
  const char* getName() override { return "TETRIS"; }

  int optionCount() override { return 1; }
  const GameOption* option(int) override;
  void setOption(int, int value) override { startLevel = value; }
};

extern TetrisGame tetrisGame;

#endif
