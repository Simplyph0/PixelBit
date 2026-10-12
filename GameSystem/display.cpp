#include "display.h"
#include "input.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static bool displayAnswers() {
  Wire.beginTransmission(SCREEN_ADDRESS);
  return Wire.endTransmission() == 0;
}

void initDisplay() {
  // Find the display: the configured pins first, then the same two swapped. The
  // library must not call Wire.begin() again: that would reset the bus to the
  // default pins.
  Wire.begin(PIN_SDA, PIN_SCL);
  if (!displayAnswers()) {
    Wire.begin(PIN_SCL, PIN_SDA);
    if (displayAnswers()) {
      Serial.println(F("Display found with SDA and SCL swapped"));
    } else {
      Wire.begin(PIN_SDA, PIN_SCL);
      Serial.println(F("No display answered on either pin order"));
    }
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS, true, false)) {
    // Nothing to draw on: say why over USB now and then, without tripping the watchdog
    for (;;) {
      Serial.println(F("SSD1306 display could not be started (check its wiring)"));
      delay(2000);
    }
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
    if (rawPressedPin() >= 0) break;
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
