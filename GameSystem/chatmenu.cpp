#include "chatmenu.h"
#include "display.h"
#include "input.h"
#include "highscore.h"

ChatMenu chatMenu;

static const uint8_t MAIN_ROWS = 2;   // INBOX, NEW MESSAGE

static bool sameId(const uint8_t* a, const uint8_t* b) { return memcmp(a, b, 3) == 0; }

static void ageText(unsigned long at, char* out, size_t n) {
  unsigned long s = (millis() - at) / 1000;
  if (s < 10) snprintf(out, n, "NOW");
  else if (s < 60) snprintf(out, n, "%lus", s);
  else if (s < 3600) snprintf(out, n, "%lum", s / 60);
  else snprintf(out, n, "%luh", s / 3600);
}

// The games that can be named: the list the manager registered, whatever it holds today
static int gameCount() {
  int n = 0;
  for (int i = 0; i < chatGameTotal(); i++) if (chatGameName(i)) n++;
  return n;
}
static int gameIdAt(int k) {
  for (int i = 0; i < chatGameTotal(); i++) if (chatGameName(i) && k-- == 0) return i;
  return -1;
}

const char* ChatMenu::menuNote() {
  static char note[12];
  int n = chatUnread();
  if (n > 0) snprintf(note, sizeof(note), "%d NEW", n);
  else snprintf(note, sizeof(note), "MESSAGES");
  return note;
}

void ChatMenu::init() {
  mode = M_MAIN;
  exitRequested = false;
  sel = 0;
  notice = nullptr;
  chatMenuOpen(true);
}

void ChatMenu::stop() {
  chatMenuOpen(false);
  chatSendClear();
}

void ChatMenu::go(Mode m) {
  mode = m;
  sel = 0;
  if (m == M_TO) lastPing = 0;   // ask who is around straight away
}

// Everyone, then the saved contacts, then consoles that answered and are not saved
void ChatMenu::buildTargets() {
  uint8_t selected[3];
  bool hadSel = mode == M_TO && sel < targetCount;
  bool selAll = hadSel && targets[sel].all;
  if (hadSel) memcpy(selected, targets[sel].id, 3);

  targetCount = 0;
  targets[targetCount++] = {{0, 0, 0}, true, false, false};
  for (int i = 0; i < MAX_CONTACTS; i++) {
    uint8_t id[3];
    const char* alias;
    if (!contactAt(i, id, &alias)) break;
    Target t = {{id[0], id[1], id[2]}, false, true, false};
    targets[targetCount++] = t;
  }
  for (int i = 0; i < CHAT_NEARBY; i++) {
    uint8_t id[3];
    if (!chatNearbyAt(i, id)) break;
    bool found = false;
    for (int k = 1; k < targetCount; k++) if (sameId(targets[k].id, id)) { targets[k].near = true; found = true; }
    if (!found) targets[targetCount++] = {{id[0], id[1], id[2]}, false, false, true};
  }
  if (hadSel) {   // the list moved: keep the selection on the same console
    for (int k = 0; k < targetCount; k++) {
      if ((selAll && targets[k].all) || (!selAll && !targets[k].all && sameId(targets[k].id, selected))) sel = k;
    }
  }
  if (sel >= targetCount) sel = targetCount - 1;
}

int ChatMenu::rows() {
  switch (mode) {
    case M_MAIN: return MAIN_ROWS;
    case M_INBOX: return chatInboxCount();
    case M_TO: return targetCount;
    case M_MSG: return CM_COUNT;
    case M_GAME: return gameCount();
    default: return 0;
  }
}

void ChatMenu::row(int i, char* left, size_t ln, char* right, size_t rn, bool* mark) {
  left[0] = right[0] = 0;
  *mark = false;
  switch (mode) {
    case M_MAIN:
      if (i == 0) {
        snprintf(left, ln, "INBOX");
        if (chatUnread()) snprintf(right, rn, "%d NEW", chatUnread());
        else if (chatInboxCount()) snprintf(right, rn, "%d", chatInboxCount());
      } else {
        snprintf(left, ln, "NEW MESSAGE");
      }
      break;
    case M_INBOX: {
      const ChatMsg& m = chatInboxAt(i);
      char who[ALIAS_MAX + 1];
      chatWho(m.from, who);
      snprintf(left, ln, "%s", who);
      ageText(m.at, right, rn);
      *mark = !m.read;
      break;
    }
    case M_TO: {
      const Target& t = targets[i];
      if (t.all) snprintf(left, ln, "EVERYONE");
      else chatWho(t.id, left);
      if (t.near) snprintf(right, rn, "NEAR");
      break;
    }
    case M_MSG:
      chatLabel(i, left, ln);
      break;
    case M_GAME: {
      int g = gameIdAt(i);
      snprintf(left, ln, "%s", chatGameName(g));
      if (code == CM_BEAT_SCORE) snprintf(right, rn, "%d", getGameHighscore(g));
      break;
    }
    default:
      break;
  }
}

void ChatMenu::startSend() {
  uint32_t score = code == CM_BEAT_SCORE ? (uint32_t)getGameHighscore(game) : 0;
  chatSendClear();
  if (!chatSend(toAll ? nullptr : toId, code, game, score)) {   // the radio is not up (yet, or the Wi-Fi page has it)
    notice = "RADIO NOT READY";
    noticeUntil = millis() + 1500;
    return;
  }
  sentAt = millis();
  mode = M_SEND;
}

void ChatMenu::update() {
  unsigned long now = millis();
  if (mode == M_TO && now - lastPing >= 2000) {
    chatPing();
    lastPing = now;
  }
  if (mode == M_TO) buildTargets();

  switch (mode) {
    case M_READ: {
      int n = chatInboxCount();
      if (buttons.upPressed && readIndex > 0) readIndex--;
      else if (buttons.downPressed && readIndex + 1 < n) readIndex++;
      else if (buttons.leftPressed) { sel = readIndex; mode = M_INBOX; break; }
      else if (buttons.rightPressed) {   // reply
        toAll = false;
        memcpy(toId, chatInboxAt(readIndex).from, 3);
        go(M_MSG);
        break;
      }
      chatMarkRead(readIndex);
      break;
    }

    case M_SEND:
      if (chatSendState() == SEND_WAIT) break;
      if (buttons.leftPressed) { chatSendClear(); go(M_MAIN); }
      else if (buttons.rightPressed) { chatSendClear(); go(M_MSG); }   // another message to the same console
      break;

    default: {
      int n = rows();
      if (n > 0 && buttons.downPressed) sel = (sel + 1) % n;
      else if (n > 0 && buttons.upPressed) sel = (sel + n - 1) % n;
      else if (buttons.leftPressed) {
        switch (mode) {
          case M_MAIN: exitRequested = true; break;
          case M_INBOX: go(M_MAIN); break;
          case M_TO: go(M_MAIN); sel = 1; break;
          case M_MSG: go(M_TO); break;
          case M_GAME: go(M_MSG); sel = code; break;
          default: break;
        }
      } else if (buttons.rightPressed && n > 0) {
        switch (mode) {
          case M_MAIN:
            if (sel == 0) go(M_INBOX); else { buildTargets(); go(M_TO); }
            break;
          case M_INBOX:
            readIndex = sel;
            chatMarkRead(readIndex);
            mode = M_READ;
            break;
          case M_TO:
            toAll = targets[sel].all;
            memcpy(toId, targets[sel].id, 3);
            go(M_MSG);
            break;
          case M_MSG:
            code = sel;
            if (chatCodeNeedsGame(code)) go(M_GAME);
            else startSend();
            break;
          case M_GAME:
            game = gameIdAt(sel);
            if (code == CM_BEAT_SCORE && getGameHighscore(game) <= 0) {
              notice = "PLAY IT FIRST";
              noticeUntil = now + 1500;
            } else {
              startSend();
            }
            break;
          default: break;
        }
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------
void ChatMenu::draw() {
  switch (mode) {
    case M_READ: drawRead(); return;
    case M_SEND: drawSend(); return;
    case M_MAIN: {
      char id[LINK_ID_LEN + 1];
      linkOwnId(id);
      char me[16];
      snprintf(me, sizeof(me), "ID %s", id);
      drawList("CHAT", me, "UD:PICK R:OK L:EXIT");
      return;
    }
    case M_INBOX:
      drawList("INBOX", "", chatInboxCount() ? "UD:PICK R:READ L:BACK" : "L:BACK");
      return;
    case M_TO: drawList("SEND TO", chatNearbyCount() ? "" : "LOOKING...", "UD:PICK R:OK L:BACK"); return;
    case M_MSG: drawList("SAY", "", "UD:PICK R:SEND L:BACK"); return;
    case M_GAME: drawList("WHICH GAME", "", "UD:PICK R:SEND L:BACK"); return;
  }
}

void ChatMenu::drawList(const char* title, const char* titleRight, const char* hint) {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(title);
  // keep the title clear of the notification blip in the corner
  if (titleRight[0]) {
    int w = 6 * strlen(titleRight);
    display.setCursor(SCREEN_WIDTH - 18 - w, 0);
    display.print(titleRight);
  }

  int n = rows();
  const int visible = 4;   // the rows start below the corner where the blip shows
  int first = sel < visible ? 0 : sel - visible + 1;
  if (n == 0) {
    drawCenteredText(mode == M_INBOX ? "NO MESSAGES" : "NOTHING HERE", 30, 1);
  }
  for (int r = 0; r < visible && first + r < n; r++) {
    char left[28], right[16];
    bool mark;
    row(first + r, left, sizeof(left), right, sizeof(right), &mark);
    int y = 15 + r * 9;
    bool on = first + r == sel;
    if (on) display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
    display.setTextColor(on ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(mark ? 8 : 2, y);
    if (mark) { display.setCursor(1, y); display.print('*'); display.setCursor(8, y); }
    display.print(left);
    if (right[0]) {
      display.setCursor(SCREEN_WIDTH - 2 - 6 * strlen(right), y);
      display.print(right);
    }
  }
  display.setTextColor(SSD1306_WHITE);
  if (notice && millis() < noticeUntil) {
    display.fillRect(10, 22, 108, 20, SSD1306_BLACK);
    display.drawRect(10, 22, 108, 20, SSD1306_WHITE);
    drawCenteredText(notice, 29, 1);
  } else {
    display.setCursor(0, 56);
    display.print(hint);
  }
}

// Breaks text into lines of at most `width` characters, at spaces where it can
static int wrap(const char* text, char lines[][24], int maxLines, int width) {
  int count = 0;
  const char* p = text;
  while (*p && count < maxLines) {
    int len = strlen(p);
    if (len <= width) { strcpy(lines[count++], p); break; }
    int cut = width;
    while (cut > 0 && p[cut] != ' ') cut--;
    if (cut == 0) cut = width;
    memcpy(lines[count], p, cut);
    lines[count][cut] = 0;
    count++;
    p += cut;
    while (*p == ' ') p++;
  }
  return count;
}

void ChatMenu::drawRead() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  const ChatMsg& m = chatInboxAt(readIndex);
  char who[ALIAS_MAX + 1], age[8], line[28];
  chatWho(m.from, who);
  ageText(m.at, age, sizeof(age));
  snprintf(line, sizeof(line), "FROM %s", who);
  display.setCursor(0, 0);
  display.print(line);
  snprintf(line, sizeof(line), "%d/%d", readIndex + 1, chatInboxCount());
  display.setCursor(SCREEN_WIDTH - 18 - 6 * strlen(line), 0);
  display.print(line);
  display.setCursor(0, 9);
  display.print(age);
  display.drawFastHLine(0, 18, SCREEN_WIDTH, SSD1306_WHITE);

  char text[64];
  chatFormat(m, text, sizeof(text));
  char lines[4][24];
  int n = wrap(text, lines, 4, 21);
  for (int i = 0; i < n; i++) { display.setCursor(0, 22 + i * 9); display.print(lines[i]); }
  display.setCursor(0, 56);
  display.print(chatInboxCount() > 1 ? F("UD R:REPLY L:BACK") : F("R:REPLY L:BACK"));
}

void ChatMenu::drawSend() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char who[ALIAS_MAX + 1], line[28];
  if (toAll) snprintf(who, sizeof(who), "EVERYONE");
  else chatWho(toId, who);
  snprintf(line, sizeof(line), "TO %s", who);
  display.setCursor(0, 0);
  display.print(line);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  ChatMsg m = {};
  m.code = code;
  m.arg = game;
  m.val = code == CM_BEAT_SCORE ? (uint32_t)getGameHighscore(game) : 0;
  char text[64];
  chatFormat(m, text, sizeof(text));
  char lines[3][24];
  int n = wrap(text, lines, 3, 21);
  for (int i = 0; i < n; i++) { display.setCursor(0, 13 + i * 9); display.print(lines[i]); }

  const char* result = "";
  switch (chatSendState()) {
    case SEND_WAIT: result = ((millis() / 300) % 4 == 0) ? "SENDING" : ((millis() / 300) % 4 == 1) ? "SENDING." : ((millis() / 300) % 4 == 2) ? "SENDING.." : "SENDING..."; break;
    case SEND_DELIVERED: result = "DELIVERED"; break;
    case SEND_SENT: result = "SENT"; break;
    case SEND_FAILED: result = "NO REPLY"; break;
    default: break;
  }
  drawCenteredText(result, 42, 1);
  if (chatSendState() != SEND_WAIT) {
    display.setCursor(0, 56);
    display.print(F("R:ANOTHER L:DONE"));
  }
}
