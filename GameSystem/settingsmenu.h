#ifndef SETTINGSMENU_H
#define SETTINGSMENU_H

#include "config.h"
#include "game.h"

// SETTINGS: the last tile of the menu. Console-wide settings and tools that need
// neither a computer nor typing (those stay on the Wi-Fi pages).
//
// List: UP/DOWN scroll, RIGHT changes the value or opens the entry, LEFT leaves.
//   BRIGHTNESS      five steps, applied at once and kept
//   SCREEN OFF      NEVER, 1, 2 or 5 minutes without a button (device.h)
//   CHAT            ON: the console listens for chat messages in the background (chat.h)
//   WI-FI PAGE      a private Wi-Fi network with a web page: save backup, chat contacts (savesync.h)
//   UPDATE FIRMWARE console-to-console update over the radio (fwupdate.h)
//   RESET SCORES    hold RIGHT for 2 seconds to confirm
//   RESET OPTIONS   forgets every game's remembered settings (same confirmation)
//   SHOW INTRO      the welcome and tour play again on the next boot
//   FACTORY RESET   everything back to new: scores, settings, intro, chat contacts, videos (hold 3 seconds)
//   ABOUT           version, this console's id, build date, button layout, free memory
class SettingsMenu : public Game {
public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return false; }
  int getScore() override { return 0; }
  const char* getName() override { return "SETTINGS"; }
  bool hasScore() override { return false; }
  const char* menuNote() override { return "SYSTEM"; }
  bool wantsExit() override { return exitRequested; }
  bool keepsScreenOn() override { return mode == MODE_UPDATE || mode == MODE_WIFI; }
  bool radioBusy() const { return mode == MODE_WIFI; }   // the access point has the radio
  bool skipPresent() override { return !present; }
  void stop() override;

private:
  enum Mode : uint8_t { MODE_LIST, MODE_UPDATE, MODE_CONFIRM, MODE_DONE, MODE_ABOUT, MODE_WIFI };
  Mode mode = MODE_LIST;
  bool exitRequested = false;
  bool present = true;          // false: the firmware update drew nothing new this frame
  uint8_t sel = 0;
  uint8_t confirmKind = 0;      // what MODE_CONFIRM is about to erase
  unsigned long holdSince = 0, doneUntil = 0;
  const char* doneText = "";
  bool armed = false;              // RIGHT was let go since the confirmation opened
  bool restartAfterDone = false;   // the factory reset ends with a restart

  void activate();
  void drawList();
  void drawConfirm();
  void drawAbout();
};

extern SettingsMenu settingsMenu;

#endif
