#ifndef RPS_H
#define RPS_H

#include "config.h"
#include "game.h"
#include "lobby.h"
#include "ui.h"

#define RPS_WINS 3        // first to this many round wins takes the series

// Rock-paper-scissors against a computer that learns your habits (five levels),
// or against a friend on a second console.
class RpsGame : public Game {
private:
  enum Stage : uint8_t { CHOOSE, REVEAL, OVER };
  Stage stage;
  uint8_t round;               // 0-based
  uint8_t selection;           // 0 rock, 1 paper, 2 scissors
  uint8_t myChoice, opChoice;  // 0xFF = not locked in / unknown
  uint8_t myWins, opWins;
  uint8_t result;              // of the last round: 0 draw, 1 I won, 2 they won
  unsigned long revealAt;
  bool seriesOver;

  // what the computer knows about the player
  uint8_t history[2];          // last two player moves (0xFF unknown)
  uint8_t freq[3];
  uint8_t order1[3][3];
  uint8_t order2[9][3];

  uint8_t mode = 0;
  uint8_t level = DIFFICULTY_DEFAULT;

  Lobby lobby;
  enum Phase : uint8_t { PH_LOBBY, PH_LOCAL, PH_HOST, PH_CLIENT } phase = PH_LOCAL;
  bool exitRequested = false;
  bool wantRematch = false, peerRematch = false;
  bool gotState = false;
  bool peerLocked = false;     // the other side has chosen (versus)
  uint8_t hostChoice, clientChoice;   // host: both choices as they arrive

  void startSeries();
  void startChoose();
  void learn(uint8_t playerMove);
  uint8_t aiChoice();
  void resolve(uint8_t mine, uint8_t theirs);
  void updateLocal();
  void updateHost();
  void updateClient();
  bool linkAlive();
  void handleChoiceInput();
  void drawScreen();

public:
  void init() override;
  void update() override;
  void draw() override;
  void stop() override { lobby.end(); }
  void restart() override;
  bool wantsExit() override { return exitRequested; }
  bool isGameOver() override { return phase == PH_LOCAL && stage == OVER && millis() >= revealAt; }
  bool isGameWon() override { return myWins > opWins; }
  int getScore() override { return (myWins * 10 + (myWins > opWins ? 20 : 0)) * (level + 1); }
  const char* getName() override { return "ROCK PAPER SCISSORS"; }

  int optionCount() override { return 2; }
  const GameOption* option(int i) override { return i == 0 ? &OPT_MODE_AI : &OPT_DIFFICULTY; }
  bool optionShown(int i) override { return i == 0 || mode == 0; }
  void setOption(int i, int value) override { if (i == 0) mode = value; else level = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern RpsGame rpsGame;

#endif
