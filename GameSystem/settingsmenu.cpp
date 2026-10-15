#include "settingsmenu.h"
#include <ESP8266WiFi.h>   // ESP.getFreeHeap()
#include "display.h"
#include "input.h"
#include "device.h"
#include "highscore.h"
#include "fwupdate.h"
#include "savesync.h"
#include "gamelink.h"
#include "gamemanager.h"
#include "contacts.h"
#include "videoplayer.h"
#include "ui.h"

SettingsMenu settingsMenu;

enum Item : uint8_t { IT_BRIGHT, IT_SLEEP, IT_CHAT, IT_WIFI, IT_UPDATE, IT_SCORES, IT_OPTIONS, IT_INTRO, IT_FACTORY, IT_ABOUT, IT_COUNT };

// The entries shown, in order (the update is left out when the build has no firmware update)
static uint8_t shown[IT_COUNT];
static uint8_t shownCount;

static const char* itemName(uint8_t it) {
  switch (it) {
    case IT_BRIGHT:  return "BRIGHTNESS";
    case IT_SLEEP:   return "SCREEN OFF";
    case IT_CHAT:    return "CHAT";
    case IT_WIFI:    return "WI-FI PAGE";
    case IT_UPDATE:  return "UPDATE FIRMWARE";
    case IT_SCORES:  return "RESET SCORES";
    case IT_OPTIONS: return "RESET OPTIONS";
    case IT_INTRO:   return "SHOW INTRO";
    case IT_FACTORY: return "FACTORY RESET";
    default:         return "ABOUT";
  }
}

void SettingsMenu::init() {
  mode = MODE_LIST;
  exitRequested = false;
  present = true;
  sel = 0;
  holdSince = doneUntil = 0;
  shownCount = 0;
  for (uint8_t i = 0; i < IT_COUNT; i++) {
#if !ENABLE_P2P_UPDATE
    if (i == IT_UPDATE) continue;
#endif
    shown[shownCount++] = i;
  }
}

void SettingsMenu::stop() {
#if ENABLE_P2P_UPDATE
  if (mode == MODE_UPDATE) fwUpdate.stop();
#endif
  if (mode == MODE_WIFI) syncEnd();
  mode = MODE_LIST;
}

void SettingsMenu::activate() {
  switch (shown[sel]) {
    case IT_BRIGHT:
      setBrightness((deviceSettings().brightness + 1) % BRIGHTNESS_LEVELS);
      break;
    case IT_SLEEP:
      setSleep((deviceSettings().sleep + 1) % SLEEP_CHOICES);
      break;
    case IT_CHAT:
      setChat(!deviceSettings().chat);
      break;
    case IT_INTRO:
      gameManager.showIntroNextBoot();
      doneText = "INTRO ON NEXT BOOT";
      doneUntil = millis() + 1800;
      restartAfterDone = false;
      mode = MODE_DONE;
      break;
    case IT_WIFI:
      syncBegin();
      mode = MODE_WIFI;
      break;
    case IT_UPDATE:
#if ENABLE_P2P_UPDATE
      fwUpdate.begin();
      mode = MODE_UPDATE;
#endif
      break;
    case IT_SCORES:
    case IT_OPTIONS:
    case IT_FACTORY:
      confirmKind = shown[sel];
      holdSince = 0;
      armed = false;
      mode = MODE_CONFIRM;
      break;
    case IT_ABOUT:
      mode = MODE_ABOUT;
      break;
  }
}

void SettingsMenu::update() {
  present = true;
  unsigned long now = millis();
  switch (mode) {
    case MODE_LIST:
      if (buttons.downPressed) sel = (sel + 1) % shownCount;
      else if (buttons.upPressed) sel = (sel + shownCount - 1) % shownCount;
      else if (buttons.rightPressed) activate();
      else if (buttons.leftPressed) exitRequested = true;
      break;

    case MODE_WIFI:
      if (buttons.leftPressed || !syncUpdate()) {
        syncEnd();
        mode = MODE_LIST;
      }
      break;

    case MODE_UPDATE:
#if ENABLE_P2P_UPDATE
      if (fwUpdate.update()) mode = MODE_LIST;   // fwUpdate.stop() has already run
#else
      mode = MODE_LIST;
#endif
      break;

    case MODE_CONFIRM:
      if (buttons.leftPressed) { mode = MODE_LIST; break; }
      // The RIGHT that opened this screen may still be down: the hold only starts after it has been let go,
      // so holding RIGHT a little long on the list can never erase anything
      if (!buttons.right) armed = true;
      if (armed && buttons.right && !buttons.left) {
        if (!holdSince) holdSince = now;
        if (now - holdSince >= (confirmKind == IT_FACTORY ? 3000UL : 2000UL)) {
          restartAfterDone = false;
          if (confirmKind == IT_SCORES) {
            highscoreManager.resetAllHighscores();
            doneText = "SCORES ERASED";
          } else if (confirmKind == IT_OPTIONS) {
            gameManager.resetGameOptions();
            doneText = "SETTINGS FORGOTTEN";
          } else {
            gameManager.factoryReset();   // scores, game settings, tour flag
            resetDeviceSettings();        // brightness, screen off, chat
            contactsClear();              // chat aliases
            videoEraseAll();              // clips
            doneText = "RESETTING...";
            restartAfterDone = true;      // and start up like a new console
          }
          doneUntil = now + (restartAfterDone ? 1800UL : 1500UL);
          mode = MODE_DONE;
        }
      } else {
        holdSince = 0;
      }
      break;

    case MODE_DONE:
      if (restartAfterDone) {
        if (now >= doneUntil) ESP.restart();   // a clean boot: the console greets its owner again
      } else if (now >= doneUntil || buttons.leftPressed || buttons.rightPressed) {
        mode = MODE_LIST;
      }
      break;

    case MODE_ABOUT:
      if (buttons.leftPressed || buttons.rightPressed) mode = MODE_LIST;
      break;
  }
}

void SettingsMenu::draw() {
  switch (mode) {
    case MODE_UPDATE:
#if ENABLE_P2P_UPDATE
      present = fwUpdate.draw();
#endif
      break;
    case MODE_WIFI:
      present = syncDraw();
      break;
    case MODE_CONFIRM: drawConfirm(); break;
    case MODE_DONE:
      clearDisplay();
      drawCenteredText(doneText, 28, 1);
      break;
    case MODE_ABOUT: drawAbout(); break;
    default: drawList(); break;
  }
}

void SettingsMenu::drawList() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("SETTINGS"));
  char text[24];
  snprintf(text, sizeof(text), "%d/%d", sel + 1, shownCount);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(text), 0);
  display.print(text);

  const int rows = 5;
  int first = sel < rows ? 0 : sel - rows + 1;
  for (int r = 0; r < rows && first + r < shownCount; r++) {
    uint8_t it = shown[first + r];
    int y = 10 + r * 9;
    bool on = first + r == sel;
    if (on) display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
    display.setTextColor(on ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(2, y);
    display.print(itemName(it));
    const char* value = nullptr;
    if (it == IT_BRIGHT) value = BRIGHTNESS_NAMES[deviceSettings().brightness];
    else if (it == IT_SLEEP) value = SLEEP_NAMES[deviceSettings().sleep];
    else if (it == IT_CHAT) value = deviceSettings().chat ? "ON" : "OFF";
    if (value) {
      display.setCursor(SCREEN_WIDTH - 2 - 6 * strlen(value), y);
      display.print(value);
    }
  }
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print(F("UD:PICK R:SET L:BACK"));
}

void SettingsMenu::drawConfirm() {
  clearDisplay();
  bool factory = confirmKind == IT_FACTORY;
  drawCenteredText(confirmKind == IT_SCORES ? "ERASE ALL SCORES?" : factory ? "FACTORY RESET?" : "FORGET GAME SETTINGS?", 4, 1);
  if (factory) {
    drawCenteredText("Scores, settings,", 14, 1);
    drawCenteredText("contacts and videos.", 23, 1);
  } else {
    drawCenteredText(confirmKind == IT_SCORES ? "Cannot be undone." : "Every game asks again", 20, 1);
  }
  drawCenteredText(factory ? "HOLD RIGHT 3 SECONDS" : "HOLD RIGHT 2 SECONDS", 34, 1);
  display.drawRect(14, 44, 100, 6, SSD1306_WHITE);
  if (holdSince) {
    unsigned long held = millis() - holdSince;
    unsigned long total = factory ? 3000UL : 2000UL;
    display.fillRect(14, 44, (int)(min(held, total) * 100 / total), 6, SSD1306_WHITE);
  }
  drawCenteredText("LEFT: CANCEL", 56, 1);
}

void SettingsMenu::drawAbout() {
  clearDisplay();
  drawCenteredText(OS_NAME " " OS_VERSION, 2, 1);
  char text[24], id[LINK_ID_LEN + 1];
  linkOwnId(id);
  snprintf(text, sizeof(text), "YOUR ID: %s", id);   // what other consoles (chat, versus) call this one
  drawCenteredText(text, 13, 1);
  drawCenteredText(__DATE__, 24, 1);
  drawCenteredText(FLIPPED_BUTTONS ? "BUTTONS: FLIPPED" : "BUTTONS: NORMAL", 35, 1);
  snprintf(text, sizeof(text), "FREE RAM %u", (unsigned)ESP.getFreeHeap());
  drawCenteredText(text, 46, 1);
  drawCenteredText("LEFT: BACK", 55, 1);
}
