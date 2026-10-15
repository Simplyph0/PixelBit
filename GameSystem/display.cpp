#include "display.h"
#include "input.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static uint8_t displayAddress = SCREEN_ADDRESS;

// 0 = the device answered; 2 = nobody answered the address; 1, 3, 4, 5 = other bus trouble
static uint8_t probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission();
}

// I2C bus recovery. When SDA and SCL are wired the other way round, probing them in the
// wrong order sends the display garbage, which can leave it half way through a byte and
// holding SDA low, so that it ignores every later probe. The standard cure is to clock SCL
// (up to nine times) until the device lets go of SDA, then send a STOP. Only on the real
// board: the emulator has no pins.
#ifdef ARDUINO_ARCH_ESP8266
static void busClear(int sda, int scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  delayMicroseconds(20);
  for (int i = 0; i < 9 && digitalRead(sda) == LOW; i++) {
    pinMode(scl, OUTPUT);
    digitalWrite(scl, LOW);
    delayMicroseconds(10);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(10);
  }
  pinMode(sda, OUTPUT);            // STOP: SDA goes from low to high while SCL is high
  digitalWrite(sda, LOW);
  delayMicroseconds(10);
  pinMode(sda, INPUT_PULLUP);
  delayMicroseconds(10);
}
#else
static void busClear(int, int) {}
#endif

// Finds the display: at SCREEN_ADDRESS or the address next to it (0x3C or 0x3D, the two
// that the usual modules use), on the configured pins and then with SDA and SCL swapped.
// After every bus restart the clock is set and the lines get a moment to settle. A display
// that has only just been powered does not answer yet, so when nothing answers it tries
// again for about a second before giving up, and says what the bus reported. On success
// the bus is left set up for the pins that worked. The library must not call Wire.begin()
// again afterwards: that would reset the bus to the default pins.
static bool findDisplay() {
  const uint8_t addresses[2] = {SCREEN_ADDRESS, (uint8_t)(SCREEN_ADDRESS ^ 1)};
  uint8_t status[2][2] = {{255, 255}, {255, 255}};   // [swapped][address]: the last result
  for (int attempt = 0; attempt < 10; attempt++) {
    for (int swapped = 0; swapped < 2; swapped++) {
      busClear(swapped ? PIN_SCL : PIN_SDA, swapped ? PIN_SDA : PIN_SCL);   // frees a display the other order left stuck
      if (swapped) Wire.begin(PIN_SCL, PIN_SDA);
      else         Wire.begin(PIN_SDA, PIN_SCL);
      Wire.setClock(100000);
      delay(2);
      for (int a = 0; a < 2; a++) {
        status[swapped][a] = probe(addresses[a]);
        if (status[swapped][a] != 0) continue;
        displayAddress = addresses[a];
        Serial.printf("Display at 0x%02X%s\n", displayAddress, swapped ? ", SDA and SCL swapped" : "");
        return true;
      }
    }
    delay(100);
  }
  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.printf("No display answered. Bus status (0 = ok, 2 = no answer): 0x%02X normal=%d swapped=%d, 0x%02X normal=%d swapped=%d\n",
                addresses[0], status[0][0], status[1][0], addresses[1], status[0][1], status[1][1]);
  return false;
}

void initDisplay() {
  findDisplay();

  if (!display.begin(SSD1306_SWITCHCAPVCC, displayAddress, true, false)) {
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

static bool blipOn = false;
static unsigned long attentionUntil = 0;

void setNotifyBlip(bool on) { blipOn = on; }
void notifyAttention() { attentionUntil = millis() + 4000; }

// The corner is 16 x 16 pixels: columns 112..127, the first two pages of the buffer
static const int BLIP_X = 120, BLIP_Y = 7;

static void drawBlip() {
  unsigned long now = millis();
  bool fast = (long)(attentionUntil - now) > 0;
  bool solid = fast ? (now / 150) % 2 == 0 : (now / 600) % 3 != 0;   // flashes quickly at first, then pulses
  display.fillCircle(BLIP_X, BLIP_Y, 7, SSD1306_BLACK);               // clears a ring around the dot
  if (solid) {
    display.fillCircle(BLIP_X, BLIP_Y, 5, SSD1306_WHITE);
    display.fillCircle(BLIP_X, BLIP_Y, 1, SSD1306_BLACK);
  } else {
    display.drawCircle(BLIP_X, BLIP_Y, 5, SSD1306_WHITE);
    display.fillCircle(BLIP_X, BLIP_Y, 2, SSD1306_WHITE);
  }
}

void updateDisplay() {
  if (!blipOn) {
    display.display();
    return;
  }
  uint8_t* buf = display.getBuffer();
  uint8_t keep[2][16];
  for (int page = 0; page < 2; page++) memcpy(keep[page], buf + page * SCREEN_WIDTH + 112, 16);
  drawBlip();
  display.display();
  for (int page = 0; page < 2; page++) memcpy(buf + page * SCREEN_WIDTH + 112, keep[page], 16);   // the game's picture is as it was
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
