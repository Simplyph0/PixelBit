#ifndef WELCOME_H
#define WELCOME_H

#include "config.h"

// The animated greeting on the very first boot: the PixelOS logo dissolves in
// and out, then "Welcome to your new PixelBit!" types itself out with sparkles.
// Any button skips it (to the tour).
class Welcome {
public:
  void begin();
  bool update();   // true when it is over (finished or skipped)
  void draw();     // does not present; the manager does

private:
  unsigned long startedAt;
};

extern Welcome welcome;

#endif
