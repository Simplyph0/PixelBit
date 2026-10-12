#ifndef INPUT_H
#define INPUT_H

#include "config.h"

struct ButtonState {
  bool up;
  bool down;
  bool left;
  bool right;
  bool upPressed;
  bool downPressed;
  bool leftPressed;
  bool rightPressed;
};

extern ButtonState buttons;

void initInput();
void updateInput();

// The pins come from config.h (FLIPPED_BUTTONS chooses the layout at build time)
int buttonPin(int button);   // 0 up, 1 down, 2 left, 3 right: the GPIO it is wired to
int rawPressedPin();         // the GPIO of a button held right now, or -1

#endif
