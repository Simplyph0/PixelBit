#include "device.h"
#include "highscore.h"   // DEVICE_ADDR: the EEPROM map
#include "input.h"
#include "display.h"
#include <EEPROM.h>

// EEPROM record at DEVICE_ADDR: "DEVS", brightness, sleep, chat, checksum (sum of the
// first seven bytes). Anything unreadable falls back to the defaults.
static const DeviceSettings DEFAULTS = {3, 0, 1};
static DeviceSettings settings = DEFAULTS;
static bool screenOff = false;
static unsigned long lastActivity = 0;

const char* const BRIGHTNESS_NAMES[BRIGHTNESS_LEVELS] = {"LOWEST", "LOW", "MEDIUM", "HIGH", "MAX"};
const char* const SLEEP_NAMES[SLEEP_CHOICES] = {"NEVER", "1 MIN", "2 MIN", "5 MIN"};
static const uint8_t CONTRAST[BRIGHTNESS_LEVELS] = {0x05, 0x30, 0x80, 0xCF, 0xFF};   // HIGH is the display's usual setting
static const unsigned long SLEEP_MS[SLEEP_CHOICES] = {0, 60000UL, 120000UL, 300000UL};

static void applyContrast() {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(CONTRAST[settings.brightness]);
}

static void save() {
  uint8_t b[7] = {'D', 'E', 'V', 'S', settings.brightness, settings.sleep, settings.chat};
  uint8_t sum = 0;
  for (int i = 0; i < 7; i++) { EEPROM.write(DEVICE_ADDR + i, b[i]); sum += b[i]; }
  EEPROM.write(DEVICE_ADDR + 7, sum);
  EEPROM.commit();
}

void initDevice() {
  uint8_t b[8], sum = 0;
  for (int i = 0; i < 8; i++) b[i] = EEPROM.read(DEVICE_ADDR + i);
  for (int i = 0; i < 7; i++) sum += b[i];
  if (b[0] == 'D' && b[1] == 'E' && b[2] == 'V' && b[3] == 'S' && b[7] == sum &&
      b[4] < BRIGHTNESS_LEVELS && b[5] < SLEEP_CHOICES && b[6] < 2) {
    settings.brightness = b[4];
    settings.sleep = b[5];
    settings.chat = b[6];
  }
  lastActivity = millis();
  applyContrast();
}

const DeviceSettings& deviceSettings() { return settings; }

void setBrightness(uint8_t level) {
  if (level >= BRIGHTNESS_LEVELS) return;
  settings.brightness = level;
  applyContrast();
  save();
}

void setSleep(uint8_t choice) {
  if (choice >= SLEEP_CHOICES) return;
  settings.sleep = choice;
  save();
}

void setChat(bool on) {
  settings.chat = on ? 1 : 0;
  save();
}

void resetDeviceSettings() {
  settings = DEFAULTS;
  applyContrast();
  save();
}

void wakeScreen() {
  if (screenOff) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    screenOff = false;
  }
  lastActivity = millis();
}

void updateDevice(bool keepAwake) {
  unsigned long now = millis();
  bool any = buttons.up || buttons.down || buttons.left || buttons.right;
  if (screenOff) {
    if (any) {
      display.ssd1306_command(SSD1306_DISPLAYON);
      screenOff = false;
      lastActivity = now;
    }
    if (any || buttons.upPressed || buttons.downPressed || buttons.leftPressed || buttons.rightPressed)
      buttons = {};   // the press that woke the screen does nothing else
    return;
  }
  if (any || keepAwake || !SLEEP_MS[settings.sleep]) {
    lastActivity = now;
  } else if (now - lastActivity >= SLEEP_MS[settings.sleep]) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    screenOff = true;
  }
}
