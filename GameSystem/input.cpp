#include "input.h"

ButtonState buttons;

// GPIOs of the four buttons, as set in config.h (FLIPPED_BUTTONS picks them)
static const int PINS[4] = {PIN_BUTTON_UP, PIN_BUTTON_DOWN, PIN_BUTTON_LEFT, PIN_BUTTON_RIGHT};

// A change is only taken once the button has been steady for this long, so a
// bouncing contact does not count as two presses
static const unsigned long DEBOUNCE_MS = 25;
static bool stable[4];
static unsigned long changedAt[4];

int buttonPin(int button) {
  if (button < 0 || button > 3) return -1;
  return PINS[button];
}

int rawPressedPin() {
  for (int i = 0; i < 4; i++) if (digitalRead(PINS[i]) == LOW) return PINS[i];
  return -1;
}

void initInput() {
  for (int i = 0; i < 4; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
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
