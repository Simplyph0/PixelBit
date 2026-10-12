#ifndef PACMAN_H
#define PACMAN_H

#include "config.h"
#include "game.h"
#include "ui.h"

#define MAZE_MAX_W 16
#define MAZE_MAX_H 7
#define CELL_SIZE 8
#define MAZE_TOP 8          // the row above the maze is the score line
#define MAX_GHOSTS 4

struct PacMan {
  int x, y;
  int direction; // 0=right, 1=down, 2=left, 3=up
  int nextDirection;
};

struct Ghost {
  int x, y;
  int startX, startY;
  int direction;
  bool active;
  unsigned long respawnAt;   // after being eaten
};

// Five mazes that grow from small to the full screen, with more ghosts each
// time. The option picks the maze you start in; clearing one leads to the next,
// and clearing the last wins. Three lives. Power pellets turn the ghosts blue
// for a while so they can be eaten. Each ghost hunts differently: ghost 1
// chases, 2 cuts you off, 3 wanders, 4 patrols a corner until you come close.
class PacManGame : public Game {
private:
  PacMan pacman;
  Ghost ghosts[MAX_GHOSTS];
  uint8_t maze[MAZE_MAX_H][MAZE_MAX_W];   // 0 = wall, 1 = dot, 2 = empty, 3 = power pellet
  int firstStage = DIFFICULTY_DEFAULT;     // option: the maze to start in
  int stage;                               // index into LEVELS in pacman.cpp, grows as mazes are cleared
  int mazeW, mazeH;                        // cells, including the border wall
  int offsetX, offsetY;                    // screen position of the maze
  int ghostCount;
  int lives;
  int score;
  int dotsEaten;
  int totalDots;
  int ghostChain;                          // ghosts eaten during this power-up
  bool gameOver;
  bool gameWon;
  unsigned long lastMove;
  unsigned long lastGhostMove;
  unsigned long resumeAt;                  // everything waits until then (start, after a death)
  unsigned long frightenedUntil;
  unsigned long bannerUntil;
  int moveDelay;
  int ghostDelay;
  int ghostRandomPct;

  void startStage();
  void resetPositions();
  void generateMaze();
  void updatePacMan();
  void updateGhosts();
  void moveGhost(int i);
  void checkCollisions();
  bool isValidMove(int x, int y);
  void drawMaze();
  void drawPacMan();
  void drawGhosts();
  void drawUI();
  
public:
  void init() override;
  void update() override;
  void draw() override;
  void handleInput();
  bool isGameOver() override { return gameOver; }
  bool isGameWon() override { return gameWon; }
  int getScore() override { return score; }
  const char* getName() override { return "PAC-MAN"; }
  int optionCount() override { return 1; }
  const GameOption* option(int) override { return &OPT_DIFFICULTY; }
  void setOption(int, int value) override { firstStage = constrain(value, 0, DIFFICULTY_LEVELS - 1); }
};

extern PacManGame pacmanGame;

#endif
