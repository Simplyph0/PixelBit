#include "input.h"

ButtonState buttons;

// GPIOs of the four buttons on a console built the normal way round. Upside
// down, UP and DOWN swap pins, and so do LEFT and RIGHT.
static const int PINS_NORMAL[4] = {12, 13, 2, 14};   // up, down, left, right
static bool flippedLayout = FLIPPED_BUTTONS;

// A change is only taken once the button has been steady for this long, so a
// bouncing contact does not count as two presses
static const unsigned long DEBOUNCE_MS = 25;
static bool stable[4];
static unsigned long changedAt[4];

void setButtonsFlipped(bool flipped) { flippedLayout = flipped; }
bool buttonsFlipped() { return flippedLayout; }

int buttonPin(int button) {
  if (button < 0 || button > 3) return -1;
  return PINS_NORMAL[flippedLayout ? (button ^ 1) : button];
}

int rawPressedPin() {
  for (int i = 0; i < 4; i++) if (digitalRead(PINS_NORMAL[i]) == LOW) return PINS_NORMAL[i];
  return -1;
}

int layoutFromUpPin(int pin) {
  if (pin == PINS_NORMAL[0]) return 0;
  if (pin == PINS_NORMAL[1]) return 1;
  return -1;
}

void initInput() {
  for (int i = 0; i < 4; i++) {
    pinMode(PINS_NORMAL[i], INPUT_PULLUP);
    stable[i] = false;
    changedAt[i] = 0;
  }
  buttons = {};
}

void updateInput() {
  unsigned long now = millis();
  bool pressed[4];
  for (int i = 0; i < 4; i++) {
    bool raw = digitalRead(buttonPin(i)) == LOW;   // INPUT_PULLUP: pressed reads LOW
    pressed[i] = false;
    if (raw != stable[i] && now - changedAt[i] >= DEBOUNCE_MS) {
      stable[i] = raw;
      changedAt[i] = now;
      pressed[i] = raw;
    }
  }

  buttons.up = stable[0];
  buttons.down = stable[1];
  buttons.left = stable[2];
  buttons.right = stable[3];
  buttons.upPressed = pressed[0];
  buttons.downPressed = pressed[1];
  buttons.leftPressed = pressed[2];
  buttons.rightPressed = pressed[3];

  if (pressed[0] || pressed[1] || pressed[2] || pressed[3]) {
    LOG("btn pressed: UP=%d DOWN=%d LEFT=%d RIGHT=%d\n", pressed[0], pressed[1], pressed[2], pressed[3]);
  }
}
