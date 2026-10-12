#include "display.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void initDisplay() {
  // The library must not call Wire.begin() again: that would reset the bus to
  // the default pins and ignore PIN_SDA / PIN_SCL (FLIPPED_I2C).
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS, true, false)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.display();
}

// Boot screen: OS name large, version smaller underneath. Any button skips it.
void showSplash() {
  display.clearDisplay();

  drawCenteredText(OS_NAME, 14, 3);      // 6 chars x 18 px = 108 px wide
  drawCenteredText(OS_VERSION, 44, 1);
  display.display();

  unsigned long start = millis();
  while (millis() - start < SPLASH_MS) {
    if (digitalRead(PIN_BUTTON_UP) == LOW || digitalRead(PIN_BUTTON_DOWN) == LOW ||
        digitalRead(PIN_BUTTON_LEFT) == LOW || digitalRead(PIN_BUTTON_RIGHT) == LOW) {
      break;
    }
    delay(10);
  }
}

void clearDisplay() {
  display.clearDisplay();
}

void updateDisplay() {
  display.display();
}

void drawCenteredText(const char* text, int y, int textSize) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}
