#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// One setting a game offers before it starts (difficulty, mode, ...). The
// manager shows one screen per option, remembers the choices per game and keeps
// them across power-offs.
struct GameOption {
  const char* name;
  uint8_t count;                 // number of values
  const char* const* values;     // their names
  uint8_t defaultValue;
  bool strength;                 // draw the strength bar (difficulty-like options)
};

// Common interface every game implements, so the game manager can drive them
// through one table instead of a switch per operation.
class Game {
public:
  virtual void init() = 0;
  virtual void update() = 0;
  // Draws the frame; the manager presents it, so do not call updateDisplay() here.
  virtual void draw() = 0;
  virtual bool isGameOver() = 0;
  virtual bool isGameWon() { return false; }
  virtual int getScore() = 0;
  // false for something that is not scored (the video player): the menu then shows no "best" line
  virtual bool hasScore() { return true; }
  virtual const char* getName() = 0;

  // Called when the player leaves the game (menu, exit combo, game over), so a
  // game can release anything it holds, such as the radio or the inverted display.
  virtual void stop() {}
  // Play again after a result screen. Defaults to a fresh init().
  virtual void restart() { init(); }
  // A game with its own menus and end screens sets this to go back to the main menu.
  virtual bool wantsExit() { return false; }

  // Settings shown before the game starts. setOption() is called for every
  // option before init(), and again when the player changes one (so optionShown()
  // can depend on earlier choices).
  virtual int optionCount() { return 0; }
  virtual const GameOption* option(int i) { (void)i; return nullptr; }
  virtual bool optionShown(int i) { (void)i; return true; }
  virtual void setOption(int i, int value) { (void)i; (void)value; }
};

#endif
