#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H

#include "config.h"
#include "game.h"
#include "snake.h"
#include "tetris.h"
#include "flappy.h"
#include "game2048.h"
#include "breakout.h"
#include "frogger.h"
#include "helicopter.h"
#include "pacman.h"
#include "pong.h"
#include "invaders.h"
#include "dino.h"
#include "tictactoe.h"
#include "rps.h"
#include "connect4.h"
#include "videoplayer.h"
#include "tutorial.h"
#include "welcome.h"
#include "testmode.h"
#include "savesync.h"
#include "highscore.h"

class GameManager {
private:
  GameState currentState;
  int currentGame;
  int menuSelection;
  float menuPos;      // fractional carousel index currently centred
  unsigned long menuRepeatAt;   // when a held LEFT/RIGHT scrolls next
  int optIndex;                            // option being shown on the options screens
  uint8_t optionValue[MAX_GAMES][MAX_GAME_OPTIONS];   // remembered per game, saved in EEPROM
  uint8_t settingFlags;                    // SETTING_* bits saved with the options
  uint8_t leftTaps;                        // LEFT presses towards the test-mode entry
  unsigned long lastLeftTap, testUpSince;
  bool menuNotice;                         // the "tour skipped" message is showing
  unsigned long downSince;                 // DOWN pressed on the menu since then (0 = not)
  bool downLongDone;                       // the long press already started the tour
  uint16_t optionsSeen;                    // bit n: game n's settings screens were shown once
  int nextShownOption(int from, int dir);
  int menuTarget;     // unwrapped index the carousel is sliding to
  static Game* const games[MAX_GAMES];
  bool newHighscore; // Flag for new highscore
  unsigned long exitHeldSince; // 0 = exit combo not held
  uint8_t secretHist[SECRET_LEN];   // last presses on the menu: 0=L 1=U 2=D 3=R
  int secretCount;
  unsigned long secretLast, secretArmedUntil, secretHoldSince;
  int getCurrentScore();
  bool exitComboHeld();
  bool syncStopRequested();
  unsigned long syncLastUpTap;
  bool trackSecret();

public:
  void init();
  void startTutorial();
  void acknowledgeTour();       // saves "the first-boot tour was seen"
  bool checkTestEntry();        // LEFT x3 then hold UP: manufacturing test mode
  void skipTutorialQuietly();   // for the emulator, which has no saved first-boot flag
  void update();
  void setState(GameState newState);
  void setGame(int gameId);
  void showMenu();
  void openGame(int gameId, bool forceSettings = false);
  void showOptions();
  void handleOptionsInput();
  void handleMenuInput();
  void moveMenu(int step);
  void showResult(bool won);
  void handleResultInput();
  int getCurrentGame() { return currentGame; }
  GameState getState() { return currentState; }
};

extern GameManager gameManager;

void initGameManager();
void updateGameManager();

#endif