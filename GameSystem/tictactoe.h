#ifndef TICTACTOE_H
#define TICTACTOE_H

#include "config.h"
#include "game.h"
#include "lobby.h"
#include "ui.h"

#define TTT_ROUNDS 5      // a series is five rounds; the starter alternates

// Tic-tac-toe against the computer (five levels) or against a friend on a
// second console. The player on this console is X in a game against the
// computer and for the host; the joiner is O.
class TicTacToeGame : public Game {
private:
  uint8_t board[9];            // 0 empty, 1 X, 2 O
  uint8_t cursor;
  uint8_t round;               // 0-based
  uint8_t turn;                // 1 = X to move, 2 = O
  uint8_t winLine;             // 0..7 of the finished round, 0xFF none
  uint8_t roundResult;         // 0 playing, 1 X won, 2 O won, 3 draw
  uint8_t ptsX, ptsO;          // series points: win 3, draw 1
  bool seriesOver;
  unsigned long nextAt;        // when the AI moves / the next round starts

  uint8_t mode = 0;            // 0 = vs AI, 1 = two consoles
  uint8_t level = DIFFICULTY_DEFAULT;
  uint8_t myMark = 1;          // 1 = X, 2 = O

  Lobby lobby;
  enum Phase : uint8_t { PH_LOBBY, PH_LOCAL, PH_HOST, PH_CLIENT } phase = PH_LOCAL;
  bool exitRequested = false;
  bool wantRematch = false, peerRematch = false;
  bool gotState = false;
  uint8_t pendingCell = 0xFF, pendingCount = 0;   // joiner: move being asked for

  void startSeries();
  void startRound();
  void finishMove();
  bool place(uint8_t cell, uint8_t mark);
  uint8_t winnerOf(const uint8_t* b, uint8_t* line);
  uint8_t chooseAiMove();
  int minimax(uint8_t* b, uint8_t mark, uint8_t me, int depth);
  uint8_t moveCursor(int dir);
  void moveCursorDown();
  void handleCursorInput();
  void updateLocal();
  void updateHost();
  void updateClient();
  bool linkAlive();
  void drawBoard();

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
  const char* getName() override { return "TIC-TAC-TOE"; }

  int optionCount() override { return 2; }
  const GameOption* option(int i) override { return i == 0 ? &OPT_MODE_AI : &OPT_DIFFICULTY; }
  bool optionShown(int i) override { return i == 0 || mode == 0; }
  void setOption(int i, int value) override { if (i == 0) mode = value; else level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern TicTacToeGame ticTacToeGame;

#endif
