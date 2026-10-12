#ifndef TUTORIAL_H
#define TUTORIAL_H

#include "config.h"

// The tour shown on the first boot (and again when DOWN is held on the menu).
// Some pages ask the player to press a button before they continue; holding any
// button for TOUR_SKIP_MS skips the whole tour.
enum TutorialResult : uint8_t { TUT_RUNNING, TUT_FINISHED, TUT_SKIPPED };

class Tutorial {
public:
  void begin();
  TutorialResult update();
  void draw();   // does not present; the manager does

private:
  uint8_t page;
  unsigned long anyHeldSince;    // some button held since then (0 = none)
  unsigned long comboSince;      // UP + DOWN held together since then
  bool armed;                    // all buttons were released once: only then does holding skip

  void go(int to);
};

extern Tutorial tutorial;

#endif
