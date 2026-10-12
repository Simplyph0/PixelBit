#ifndef CONNECT4_H
#define CONNECT4_H

#include "config.h"
#include "game.h"
#include "lobby.h"
#include "ui.h"

#define C4_COLS 7
#define C4_ROWS 6
#define C4_ROUNDS 3       // a series is three rounds; the starter alternates

// Connect 4 against the computer (five levels, a search that looks further
// ahead each level) or against a friend on a second console. The player on this
// console drops the solid discs (the host on two consoles); the other side's
// discs are rings. LEFT / RIGHT choose a column, UP or DOWN drops the disc.
class Connect4Game : public Game {
private:
  uint8_t board[C4_ROWS][C4_COLS];   // [row][col], row 0 is the top; 0 empty, 1 solid, 2 ring
  uint8_t shown[C4_ROWS][C4_COLS];   // what the screen last saw, to animate new discs
  uint8_t cursor;
  uint8_t round;
  uint8_t turn;                      // 1 or 2: who drops next
  uint8_t winCells[4][2];            // row, col of the winning four
  uint8_t roundResult;               // 0 playing, 1, 2 won, 3 draw
  uint8_t ptsX, ptsO;                // series points: win 3, draw 1
  bool seriesOver;
  unsigned long nextAt;
  bool thinkingShown;                // the computer waits one frame so "THINKING" is on screen

  int8_t animRow, animCol;           // a disc that is still falling
  unsigned long animStart;

  uint8_t mode = 0;                  // 0 = vs AI, 1 = two consoles
  uint8_t level = DIFFICULTY_DEFAULT;
  uint8_t myMark = 1;

  Lobby lobby;
  enum Phase : uint8_t { PH_LOBBY, PH_LOCAL, PH_HOST, PH_CLIENT } phase = PH_LOCAL;
  bool exitRequested = false;
  bool wantRematch = false, peerRematch = false;
  bool gotState = false;
  uint8_t pendingCol = 0xFF, pendingCount = 0;   // joiner: move being asked for

  // computer: search work space
  uint8_t work[C4_ROWS][C4_COLS];
  uint8_t filled[C4_COLS];
  unsigned long searchDeadline;
  long nodes;
  bool aborted;

  void startSeries();
  void startRound();
  bool drop(uint8_t col, uint8_t mark);
  bool findWin(const uint8_t b[C4_ROWS][C4_COLS], int row, int col, uint8_t cells[4][2]);
  uint8_t discCount();
  bool columnFree(uint8_t col) { return board[0][col] == 0; }
  uint8_t chooseAiMove();
  int search(int depth, int alpha, int beta, uint8_t mark, int ply);
  int evaluate(uint8_t mark);
  void handleInput();
  void updateLocal();
  void updateHost();
  void updateClient();
  bool linkAlive();
  void drawBoard();
  void noteNewDiscs();

public:
  void init() override;
  void update() override;
  void draw() override;
  void stop() override { lobby.end(); }
  void restart() override;
  bool wantsExit() override { return exitRequested; }
  bool isGameOver() override { return phase == PH_LOCAL && seriesOver && millis() >= nextAt; }
  bool isGameWon() override { return ptsX > ptsO; }
  int getScore() override { return ptsX * (level + 1); }
  const char* getName() override { return "CONNECT 4"; }

  int optionCount() override { return 2; }
  const GameOption* option(int i) override { return i == 0 ? &OPT_MODE_AI : &OPT_DIFFICULTY; }
  bool optionShown(int i) override { return i == 0 || mode == 0; }
  void setOption(int i, int value) override { if (i == 0) mode = value; else level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern Connect4Game connect4Game;

#endif
