#ifndef CHECKERS_H
#define CHECKERS_H

#include "config.h"
#include "game.h"
#include "lobby.h"
#include "ui.h"
#include "checkersengine.h"

// Checkers against the computer (five levels, from random moves to a search that thinks
// for four seconds) or against a friend on a second console. You start at the bottom and
// move first; the sides swap in every new game.
//
// With four buttons there is no cursor: LEFT / RIGHT step through the pieces that can move
// (a capture is compulsory, so only the pieces that can jump show), UP picks one, LEFT /
// RIGHT step through where it can go, UP moves it, DOWN goes back. A piece that can jump
// again must: it is picked for you and you choose its next jump.
//
// Two consoles: both run the same rules. The host decides: it plays its own steps, takes the
// joiner's (checked against the rules) and sends the whole position every frame.
//   host -> joiner: flags, game number, steps (2), side, chain, quiet, board (12 bytes, 3 bits a dark square),
//                   the last step (2)
//   joiner -> host: flags, game number, steps it has seen (2), step it wants (2)
class CheckersGame : public Game {
private:
  CkPos pos;
  uint16_t steps = 0;                // steps played in this game; a multiple jump is several
  uint16_t legal[CK_MAX_STEPS];
  uint8_t legalCount = 0;

  enum Stage : uint8_t { ST_PIECE, ST_DEST, ST_WAIT } stage = ST_PIECE;
  uint8_t pieces[12];                // squares of the pieces that can move, in reading order on the screen
  uint8_t pieceCount = 0, pieceIdx = 0;
  uint16_t dests[4];                 // steps of the chosen piece, in reading order of where they go
  uint8_t destCount = 0, destIdx = 0;
  uint16_t lastStep = 0;

  CkResult result = CK_PLAYING;
  bool over = false;
  int8_t winner = -1;                // side that won, -1 a draw
  unsigned long nextAt = 0;
  bool thinkingShown = false;

  uint8_t mode = 0;                  // 0 = against the computer, 1 = two consoles
  uint8_t level = 1;
  uint8_t myColor = 0;               // the side on this console: 0 starts at the bottom and moves first
  uint8_t gameNo = 0;                // how many games this session; the sides follow it

  Lobby lobby;
  enum Phase : uint8_t { PH_LOBBY, PH_LOCAL, PH_HOST, PH_CLIENT } phase = PH_LOCAL;
  bool exitRequested = false;
  bool wantRematch = false, peerRematch = false;
  bool gotState = false;
  uint16_t wantStep = 0, wantAt = 0; // joiner: the step it is asking for, and the step count it was asked at
  unsigned long wantSince = 0;

  void newGame();
  void refresh();
  void sortByScreen(uint8_t* sq, int n) const;
  void buildDests();
  void unpackBoard(const uint8_t* in);
  uint8_t screenKey(uint8_t sq) const;
  bool playStep(uint16_t step);
  uint16_t thinkAi();
  uint16_t handleInput();           // the step the player chose, or 0
  void updateLocal();
  void updateHost();
  void updateClient();
  bool linkAlive();
  bool myTurn() const { return !over && pos.side == myColor; }

  void drawBoard();
  void drawPanel();
  void drawPiece(int cx, int cy, uint8_t piece);

public:
  void init() override;
  void update() override;
  void draw() override;
  void stop() override { lobby.end(); }
  void restart() override;
  bool wantsExit() override { return exitRequested; }
  bool isGameOver() override { return phase == PH_LOCAL && over && millis() >= nextAt; }
  bool isGameWon() override { return phase == PH_LOCAL && winner == (int8_t)myColor; }
  int getScore() override;
  const char* getName() override { return "CHECKERS"; }

  int optionCount() override { return 2; }
  const GameOption* option(int i) override;
  bool optionShown(int i) override { return i == 0 || mode == 0; }      // the level only matters against the computer
  void setOption(int i, int value) override {
    if (i == 0) mode = value;
    else level = constrain(value, 0, DIFFICULTY_LEVELS - 1);
  }
};

extern CheckersGame checkersGame;

#endif
