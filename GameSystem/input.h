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

// Button layout. A console built upside down has every button in the opposite
// place; the layout is chosen at run time (saved in the settings, set in test
// mode > LAYOUT) so one firmware works on both kinds of console.
void setButtonsFlipped(bool flipped);
bool buttonsFlipped();
int buttonPin(int button);   // 0 up, 1 down, 2 left, 3 right: the GPIO it is wired to
int rawPressedPin();         // the GPIO of a button held right now, or -1 (ignores the layout)
int layoutFromUpPin(int pin);   // the button called UP is on this GPIO: 0 normal, 1 upside down, -1 not UP or DOWN

#endif
