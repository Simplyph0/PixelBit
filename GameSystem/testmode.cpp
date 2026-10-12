#include "testmode.h"
#include "display.h"
#include "input.h"
#include "gamelink.h"
#include "highscore.h"
#include "fwupdate.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>

TestMode testMode;

struct TestItem { const char* name; uint8_t page; };   // page 0 = EXIT
static const TestItem ITEM_LIST[] = {
  {"DISPLAY", 1}, {"BUTTONS", 2}, {"STORAGE", 3}, {"RADIO", 4}, {"SYSTEM", 5},
#if ENABLE_P2P_UPDATE
  {"UPDATE", 6},
#endif
  {"EXIT", 0}
};
static const uint8_t ITEMS = sizeof(ITEM_LIST) / sizeof(ITEM_LIST[0]);
static const uint8_t VISIBLE_ROWS = 6;
static const uint8_t PATTERNS = 8;

// Scratch area for the storage test, after the scores and the settings (see highscore.h)
static const int TEST_ADDR = TEST_SCRATCH_ADDR;
static const int TEST_LEN = TEST_SCRATCH_LEN;

void TestMode::begin() {
  page = P_LIST;
  sel = 0;
  memset(result, 0, sizeof(result));
  pattern = 0;
  patternsSeen = 0;
  buttonsSeen = 0;
  comboSince = 0;
  radioStarted = false;
  peersSeen = 0;
}

void TestMode::open(Page p) {
  page = p;
  comboSince = 0;
  switch (p) {
    case P_DISPLAY:
      pattern = 0;
      patternsSeen = 1;
      break;
    case P_BUTTONS:
      buttonsSeen = 0;
      break;
    case P_STORAGE:
      runStorageTest();
      break;
    case P_SYSTEM:
      fwHasher.begin();
      fwPos = 0;
      fwDone = false;
      break;
    case P_UPDATE:
#if ENABLE_P2P_UPDATE
      fwUpdate.begin();
#endif
      break;
    case P_RADIO:
      radioStarted = true;
      radioOk = gameLink.begin(250);   // game tag 250: the test beacons
      radioHosting = false;
      peersSeen = 0;
      if (radioOk) gameLink.scan();
      break;
    default:
      break;
  }
}

void TestMode::leavePage() {
  if (page == P_RADIO && radioStarted) {
    gameLink.end();
    radioStarted = false;
  }
  if (page == P_DISPLAY) display.invertDisplay(false);
#if ENABLE_P2P_UPDATE
  if (page == P_UPDATE) fwUpdate.stop();
#endif
  page = P_LIST;
}

// ---------------------------------------------------------------------------
// Storage: write four patterns, read each back from the EEPROM buffer and from
// the flash itself (on the real board), so a bad flash chip shows up
// ---------------------------------------------------------------------------
#ifdef ARDUINO_ARCH_ESP8266
extern "C" uint32_t _EEPROM_start;
#endif

void TestMode::runStorageTest() {
  static const uint8_t PATTERN_BYTES[4] = {0x55, 0xAA, 0xFF, 0x00};
  storageOk = true;
  strcpy(storageDetail, "all patterns ok");

  for (int p = 0; p < 4 && storageOk; p++) {
    for (int i = 0; i < TEST_LEN; i++) EEPROM.write(TEST_ADDR + i, PATTERN_BYTES[p] ^ (i == 7 ? 0x0F : 0));
    if (!EEPROM.commit()) { storageOk = false; strcpy(storageDetail, "commit failed"); break; }

    for (int i = 0; i < TEST_LEN; i++) {
      if (EEPROM.read(TEST_ADDR + i) != (uint8_t)(PATTERN_BYTES[p] ^ (i == 7 ? 0x0F : 0))) {
        storageOk = false;
        snprintf(storageDetail, sizeof(storageDetail), "buffer byte %d", i);
        break;
      }
    }

#ifdef ARDUINO_ARCH_ESP8266
    if (storageOk) {
      uint32_t flashBase = (uint32_t)&_EEPROM_start - 0x40200000;
      uint32_t words[TEST_LEN / 4];
      if (!ESP.flashRead(flashBase + TEST_ADDR, words, TEST_LEN)) {
        storageOk = false;
        strcpy(storageDetail, "flash read failed");
      } else {
        const uint8_t* bytes = (const uint8_t*)words;
        for (int i = 0; i < TEST_LEN; i++) {
          if (bytes[i] != (uint8_t)(PATTERN_BYTES[p] ^ (i == 7 ? 0x0F : 0))) {
            storageOk = false;
            snprintf(storageDetail, sizeof(storageDetail), "flash byte %d", i);
            break;
          }
        }
      }
    }
#endif
  }

  // Leave the scratch area empty
  for (int i = 0; i < TEST_LEN; i++) EEPROM.write(TEST_ADDR + i, 0);
  EEPROM.commit();
  result[2] = storageOk ? 1 : 2;
}

// ---------------------------------------------------------------------------
// Radio: scan for other consoles in test mode; UP switches between scanning
// and beaconing, so two units can find each other
// ---------------------------------------------------------------------------
void TestMode::updateRadio() {
  if (!radioOk) { result[3] = 2; return; }
  gameLink.poll();

  if (buttons.upPressed) {
    radioHosting = !radioHosting;
    if (radioHosting) gameLink.host(); else gameLink.scan();
  }

  if (!radioHosting) {
    int n = gameLink.hostCount();
    for (int i = 0; i < n && i < 3; i++) strncpy(peerIds[i], gameLink.hostId(i), 7);
    if (n > peersSeen) peersSeen = n;
    if (n > 0) result[3] = 1;
  }
}

// ---------------------------------------------------------------------------
bool TestMode::update() {
  unsigned long now = millis();

  switch (page) {
    case P_LIST:
      if (buttons.rightPressed || buttons.downPressed) sel = (sel + 1) % ITEMS;
      else if (buttons.leftPressed) sel = (sel + ITEMS - 1) % ITEMS;
      else if (buttons.upPressed) {
        uint8_t p = ITEM_LIST[sel].page;
        if (p == 0) return true;   // EXIT
        open((Page)p);
        if (p == P_SYSTEM) result[sel] = 1;
      }
      break;

    case P_DISPLAY:
      if (buttons.upPressed || buttons.rightPressed) {
        pattern = (pattern + 1) % PATTERNS;
        patternsSeen |= 1 << pattern;
        if (patternsSeen == 0xFF) result[0] = 1;
      } else if (buttons.leftPressed) {
        pattern = (pattern + PATTERNS - 1) % PATTERNS;
      } else if (buttons.downPressed) {
        leavePage();
      }
      break;

    case P_BUTTONS:
      if (buttons.up) buttonsSeen |= 1;
      if (buttons.down) buttonsSeen |= 2;
      if (buttons.left) buttonsSeen |= 4;
      if (buttons.right) buttonsSeen |= 8;
      if (buttonsSeen == 15) result[1] = 1;
      // leave by holding UP and DOWN together for a second
      if (buttons.up && buttons.down) {
        if (!comboSince) comboSince = now;
        if (now - comboSince >= EXIT_HOLD_MS) leavePage();
      } else {
        comboSince = 0;
      }
      break;

    case P_STORAGE:
      if (buttons.upPressed) runStorageTest();
      else if (buttons.downPressed) leavePage();
      break;

    case P_RADIO:
      updateRadio();
      if (buttons.downPressed) leavePage();
      break;

    case P_SYSTEM:
      updateFingerprint();
      if (buttons.downPressed || buttons.upPressed) leavePage();
      break;

    case P_UPDATE:
#if ENABLE_P2P_UPDATE
      if (fwUpdate.update()) {
        result[sel] = 1;
        page = P_LIST;    // fwUpdate.stop() has already run
      }
#else
      leavePage();
#endif
      break;
  }
  return false;
}

// SHA-256 of the running firmware, a little each frame; the first 8 hex digits
// are shown so two consoles can be compared (and match what an update offers)
void TestMode::updateFingerprint() {
  if (fwDone) return;
  uint32_t size = ESP.getSketchSize();
  uint8_t buf[256];
  for (int i = 0; i < 16 && fwPos < size; i++) {
    uint32_t n = size - fwPos < sizeof(buf) ? size - fwPos : sizeof(buf);
    ESP.flashRead(fwPos, buf, n);
    fwHasher.update(buf, n);
    fwPos += n;
  }
  if (fwPos >= size) {
    uint8_t sha[32];
    fwHasher.finish(sha);
    FirmwareUpdate::fingerprint(sha, fwText);
    fwDone = true;
  }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void line(int y, const char* text) {
  display.setCursor(0, y);
  display.print(text);
}

bool TestMode::draw() {
  if (page == P_UPDATE) {
#if ENABLE_P2P_UPDATE
    return fwUpdate.draw();
#endif
  }
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char buf[40];

  switch (page) {
    case P_LIST: {
      line(0, "TEST MODE");
      display.setCursor(SCREEN_WIDTH - 6 * 9, 0);
      display.print(F("NOT SAVED"));
      int first = sel >= VISIBLE_ROWS ? sel - VISIBLE_ROWS + 1 : 0;   // scroll to keep the selection in view
      for (int row = 0; row < VISIBLE_ROWS && first + row < ITEMS; row++) {
        int i = first + row;
        int y = 10 + row * 9;
        bool on = i == sel;
        if (on) display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
        display.setTextColor(on ? SSD1306_BLACK : SSD1306_WHITE);
        display.setCursor(4, y);
        display.print(ITEM_LIST[i].name);
        if (ITEM_LIST[i].page != 0) {
          const char* r = result[i] == 1 ? "OK" : (result[i] == 2 ? "FAIL" : "--");
          display.setCursor(SCREEN_WIDTH - 6 * strlen(r) - 4, y);
          display.print(r);
        }
      }
      display.setTextColor(SSD1306_WHITE);
      break;
    }

    case P_DISPLAY: {
      // Whole-screen patterns: no text, so every pixel can be judged
      switch (pattern) {
        case 0: display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE); break;
        case 1: break;
        case 2: case 3:
          for (int y = 0; y < SCREEN_HEIGHT; y++)
            for (int x = 0; x < SCREEN_WIDTH; x++)
              if (((x + y) & 1) == (pattern == 2 ? 0 : 1)) display.drawPixel(x, y, SSD1306_WHITE);
          break;
        case 4: for (int y = 0; y < SCREEN_HEIGHT; y += 2) display.drawFastHLine(0, y, SCREEN_WIDTH, SSD1306_WHITE); break;
        case 5: for (int x = 0; x < SCREEN_WIDTH; x += 2) display.drawFastVLine(x, 0, SCREEN_HEIGHT, SSD1306_WHITE); break;
        case 6:
          display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
          display.drawLine(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);
          display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, 0, SSD1306_WHITE);
          display.drawFastHLine(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SSD1306_WHITE);
          display.drawFastVLine(SCREEN_WIDTH / 2, 0, SCREEN_HEIGHT, SSD1306_WHITE);
          break;
        default:   // a 8-pixel grid with the corners marked
          for (int y = 0; y < SCREEN_HEIGHT; y += 8) display.drawFastHLine(0, y, SCREEN_WIDTH, SSD1306_WHITE);
          for (int x = 0; x < SCREEN_WIDTH; x += 8) display.drawFastVLine(x, 0, SCREEN_HEIGHT, SSD1306_WHITE);
          display.fillRect(0, 0, 3, 3, SSD1306_WHITE);
          display.fillRect(SCREEN_WIDTH - 3, SCREEN_HEIGHT - 3, 3, 3, SSD1306_WHITE);
          break;
      }
      break;
    }

    case P_BUTTONS: {
      line(0, "BUTTON TEST");
      line(10, "Press each button.");
      struct Key { int x, y; uint8_t bit; const char* name; bool down; } keys[4] = {
        {59, 19, 1, "U", buttons.up}, {59, 39, 2, "D", buttons.down}, {40, 29, 4, "L", buttons.left}, {78, 29, 8, "R", buttons.right}
      };
      for (const Key& k : keys) {
        if (k.down) display.fillRect(k.x, k.y, 10, 10, SSD1306_WHITE);
        else display.drawRect(k.x, k.y, 10, 10, SSD1306_WHITE);
        if (buttonsSeen & k.bit) display.fillRect(k.x + 3, k.y + 3, 4, 4, k.down ? SSD1306_BLACK : SSD1306_WHITE);   // seen
      }
      if (result[1] == 1) { display.setCursor(SCREEN_WIDTH - 36, 0); display.print(F("ALL OK")); }
      unsigned long held = comboSince ? millis() - comboSince : 0;
      line(52, "Hold UP+DOWN: back");
      display.drawRect(0, 60, 128, 4, SSD1306_WHITE);
      display.fillRect(1, 61, (int)min(held * 126UL / EXIT_HOLD_MS, 126UL), 2, SSD1306_WHITE);
      break;
    }

    case P_STORAGE:
      line(0, "STORAGE TEST");
      line(14, storageOk ? "RESULT: PASS" : "RESULT: FAIL");
      line(26, storageDetail);
#ifdef ARDUINO_ARCH_ESP8266
      line(38, "buffer + flash read");
#else
      line(38, "buffer only (PC)");
#endif
      line(54, "UP:again  DOWN:back");
      break;

    case P_RADIO: {
      line(0, "RADIO TEST");
      if (!radioOk) {
        line(14, "INIT FAILED");
      } else {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        snprintf(buf, sizeof(buf), "MAC %02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        line(10, buf);
        snprintf(buf, sizeof(buf), "ID %s   %s", gameLink.localId(), radioHosting ? "BEACON" : "SCAN");
        line(20, buf);
        snprintf(buf, sizeof(buf), "SEEN: %d unit%s", peersSeen, peersSeen == 1 ? "" : "s");
        line(30, buf);
        if (!radioHosting && peersSeen > 0) {
          snprintf(buf, sizeof(buf), "%s %s %s", peerIds[0], peersSeen > 1 ? peerIds[1] : "", peersSeen > 2 ? peerIds[2] : "");
          line(40, buf);
        }
      }
      line(54, "UP:role  DOWN:back");
      break;
    }

    case P_UPDATE:
      break;

    case P_SYSTEM: {
      uint8_t mac[6];
      WiFi.macAddress(mac);
      char id[LINK_ID_LEN + 1];
      linkOwnId(id);
      line(0, OS_NAME " " OS_VERSION);
      snprintf(buf, sizeof(buf), "ID %s CHIP %06X", id, (unsigned)ESP.getChipId());
      line(9, buf);
      snprintf(buf, sizeof(buf), "MAC %02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      line(18, buf);
      snprintf(buf, sizeof(buf), "FLASH %uK/%uK", (unsigned)(ESP.getFlashChipRealSize() / 1024), (unsigned)(ESP.getFlashChipSize() / 1024));
      line(27, buf);
      snprintf(buf, sizeof(buf), "HEAP %u UP %lus", (unsigned)ESP.getFreeHeap(), millis() / 1000);
      line(36, buf);
      snprintf(buf, sizeof(buf), "RST %s", ESP.getResetReason().c_str());
      buf[21] = 0;
      line(45, buf);
      if (fwDone) snprintf(buf, sizeof(buf), "FW %s %uK", fwText, (unsigned)((ESP.getSketchSize() + 1023) / 1024));
      else snprintf(buf, sizeof(buf), "FW ID computing %d%%", (int)(fwPos * 100UL / (ESP.getSketchSize() ? ESP.getSketchSize() : 1)));
      line(54, buf);
      break;
    }
  }
  return true;
}
