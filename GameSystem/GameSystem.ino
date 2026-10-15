// GameSystem.ino - the main sketch file
#include "config.h"
#include "display.h"
#include "input.h"
#include "gamemanager.h"
#include "savesync.h"
#include "highscore.h"
#include "settings.h"
#include "frame.h"
#include "device.h"
#include "chat.h"
#include "contacts.h"

void setup() {
  Serial.begin(115200);
  initDisplay();
  initInput();
  initHighscores();                    // starts the EEPROM: scores and settings are saved there
  initDevice();                        // saved brightness
  initContacts();                      // chat aliases
  initChat();
  if (!isFirstBoot()) showSplash();    // the first boot has its own greeting
  syncInit();
  initGameManager();
}

void loop() {
  updateInput();
  updateDevice(gameManager.keepsScreenOn());   // screen-off timer; may swallow the press that wakes the screen
  updateChat(gameManager.radioBlocked());      // keeps the radio listening for chat, and shows the blip
  updateGameManager();
  frameWait();   // steady game speed (see frame.h)
}