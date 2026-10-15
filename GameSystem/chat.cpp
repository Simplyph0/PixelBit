#include "chat.h"
#include <ESP8266WiFi.h>
#include "contacts.h"
#include "device.h"
#include "display.h"

// Packet body (inside the link's authenticated lobby packet):
//   [kind, id (2), from (3), to (3)]            9 bytes, every kind
//   MESSAGE adds [code, arg, score (3)]         14 bytes in all
enum Kind : uint8_t { K_MSG = 1, K_ACK = 2, K_PING = 3, K_PONG = 4 };
static const uint8_t HEADER = 9, MSG_LEN = 14;
static const uint8_t ALL[3] = {0xFF, 0xFF, 0xFF};

static const char* const TEXT[CM_COUNT] = {
  "LET'S PLAY", "BEAT MY SCORE", "WANNA PLAY?", "REMATCH?", "GOOD GAME!", "NICE ONE!",
  "YES!", "NO THANKS", "MAYBE LATER", "GIVE ME A MINUTE", "HELLO!", "BYE!"
};

static const unsigned long RESEND_MS = 150;
static const uint8_t SEND_TRIES = 6;
static const unsigned long ACK_WAIT_MS = 500;   // after the last try

static int gameTotal = 0;
static const char* (*gameNameOf)(int) = nullptr;

static ChatMsg inbox[CHAT_INBOX];
static int inboxCount = 0;

struct Near { uint8_t id[3]; unsigned long seen; };
static Near nearby[CHAT_NEARBY];
static int nearbyCount = 0;

struct Seen { uint8_t from[3]; uint16_t id; };
static Seen seen[8];
static uint8_t seenNext = 0;

static uint8_t own[3];
static bool menuOpen = false;
static unsigned long retryAt = 0;
static int lastUnread = 0;

// outgoing
static ChatSendState sendState = SEND_IDLE;
static uint8_t outBody[MSG_LEN];
static uint8_t outTo[3];
static bool outAll = false;
static uint8_t outTries = 0;
static unsigned long outAt = 0;

void chatSetGameNames(int total, const char* (*nameOf)(int)) { gameTotal = total; gameNameOf = nameOf; }
int chatGameTotal() { return gameTotal; }
const char* chatGameName(int i) { return (gameNameOf && i >= 0 && i < gameTotal) ? gameNameOf(i) : nullptr; }

bool chatCodeNeedsGame(uint8_t c) { return c == CM_PLAY_GAME || c == CM_BEAT_SCORE; }
bool chatCodeHasScore(uint8_t c) { return c == CM_BEAT_SCORE; }

void chatLabel(uint8_t code, char* out, size_t n) {
  if (code >= CM_COUNT) { snprintf(out, n, "?"); return; }
  snprintf(out, n, "%s%s", TEXT[code], chatCodeNeedsGame(code) ? "..." : "");
}

void chatFormat(const ChatMsg& m, char* out, size_t n) {
  if (m.code >= CM_COUNT) { snprintf(out, n, "(A MESSAGE THIS CONSOLE CAN'T READ)"); return; }
  const char* g = chatGameName(m.arg);
  if (!g) g = "A GAME";
  if (m.code == CM_PLAY_GAME) snprintf(out, n, "LET'S PLAY %s", g);
  else if (m.code == CM_BEAT_SCORE) snprintf(out, n, "BEAT MY SCORE IN %s: %lu", g, (unsigned long)m.val);
  else snprintf(out, n, "%s", TEXT[m.code]);
}

void chatWho(const uint8_t id[3], char* out) {
  const char* a = contactAlias(id);
  if (a) { strncpy(out, a, ALIAS_MAX); out[ALIAS_MAX] = 0; }
  else linkIdToText(id, out);   // 5 characters
}

// ---- inbox ----
int chatInboxCount() { return inboxCount; }
const ChatMsg& chatInboxAt(int i) { return inbox[i]; }
void chatMarkRead(int i) { if (i >= 0 && i < inboxCount) inbox[i].read = true; }
int chatUnread() {
  int n = 0;
  for (int i = 0; i < inboxCount; i++) if (!inbox[i].read) n++;
  return n;
}

static void pushInbox(const uint8_t* from, uint8_t code, uint8_t arg, uint32_t val) {
  int keep = inboxCount < CHAT_INBOX ? inboxCount : CHAT_INBOX - 1;
  memmove(&inbox[1], &inbox[0], keep * sizeof(ChatMsg));
  memcpy(inbox[0].from, from, 3);
  inbox[0].code = code;
  inbox[0].arg = arg;
  inbox[0].val = val;
  inbox[0].at = millis();
  inbox[0].read = false;
  if (inboxCount < CHAT_INBOX) inboxCount++;
}

// ---- who is around ----
static void noteNearby(const uint8_t* id) {
  unsigned long now = millis();
  int slot = -1;
  for (int i = 0; i < nearbyCount; i++) if (memcmp(nearby[i].id, id, 3) == 0) slot = i;
  if (slot < 0) {
    if (nearbyCount < CHAT_NEARBY) {
      slot = nearbyCount++;
    } else {   // full: replace the one heard longest ago
      slot = 0;
      for (int i = 1; i < nearbyCount; i++) if (now - nearby[i].seen > now - nearby[slot].seen) slot = i;
    }
    memcpy(nearby[slot].id, id, 3);
  }
  nearby[slot].seen = now;
}

int chatNearbyCount() {
  int n = 0;
  unsigned long now = millis();
  for (int i = 0; i < nearbyCount; i++) if (now - nearby[i].seen < CHAT_NEARBY_MS) n++;
  return n;
}

bool chatNearbyAt(int i, uint8_t id[3]) {
  unsigned long now = millis();
  for (int s = 0; s < nearbyCount; s++) {
    if (now - nearby[s].seen >= CHAT_NEARBY_MS) continue;
    if (i-- == 0) { memcpy(id, nearby[s].id, 3); return true; }
  }
  return false;
}

// ---- packets ----
static uint16_t newId() {
  uint16_t id;
  do { id = ESP.random() & 0xFFFF; } while (id == 0);
  return id;
}

static void header(uint8_t* b, uint8_t kind, uint16_t id, const uint8_t* to) {
  b[0] = kind;
  b[1] = id;
  b[2] = id >> 8;
  memcpy(b + 3, own, 3);
  memcpy(b + 6, to, 3);
}

static void sendSmall(uint8_t kind, uint16_t id, const uint8_t* to) {
  uint8_t b[HEADER];
  header(b, kind, id, to);
  gameLink.sendChat(b, HEADER);
}

static bool seenBefore(const uint8_t* from, uint16_t id) {
  for (int i = 0; i < 8; i++) if (seen[i].id == id && memcmp(seen[i].from, from, 3) == 0) return true;
  return false;
}

static void onChat(const uint8_t* b, uint8_t n) {
  if (n < HEADER) return;
  uint8_t kind = b[0];
  uint16_t id = b[1] | (b[2] << 8);
  const uint8_t* from = b + 3;
  const uint8_t* to = b + 6;
  if (memcmp(from, own, 3) == 0) return;   // our own packet, if the radio ever hands it back
  noteNearby(from);
  bool forMe = memcmp(to, own, 3) == 0;
  bool forAll = memcmp(to, ALL, 3) == 0;

  switch (kind) {
    case K_MSG:
      if (n != MSG_LEN || !(forMe || forAll)) return;
      if (!seenBefore(from, id)) {
        memcpy(seen[seenNext].from, from, 3);
        seen[seenNext].id = id;
        seenNext = (seenNext + 1) % 8;
        pushInbox(from, b[9], b[10], (uint32_t)b[11] | (uint32_t)b[12] << 8 | (uint32_t)b[13] << 16);
      }
      if (forMe) sendSmall(K_ACK, id, from);   // also for a repeat: our first answer may have been lost
      break;

    case K_ACK:
      if (forMe && sendState == SEND_WAIT && !outAll && id == (uint16_t)(outBody[1] | (outBody[2] << 8)) &&
          memcmp(from, outTo, 3) == 0) {
        sendState = SEND_DELIVERED;
      }
      break;

    case K_PING:
      if (forAll) sendSmall(K_PONG, id, from);
      break;

    default:
      break;
  }
}

// ---- sending ----
bool chatSend(const uint8_t* to, uint8_t code, uint8_t arg, uint32_t val) {
  if (gameLink.state() == LINK_OFF || code >= CM_COUNT) return false;
  outAll = to == nullptr;
  memcpy(outTo, outAll ? ALL : to, 3);
  header(outBody, K_MSG, newId(), outTo);
  outBody[9] = code;
  outBody[10] = arg;
  outBody[11] = val;
  outBody[12] = val >> 8;
  outBody[13] = val >> 16;
  outTries = 1;
  outAt = millis();
  sendState = SEND_WAIT;
  gameLink.sendChat(outBody, MSG_LEN);
  return true;
}

ChatSendState chatSendState() { return sendState; }
void chatSendClear() { sendState = SEND_IDLE; }

void chatPing() {
  if (gameLink.state() == LINK_OFF) return;
  sendSmall(K_PING, newId(), ALL);
}

// ---- the radio ----
void initChat() {
  linkOwnIdBytes(own);
  gameLink.setChatHandler(onChat);
  inboxCount = 0;
  nearbyCount = 0;
  memset(seen, 0, sizeof(seen));
  lastUnread = 0;
}

void chatMenuOpen(bool open) { menuOpen = open; }

static bool chatHasRadio() {
  return gameLink.state() == LINK_IDLE && gameLink.gameTag() == LINK_TAG_CHAT;
}

void chatReleaseRadio() {
  if (chatHasRadio()) gameLink.end();
}

void updateChat(bool radioBlocked) {
  unsigned long now = millis();
  bool want = deviceSettings().chat || menuOpen || sendState == SEND_WAIT;
  bool mine = chatHasRadio();

  if (radioBlocked) {
    if (mine) gameLink.end();
  } else if (gameLink.state() == LINK_OFF) {
    if (want && now >= retryAt && !gameLink.begin(LINK_TAG_CHAT)) retryAt = now + 2000;
  } else if (mine && !want) {
    gameLink.end();
    mine = false;
  }
  if (mine) gameLink.poll();   // anyone else with the radio polls it themselves

  if (sendState == SEND_WAIT) {
    if (gameLink.state() == LINK_OFF) {
      sendState = SEND_FAILED;
    } else if (outTries < SEND_TRIES && now - outAt >= RESEND_MS) {
      gameLink.sendChat(outBody, MSG_LEN);
      outTries++;
      outAt = now;
    } else if (outTries >= SEND_TRIES && now - outAt >= (outAll ? RESEND_MS : ACK_WAIT_MS)) {
      sendState = outAll ? SEND_SENT : SEND_FAILED;
    }
  }

  int unread = chatUnread();
  if (unread > lastUnread) {
    notifyAttention();
    wakeScreen();   // a message that arrives while the screen is off should be seen
  }
  lastUnread = unread;
  setNotifyBlip(unread > 0);
}
