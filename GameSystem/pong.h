#ifndef PONG_H
#define PONG_H

#include "config.h"
#include "game.h"
#include "lobby.h"
#include "ui.h"

#define PONG_PADDLE_W 3
#define PONG_PADDLE_H 14
#define PONG_PLAYER_X 2
#define PONG_AI_X (SCREEN_WIDTH - 2 - PONG_PADDLE_W)
#define PONG_BALL_SIZE 3
#define PONG_WIN_SCORE 7

// Modes (the MODE option): against the computer, two consoles, or alone
// against a wall.
enum PongMode : uint8_t { PONG_AI, PONG_VERSUS, PONG_PRACTICE };

class PongGame : public Game {
private:
  // Positions are from this console's point of view: the "player" paddle is on
  // the left and is ours, the "ai" paddle on the right is the opponent's.
  float playerY, aiY;
  float playerH, aiH;            // paddle heights (power-ups change them)
  float playerVy, aiVy;          // paddle speed in px per frame, gives the ball spin
  float ballX, ballY, velX, velY;
  float aiError;
  int playerScore, aiScore;
  int rally;                     // practice: returns so far
  bool gameOver;
  bool gameWon;
  unsigned long lastUpdate;
  unsigned long serveUntil;

  // settings
  uint8_t mode = PONG_AI;
  uint8_t level = DIFFICULTY_DEFAULT;   // AI strength, see AI_LEVELS in pong.cpp
  uint8_t powerups = 0;                 // power-ups on (the host's choice in versus)

  // power-ups: one pick-up on the field at a time
  uint8_t puType = 0;                   // 0 none, 1 big paddle for the collector, 2 small paddle for the other
  float puX = 0, puY = 0;
  unsigned long puNextAt = 0;
  unsigned long playerBigUntil = 0, aiBigUntil = 0, playerSmallUntil = 0, aiSmallUntil = 0;

  // two-console play
  Lobby lobby;
  enum Phase : uint8_t { PH_LOBBY, PH_LOCAL, PH_HOST, PH_CLIENT } phase = PH_LOCAL;
  bool exitRequested = false;
  bool matchOver = false;        // someone reached PONG_WIN_SCORE
  bool serving = false;          // as last reported by the host
  bool wantRematch = false, peerRematch = false;
  bool gotState = false;

  void serve();
  void moveBall(float dt);
  void updateAI(float dt);
  float predictBallY();
  float pickAiAim();
  void startLocalMatch();
  void resetMatch();
  bool linkAlive();
  void movePlayer(float dt);
  void updateLocal(unsigned long now, float dt);
  void updateHost(unsigned long now, float dt);
  void updateClient(unsigned long now, float dt);
  void updatePowerups(unsigned long now);
  void applyPaddleSizes(unsigned long now);
  void paddleHit(bool left);
  void drawMatch();

public:
  void init() override;
  void update() override;
  void draw() override;
  void stop() override;
  void restart() override;
  bool wantsExit() override { return exitRequested; }
  bool isGameOver() override { return phase == PH_LOCAL && gameOver; }
  bool isGameWon() override { return phase == PH_LOCAL && gameWon; }
  int getScore() override;
  const char* getName() override { return "PONG"; }

  int optionCount() override { return 3; }
  const GameOption* option(int i) override;
  bool optionShown(int i) override;
  void setOption(int i, int value) override;
};

extern PongGame pongGame;

#endif
