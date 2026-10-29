#include "gamemanager.h"
#include <ESP8266WiFi.h>   // ESP.restart()
#include "display.h"
#include "input.h"
#include "menuicons.h"
#include "ui.h"
#include "fx.h"
#include "settings.h"
#include "tutorial.h"
#include "welcome.h"
#include "testmode.h"
#include "settingsmenu.h"

GameManager gameManager;

// Indexed by GameID (see config.h)
Game* const GameManager::games[MAX_GAMES] = {
  &snakeGame, &tetrisGame, &flappyGame, &game2048, &breakoutGame, &froggerGame,
  &helicopterGame, &pacmanGame, &pongGame, &invadersGame, &dinoGame, &ticTacToeGame, &rpsGame, &connect4Game,
  &chessGame, &minesweeperGame, &checkersGame, &videoGame, &chatMenu, &settingsMenu
};

void initGameManager() {
  gameManager.init();
}

void updateGameManager() {
  gameManager.update();
}

void GameManager::showIntroNextBoot() {
  settingFlags &= ~SETTING_TUTORIAL_DONE;
  saveGameOptions(optionValue, optionsSeen, settingFlags);
}

void GameManager::factoryReset() {
  highscoreManager.resetAllHighscores();
  resetGameOptions();
  settingFlags = 0;                       // the tour is new again
  saveGameOptions(optionValue, optionsSeen, settingFlags);
}

bool GameManager::radioBlocked() {
  if (currentState != STATE_PLAYING) return false;
  return (currentGame == GAME_VIDEO && videoGame.radioBusy()) || (currentGame == GAME_SETTINGS && settingsMenu.radioBusy());
}

// What chat can name in "LET'S PLAY ...": the games that keep a score (the video player, chat and settings do not)
static const char* playableName(int i) {
  Game* g = GameManager::gameAt(i);
  return (g && g->hasScore()) ? g->getName() : nullptr;
}

void GameManager::resetGameOptions() {
  for (int g = 0; g < MAX_GAMES; g++) {
    for (int o = 0; o < MAX_GAME_OPTIONS; o++) {
      const GameOption* opt = o < games[g]->optionCount() ? games[g]->option(o) : nullptr;
      optionValue[g][o] = opt ? opt->defaultValue : 0;
    }
  }
  optionsSeen = 0;
  saveGameOptions(optionValue, optionsSeen, settingFlags);
}

bool GameManager::keepsScreenOn() {
  if (currentState == STATE_PLAYING) return games[currentGame]->keepsScreenOn();
  return currentState == STATE_TEST;
}

void GameManager::init() {
  chatSetGameNames(MAX_GAMES, playableName);
  initHighscores();   // starts the EEPROM; without it nothing is read or saved
  currentState = STATE_MENU;
  currentGame = 0;
  menuSelection = 0;
  optIndex = 0;
  for (int g = 0; g < MAX_GAMES; g++) {
    for (int o = 0; o < MAX_GAME_OPTIONS; o++) {
      const GameOption* opt = o < games[g]->optionCount() ? games[g]->option(o) : nullptr;
      optionValue[g][o] = opt ? opt->defaultValue : 0;
    }
  }
  optionsSeen = 0;
  settingFlags = 0;
  loadGameOptions(optionValue, &optionsSeen, &settingFlags);
  // A game whose settings screens were never shown has its defaults. The saved rows of games added
  // since the block was written (or shifted by new tiles) hold whatever the tile that used to be
  // there had, so they must not be taken as a choice.
  for (int g = 0; g < MAX_GAMES; g++) {
    if ((optionsSeen >> g) & 1) continue;
    for (int o = 0; o < MAX_GAME_OPTIONS; o++) {
      const GameOption* opt = o < games[g]->optionCount() ? games[g]->option(o) : nullptr;
      optionValue[g][o] = opt ? opt->defaultValue : 0;
    }
  }
  menuNotice = false;
  leftTaps = 0;
  lastLeftTap = testUpSince = 0;
  downSince = 0;
  downLongDone = false;
  menuTarget = 0;
  menuPos = 0;
  menuRepeatAt = 0;
  newHighscore = false;
  exitHeldSince = 0;

  if (settingFlags & SETTING_TUTORIAL_DONE) {
    showMenu();
  } else {
    // First boot: greeting, then the tour. Only once the tour is acknowledged
    // (finished, or skipped and the message closed) is that saved, so a unit that
    // is switched off in between, or put into test mode, greets its owner later.
    welcome.begin();
    setState(STATE_WELCOME);
  }
}

void GameManager::acknowledgeTour() {
  if (settingFlags & SETTING_TUTORIAL_DONE) return;
  settingFlags |= SETTING_TUTORIAL_DONE;
  saveGameOptions(optionValue, optionsSeen, settingFlags);
}

// Manufacturing test mode: LEFT three times (each within 3 s of the last), then
// hold UP. Watched on the greeting and the tour. Nothing is saved by it.
bool GameManager::checkTestEntry() {
  unsigned long now = millis();
  if (buttons.leftPressed) {
    if (now - lastLeftTap > 3000) leftTaps = 0;
    leftTaps++;
    lastLeftTap = now;
  }
  if (leftTaps >= 3 && now - lastLeftTap < 8000 && buttons.up) {
    if (!testUpSince) testUpSince = now;
    if (now - testUpSince >= TEST_HOLD_MS) {
      leftTaps = 0;
      testUpSince = 0;
      testMode.begin();
      setState(STATE_TEST);
      return true;
    }
  } else {
    testUpSince = 0;
  }
  return false;
}

void GameManager::startTutorial() {
  tutorial.begin();
  setState(STATE_TUTORIAL);
}

void GameManager::skipTutorialQuietly() {
  settingFlags |= SETTING_TUTORIAL_DONE;
  if (currentState == STATE_TUTORIAL || currentState == STATE_WELCOME) setState(STATE_MENU);
}

void GameManager::update() {
  switch (currentState) {
    case STATE_MENU:
      handleMenuInput();
      break;
      
    case STATE_PLAYING:
      // Exit combo: hold UP + DOWN together. No game needs that pair held
      // for a second, and a single press of either is unaffected.
      if (exitComboHeld()) {
        setState(STATE_MENU);
        break;
      }

      if (currentGame >= 0 && currentGame < MAX_GAMES) {
        Game* g = games[currentGame];
        g->update();
        g->draw();
        if (!g->skipPresent()) {
          fxDraw();
          updateDisplay();
        }
        if (g->wantsExit()) {
          setState(STATE_MENU);
        } else if (g->isGameOver()) {
          fxShutter();
          setState(g->isGameWon() ? STATE_GAME_WON : STATE_GAME_OVER);
        }
      }
      break;

    case STATE_TEST:
      if (testMode.update()) {
        ESP.restart();   // back to a clean boot: the unit still greets its owner
      } else {
        if (testMode.draw()) updateDisplay();
      }
      break;

    case STATE_WELCOME:
      if (checkTestEntry()) break;
      if (welcome.update()) {
        startTutorial();
      } else {
        welcome.draw();
        updateDisplay();
      }
      break;

    case STATE_TUTORIAL: {
      if (checkTestEntry()) break;
      TutorialResult r = tutorial.update();
      if (r != TUT_RUNNING) {
        // Back to the menu; a skipped tour leaves a message that stays until UP is pressed
        menuNotice = (r == TUT_SKIPPED);
        if (r == TUT_FINISHED) acknowledgeTour();   // finishing it counts; skipping waits for the message
        setState(STATE_MENU);
      } else {
        tutorial.draw();
        updateDisplay();
      }
      break;
    }

    case STATE_OPTIONS:
      handleOptionsInput();
      break;

    case STATE_GAME_OVER:
    case STATE_GAME_WON:
      handleResultInput();
      break;
  }
}

void GameManager::setState(GameState newState) {
  if (currentState == STATE_PLAYING && newState != STATE_PLAYING &&
      currentGame >= 0 && currentGame < MAX_GAMES) {
    games[currentGame]->stop();
  }
  currentState = newState;
  exitHeldSince = 0;
  
  switch (newState) {
    case STATE_MENU:
      menuTarget = menuSelection;   // snap, no slide when coming back
      menuPos = menuTarget;
      menuRepeatAt = millis() + MENU_REPEAT_DELAY_MS;   // a button still held from the game must not scroll
      downSince = 0;
      downLongDone = false;
      showMenu();
      break;
    case STATE_WELCOME:
    case STATE_TUTORIAL:
    case STATE_TEST:
      break;   // drawn every frame by their own code
    case STATE_OPTIONS:
      showOptions();
      break;
    case STATE_GAME_OVER:
      showResult(false);
      break;
    case STATE_GAME_WON:
      showResult(true);
      break;
  }
}

void GameManager::setGame(int gameId) {
  if (gameId < 0 || gameId >= MAX_GAMES) return;
  LOG("game: launching %d (%s)\n", gameId, games[gameId]->getName());
  currentGame = gameId;
  menuSelection = gameId;
  
  fxReset();
  games[gameId]->init();

  // Reveal: the first frame appears from the middle outwards
  for (int step = 1; step <= 6; step++) {
    games[gameId]->draw();
    int bar = (6 - step) * 5;
    display.fillRect(0, 0, SCREEN_WIDTH, bar, SSD1306_BLACK);
    display.fillRect(0, SCREEN_HEIGHT - bar, SCREEN_WIDTH, bar, SSD1306_BLACK);
    updateDisplay();
    delay(22);
  }

  setState(STATE_PLAYING);
}

// Carousel menu: icons slide past a fixed selection frame. menuPos is the
// (fractional) index currently centred; it eases towards menuTarget, which
// counts presses without wrapping so the strip loops smoothly in both directions.
void GameManager::showMenu() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header: title left, position right
  display.setCursor(0, 0);
  display.print(F("GAMES"));
  char text[24];
  snprintf(text, sizeof(text), "%d/%d", menuSelection + 1, MAX_GAMES);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(text), 0);
  display.print(text);

  // Icons: the centred one is magnified, the outer ones are dimmed
  for (int i = 0; i < MAX_GAMES; i++) {
    float d = i - menuPos;
    d -= MAX_GAMES * floorf(d / MAX_GAMES + 0.5f);   // shortest way round the loop
    float dist = fabsf(d);
    if (dist > 2.6f) continue;

    int scale = dist < 0.5f ? 2 : 1;
    int size = 16 * scale;
    int x = SCREEN_WIDTH / 2 + (int)(d * 34) - size / 2;
    int y = 28 - size / 2;
    if (scale == 2 && menuPos == menuTarget) y += ((millis() / 400) % 2) ? 1 : 0;   // the selected icon bobs when idle
    drawGameIcon(i, x, y, scale, dist >= 1.5f);
  }

  // Selection frame (corner brackets)
  // the brackets breathe when the strip is at rest
  int pulse = (menuPos == menuTarget && (millis() / 500) % 2) ? 1 : 0;
  const int L = 45 - pulse, R = 82 + pulse, T = 9 - pulse, B = 46 + pulse, C = 5;
  display.drawFastHLine(L, T, C, SSD1306_WHITE);  display.drawFastVLine(L, T, C, SSD1306_WHITE);
  display.drawFastHLine(R - C + 1, T, C, SSD1306_WHITE);  display.drawFastVLine(R, T, C, SSD1306_WHITE);
  display.drawFastHLine(L, B, C, SSD1306_WHITE);  display.drawFastVLine(L, B - C + 1, C, SSD1306_WHITE);
  display.drawFastHLine(R - C + 1, B, C, SSD1306_WHITE);  display.drawFastVLine(R, B - C + 1, C, SSD1306_WHITE);

  // Name and best score of the selected game
  drawCenteredText(games[menuSelection]->getName(), 49, 1);
  int best = getGameHighscore(menuSelection);
  if (!games[menuSelection]->hasScore()) snprintf(text, sizeof(text), "%s", games[menuSelection]->menuNote() ? games[menuSelection]->menuNote() : "NO SOUND");
  else if (best > 0) snprintf(text, sizeof(text), "BEST %d", best);
  else               snprintf(text, sizeof(text), "NO SCORE");
  drawCenteredText(text, 57, 1);

  // Games with settings: DOWN opens them
  if (games[menuSelection]->optionCount() > 0) {
    display.setCursor(SCREEN_WIDTH - 24, 57);
    display.print(F("vSET"));
  }

  // The message left behind by a skipped tour
  if (menuNotice) {
    display.fillRect(5, 8, 118, 50, SSD1306_BLACK);
    display.drawRect(5, 8, 118, 50, SSD1306_WHITE);
    drawCenteredText("TOUR SKIPPED", 12, 1);
    drawCenteredText("To see it again:", 22, 1);
    drawCenteredText("hold DOWN on menu", 31, 1);
    drawCenteredText("for 1 second.", 40, 1);
    display.fillRect(7, 49, 114, 8, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor((SCREEN_WIDTH - (17 * 6 - 1)) / 2, 50);
    display.print(F("PRESS UP TO CLOSE"));
    display.setTextColor(SSD1306_WHITE);
  }

  updateDisplay();
}

void GameManager::moveMenu(int step) {
  menuTarget += step;
  menuSelection = ((menuTarget % MAX_GAMES) + MAX_GAMES) % MAX_GAMES;
}

void GameManager::handleMenuInput() {
  // The "tour skipped" message: nothing else works until UP closes it
  if (menuNotice) {
    if (buttons.upPressed) {
      menuNotice = false;
      acknowledgeTour();   // closing the "tour skipped" message acknowledges the tour
      showMenu();
    }
    return;
  }

  // LEFT / RIGHT scroll, and keep scrolling while held
  unsigned long now = millis();
  int step = 0;
  if (buttons.leftPressed || buttons.rightPressed) {
    step = buttons.rightPressed ? 1 : -1;
    menuRepeatAt = now + MENU_REPEAT_DELAY_MS;
  } else if ((buttons.left != buttons.right) && now >= menuRepeatAt) {
    step = buttons.right ? 1 : -1;
    menuRepeatAt = now + MENU_REPEAT_MS;
  }
  if (step) moveMenu(step);

  if (buttons.upPressed) {
    openGame(menuSelection);
    return;
  }

  // DOWN: a short press changes this game's settings, holding it replays the tour
  if (buttons.downPressed) {
    downSince = now;
    downLongDone = false;
  }
  if (downSince) {
    if (buttons.down) {
      if (!downLongDone && now - downSince >= TOUR_HOLD_MS) {
        downLongDone = true;
        downSince = 0;
        startTutorial();
        return;
      }
    } else {
      bool wasShort = !downLongDone;
      downSince = 0;
      if (wasShort && games[menuSelection]->optionCount() > 0) {
        openGame(menuSelection, true);
        return;
      }
    }
  }

  // Ease the strip towards the selection; at rest, redraw now and then for the idle animation
  static unsigned long lastIdleDraw = 0;
  if (menuPos != menuTarget) {
    menuPos += (menuTarget - menuPos) * 0.5f;
    if (fabsf(menuTarget - menuPos) < 0.03f) menuPos = menuTarget;
    showMenu();
    lastIdleDraw = millis();
  } else if (millis() - lastIdleDraw >= 250) {
    showMenu();
    lastIdleDraw = millis();
  }
}

// The next option to show after (dir = 1) or before (dir = -1) index `from`,
// skipping options the game hides given the earlier choices; -1 if none.
int GameManager::nextShownOption(int from, int dir) {
  Game* g = games[currentGame];
  for (int i = from + dir; i >= 0 && i < g->optionCount(); i += dir) {
    if (g->optionShown(i)) return i;
  }
  return -1;
}

// Games with settings show their screens the first time (and whenever the
// player asks for them with DOWN on the menu); after that the game starts at
// once with the remembered choices.
void GameManager::openGame(int gameId, bool forceSettings) {
  if (gameId < 0 || gameId >= MAX_GAMES) return;
  currentGame = gameId;
  menuSelection = gameId;

  Game* g = games[gameId];
  for (int o = 0; o < g->optionCount() && o < MAX_GAME_OPTIONS; o++) {
    const GameOption* opt = g->option(o);
    if (optionValue[gameId][o] >= opt->count) optionValue[gameId][o] = opt->defaultValue;
    g->setOption(o, optionValue[gameId][o]);
  }

  optIndex = nextShownOption(-1, 1);
  if (optIndex < 0 || (!forceSettings && ((optionsSeen >> gameId) & 1))) {
    setGame(gameId);
    return;
  }
  setState(STATE_OPTIONS);
}

void GameManager::showOptions() {
  Game* g = games[currentGame];
  bool last = nextShownOption(optIndex, 1) < 0;
  drawOptionScreen(g->getName(), *g->option(optIndex), optionValue[currentGame][optIndex],
                   last ? "DOWN:BACK  UP:PLAY" : "DOWN:BACK  UP:NEXT");
  updateDisplay();
}

void GameManager::handleOptionsInput() {
  Game* g = games[currentGame];
  const GameOption* opt = g->option(optIndex);
  uint8_t& value = optionValue[currentGame][optIndex];

  int before = value;
  if (buttons.leftPressed && value > 0) value--;
  if (buttons.rightPressed && value < opt->count - 1) value++;
  if (value != before) {
    g->setOption(optIndex, value);
    showOptions();
  }

  if (buttons.upPressed) {
    int next = nextShownOption(optIndex, 1);
    if (next >= 0) {
      optIndex = next;
      showOptions();
    } else {
      optionsSeen |= 1UL << currentGame;
      saveGameOptions(optionValue, optionsSeen, settingFlags);
      setGame(currentGame);
    }
  }
  else if (buttons.downPressed) {
    int prev = nextShownOption(optIndex, -1);
    if (prev >= 0) {
      optIndex = prev;
      showOptions();
    } else {
      setState(STATE_MENU);
    }
  }
}

int GameManager::getCurrentScore() {
  return (currentGame >= 0 && currentGame < MAX_GAMES) ? games[currentGame]->getScore() : 0;
}

// True once UP + DOWN have been held together for EXIT_HOLD_MS.
bool GameManager::exitComboHeld() {
  if (!(buttons.up && buttons.down)) {
    exitHeldSince = 0;
    return false;
  }
  if (exitHeldSince == 0) exitHeldSince = millis();
  return millis() - exitHeldSince >= EXIT_HOLD_MS;
}

// One results screen for both outcomes; only the title and restart label differ.
void GameManager::showResult(bool won) {
  clearDisplay();

  drawCenteredText(won ? "YOU WON!" : "GAME OVER", 0, 2);

  int score = getCurrentScore();
  newHighscore = checkNewHighscore(currentGame, score);
  if (newHighscore) {
    saveGameHighscore(currentGame, score);
  }

  char text[24];
  snprintf(text, sizeof(text), "Score: %d", score);
  drawCenteredText(text, 17, 1);
  snprintf(text, sizeof(text), "Best: %d", getGameHighscore(currentGame));
  drawCenteredText(text, 27, 1);

  if (newHighscore) {
    drawCenteredText("NEW HIGHSCORE!", 37, 1);
  }

  drawCenteredText("UP: Menu", 47, 1);
  drawCenteredText(won ? "RIGHT: Play Again" : "RIGHT: Restart", 57, 1);

  updateDisplay();
}

void GameManager::handleResultInput() {
  if (buttons.upPressed) {
    setState(STATE_MENU);
  }
  else if (buttons.rightPressed) {
    games[currentGame]->restart();
    setState(STATE_PLAYING);
  }
}
