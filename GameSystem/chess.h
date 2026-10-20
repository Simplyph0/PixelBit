#ifndef CHESS_H
#define CHESS_H

#include "config.h"
#include "game.h"
#include "lobby.h"
#include "ui.h"
#include "chessengine.h"

#define CHESS_MAX_PLIES 400        // a game that is still going after this many half moves is a draw

// Chess against the computer (five levels, from a beginner that plays almost at
// random to a search that thinks for five seconds) or against a friend on a
// second console. You play White first; the sides swap in every new game.
//
// With four buttons there is no cursor: LEFT / RIGHT step through the pieces
// that can move, UP picks one, LEFT / RIGHT step through where it can go, UP
// moves it, DOWN goes back. A pawn that reaches the last rank asks what it
// should become.
//
// Two consoles: both run the same rules. The host decides: it plays its own moves,
// takes the joiner's (checked against the rules) and sends every move the joiner
// has not got yet, so a lost packet is simply sent again.
//   host -> joiner: flags, game number, plies, first move sent, count, moves
//   joiner -> host: flags, game number, plies, wanted move, ply it is wanted at
class ChessGame : public Game {
private:
  ChessPos pos;
  uint16_t* history = nullptr;       // the moves played, CHESS_MAX_PLIES
  uint32_t* hashes = nullptr;        // hashes[i] is the position after i moves, so hashes[0] is the start
  uint16_t plies = 0;

  enum Stage : uint8_t { ST_PIECE, ST_DEST, ST_PROMO, ST_WAIT } stage = ST_PIECE;
  uint16_t legal[CHESS_MAX_MOVES];
  uint8_t legalCount = 0;
  uint8_t pieces[16];                // squares (0..63) of the pieces that can move, in reading order on the screen
  uint8_t pieceCount = 0, pieceIdx = 0;
  uint16_t destMove[28];             // for the chosen piece: a move to each square (a promotion shows once)
  uint8_t destCount = 0, destIdx = 0;
  uint8_t promoIdx = 0;
  uint16_t lastMove = 0;
  bool haveLast = false;

  ChessResult result = CR_PLAYING;
  bool over = false;
  int8_t winner = -1;                // colour that won (0 White, 1 Black), -1 a draw
  unsigned long nextAt = 0;
  bool thinkingShown = false;
  bool noMemory = false;

  uint8_t mode = 0;                  // 0 = against the computer, 1 = two consoles
  uint8_t level = 1;
  uint8_t pieceStyle = 0;            // 0 = little pictures, 1 = the letters P N B R Q K
  uint8_t myColor = 0;               // 0 White, 1 Black
  uint8_t gameNo = 0;                // how many games this session; the sides follow it

  Lobby lobby;
  enum Phase : uint8_t { PH_LOBBY, PH_LOCAL, PH_HOST, PH_CLIENT } phase = PH_LOCAL;
  bool exitRequested = false;
  bool wantRematch = false, peerRematch = false;
  bool gotState = false;
  uint16_t clientPly = 0;            // host: how many moves the joiner says it has
  uint16_t wantMove = 0xFFFF, wantPly = 0;   // joiner: the move it is asking for
  unsigned long wantSince = 0;

  void freeBuffers();
  bool allocBuffers();
  void newGame();
  void refreshLegal();
  void buildPieces();
  void buildDests();
  uint8_t screenKey(uint8_t sq64) const;
  bool playMove(uint16_t move);
  void checkEnd();
  uint16_t thinkAi();
  void handleInput();
  void updateLocal();
  void updateHost();
  void updateClient();
  bool linkAlive();
  bool myTurn() const { return !over && pos.side == myColor; }

  void drawBoard();
  void drawPanel();
  void drawSprite(int x, int y, uint8_t type, bool black);
  void drawSquare(int sq64, int style);
  void screenXY(uint8_t sq64, int& x, int& y) const;
  const char* resultText() const;

public:
  void init() override;
  void update() override;
  void draw() override;
  void stop() override { lobby.end(); freeBuffers(); }
  void restart() override;
  bool wantsExit() override { return exitRequested; }
  bool isGameOver() override { return phase == PH_LOCAL && over && millis() >= nextAt; }
  bool isGameWon() override { return phase == PH_LOCAL && winner == (int8_t)myColor; }
  int getScore() override;
  const char* getName() override { return "CHESS"; }

  int optionCount() override { return 3; }
  const GameOption* option(int i) override;
  bool optionShown(int i) override { return i != 1 || mode == 0; }      // the level only matters against the computer
  void setOption(int i, int value) override {
    if (i == 0) mode = value;
    else if (i == 1) level = constrain(value, 0, DIFFICULTY_LEVELS - 1);
    else pieceStyle = constrain(value, 0, 1);
  }
};

extern ChessGame chessGame;

#endif
