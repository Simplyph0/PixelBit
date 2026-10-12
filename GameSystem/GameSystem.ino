// GameSystem.ino - Hauptdatei
#include "config.h"
#include "display.h"
#include "input.h"
#include "gamemanager.h"
#include "savesync.h"
#include "highscore.h"
#include "settings.h"

void setup() {
  Serial.begin(115200);
  initDisplay();
  initInput();
  initHighscores();                    // starts the EEPROM: scores and settings are saved there
  if (!isFirstBoot()) showSplash();    // the first boot has its own greeting
  syncInit();
  initGameManager();
}

void loop() {
  updateInput();
  updateGameManager();
  delay(16); // ~60 FPS
}