#include "tutorial.h"
#include "display.h"
#include "input.h"
#include "gamemanager.h"
#include "ui.h"

Tutorial tutorial;

// What a page waits for before it moves on
enum Need : uint8_t { NEED_NOTHING, NEED_RIGHT, NEED_LEFT, NEED_UP, NEED_BOTH };

// Pad highlights: which of the four buttons the picture shows pressed
enum { PAD_NONE = 0, PAD_U = 1, PAD_D = 2, PAD_L = 4, PAD_R = 8, PAD_SHOW = 16 };

struct Page {
  const char* title;
  const char* line[4];
  const char* hint;
  uint8_t pad;
  Need need;
};

// Page 0 is the welcome screen, drawn separately. Lines are at most 15
// characters where the button picture sits on the right.
static const Page PAGES[] = {
  {"", {"", "", "", ""}, "UP:GO  HOLD ANY:SKIP", PAD_NONE, NEED_NOTHING},
  {"THE MENU", {"Scroll the games", "with LEFT and", "RIGHT. Hold to", "keep scrolling."}, "TRY IT: PRESS RIGHT", PAD_SHOW | PAD_R, NEED_RIGHT},
  {"THE MENU", {"Good! Now press", "LEFT to scroll", "back.", ""}, "TRY IT: PRESS LEFT", PAD_SHOW | PAD_L, NEED_LEFT},
  {"START A GAME", {"UP starts the", "selected game.", "Settings show", "once per game."}, "TRY IT: PRESS UP", PAD_SHOW | PAD_U, NEED_UP},
  {"SETTINGS", {"DOWN changes a", "game's settings.", "Look for vSET", "under the name."}, "UP:NEXT  DOWN:BACK", PAD_SHOW | PAD_D, NEED_NOTHING},
  {"LEAVING A GAME", {"To leave a game,", "hold UP + DOWN", "together for one", "second."}, "TRY IT: HOLD BOTH", PAD_SHOW | PAD_U | PAD_D, NEED_BOTH},
  {"PLAY A FRIEND", {"In a game's settings", "choose 2 PLAYER, then", "pick HOST or JOIN.", "The host can say no."}, "UP:NEXT  DOWN:BACK", PAD_NONE, NEED_NOTHING},
  {"ALL SET!", {"See this tour", "again: hold DOWN", "on the menu for", "one second."}, "UP:FINISH  DOWN:BACK", PAD_SHOW | PAD_D, NEED_NOTHING}
};
static const uint8_t PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);

void Tutorial::begin() {
  page = 0;
  anyHeldSince = 0;
  comboSince = 0;
  armed = false;
}

void Tutorial::go(int to) {
  page = constrain(to, 0, PAGE_COUNT - 1);
  comboSince = 0;
}

TutorialResult Tutorial::update() {
  unsigned long now = millis();

  // Holding any button skips the tour (after the buttons used to open it were let go)
  bool anyHeld = buttons.up || buttons.down || buttons.left || buttons.right;
  if (!anyHeld) armed = true;
  if (anyHeld && armed) {
    if (anyHeldSince == 0) anyHeldSince = now;
    bool onComboPage = PAGES[page].need == NEED_BOTH && buttons.up && buttons.down;
    if (now - anyHeldSince >= TOUR_SKIP_MS && !onComboPage) return TUT_SKIPPED;
  } else {
    anyHeldSince = 0;
  }

  const Page& p = PAGES[page];
  switch (p.need) {
    case NEED_RIGHT: if (buttons.rightPressed) go(page + 1); break;
    case NEED_LEFT:  if (buttons.leftPressed) go(page + 1); break;
    case NEED_UP:    if (buttons.upPressed) go(page + 1); break;
    case NEED_BOTH:
      if (buttons.up && buttons.down) {
        if (comboSince == 0) comboSince = now;
        if (now - comboSince >= EXIT_HOLD_MS) {
          go(page + 1);
          anyHeldSince = 0;   // the keys are still down: do not count that towards a skip
        }
      } else {
        comboSince = 0;
      }
      if (buttons.downPressed && !buttons.up) go(page - 1);
      break;
    case NEED_NOTHING:
      if (buttons.upPressed) {
        if (page == PAGE_COUNT - 1) return TUT_FINISHED;
        go(page + 1);
      } else if (buttons.downPressed && page > 0) {
        go(page - 1);
      }
      break;
  }
  return TUT_RUNNING;
}

// The four buttons as a small diamond; the ones to press are filled
static void drawPad(uint8_t highlight) {
  struct Key { int x, y; uint8_t bit; } keys[4] = {
    {106, 19, PAD_U}, {106, 43, PAD_D}, {96, 31, PAD_L}, {116, 31, PAD_R}
  };
  for (const Key& k : keys) {
    if (highlight & k.bit) display.fillRect(k.x, k.y, 9, 9, SSD1306_WHITE);
    else display.drawRect(k.x, k.y, 9, 9, SSD1306_WHITE);
  }
}

void Tutorial::draw() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  const Page& p = PAGES[page];

  if (page == 0) {
    drawCenteredText(OS_NAME, 6, 3);
    drawCenteredText(OS_VERSION, 34, 1);
    drawCenteredText("Welcome! A quick tour", 44, 1);
    drawCenteredText(p.hint, 56, 1);
    return;
  }

  // Title and page number
  drawCenteredText(p.title, 0, 1);
  char count[8];
  snprintf(count, sizeof(count), "%d/%d", page + 1, PAGE_COUNT);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(count), 0);
  display.print(count);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  for (int i = 0; i < 4; i++) {
    display.setCursor(1, 12 + i * 9);
    display.print(p.line[i]);
  }
  if (p.pad & PAD_SHOW) drawPad(p.pad & 15);

  // Hold-both page: a bar that fills while both buttons are down
  if (p.need == NEED_BOTH) {
    unsigned long held = comboSince ? millis() - comboSince : 0;
    int w = (int)min(held * 90UL / EXIT_HOLD_MS, 90UL);
    display.drawRect(1, 49, 92, 5, SSD1306_WHITE);
    display.fillRect(2, 50, w, 3, SSD1306_WHITE);
  }

  drawCenteredText(p.hint, 56, 1);
}
