#include "input.h"

ButtonState buttons;

void initInput() {
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);
  
  // Initialize button states
  buttons = {};
}

void updateInput() {
  const ButtonState prev = buttons;
  
  // Read current state (inverted because of INPUT_PULLUP)
  buttons.up = (digitalRead(PIN_BUTTON_UP) == LOW);
  buttons.down = (digitalRead(PIN_BUTTON_DOWN) == LOW);
  buttons.left = (digitalRead(PIN_BUTTON_LEFT) == LOW);
  buttons.right = (digitalRead(PIN_BUTTON_RIGHT) == LOW);
  
  // Detect button press events (just pressed)
  buttons.upPressed = buttons.up && !prev.up;
  buttons.downPressed = buttons.down && !prev.down;
  buttons.leftPressed = buttons.left && !prev.left;
  buttons.rightPressed = buttons.right && !prev.right;

  // Debug: log every press (pin numbers come from config.h)
  if (buttons.upPressed || buttons.downPressed || buttons.leftPressed || buttons.rightPressed) {
    LOG("btn pressed: UP=%d DOWN=%d LEFT=%d RIGHT=%d\n", buttons.upPressed, buttons.downPressed,
        buttons.leftPressed, buttons.rightPressed);
  }
}
