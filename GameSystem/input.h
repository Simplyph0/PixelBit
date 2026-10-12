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

#endif