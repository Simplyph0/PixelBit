#ifndef TESTMODE_H
#define TESTMODE_H

#include "config.h"
#include "crypto.h"

// Manufacturing test mode. Entered from the first-boot greeting or tour with
// LEFT three times, then holding UP (see GameManager). It never saves the
// "tour acknowledged" flag, so a unit that is tested still greets its new owner.
// Tests: display patterns, buttons, storage (reads the flash back), radio, system info,
// firmware update, and the button layout (the one thing here that is saved).
class TestMode {
public:
  void begin();
  bool update();   // true when the tester chooses EXIT (the console then restarts)
  bool draw();     // draws; false = nothing changed, so the manager can skip the display refresh

private:
  enum Page : uint8_t { P_LIST, P_DISPLAY, P_BUTTONS, P_STORAGE, P_RADIO, P_SYSTEM, P_UPDATE, P_LAYOUT };
  Page page;
  uint8_t sel;
  uint8_t result[8];            // per list item: 0 not run, 1 pass, 2 fail
  bool layoutArmed, layoutHint;
  uint8_t layoutPicked;         // 0 not yet, 1 normal, 2 upside down
  uint8_t pattern;
  uint8_t patternsSeen;         // bit per pattern shown
  uint8_t buttonsSeen;          // bit per button pressed (up, down, left, right)
  unsigned long comboSince;
  bool storageOk;
  char storageDetail[24];
  bool radioStarted, radioOk, radioHosting;
  int peersSeen;
  char peerIds[3][8];

  // fingerprint of the running firmware, worked out a little each frame
  Sha256 fwHasher;
  uint32_t fwPos;
  bool fwDone;
  char fwText[10];
  void updateFingerprint();

  void open(Page p);
  void leavePage();
  void runStorageTest();
  void updateRadio();
};

extern TestMode testMode;

#endif
