// Chat between two consoles over the simulated radio, and the contact list.
//   chat_test unit
//   chat_test <rx|tx> <index> <loss%> <seconds>
// tx waits until the radio is up, then: pings, sends one message to rx (and waits for the
// acknowledgement), sends one to everyone, sends one to an id nobody has. rx counts what arrives.
#include "chat.h"
#include "contacts.h"
#include "device.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "platform.h"
void air_sleep_us(unsigned long long);
extern int g_index, g_port, g_loss; extern uint32_t g_chip; void air_pump();

static int fails = 0;
#define CHECK(c, msg) do { bool ok_ = (c); printf("  %s %s\n", ok_ ? "ok  " : "FAIL", msg); if (!ok_) fails++; } while (0)

static const char* names(int i) { static const char* N[] = {"SNAKE", "TETRIS", nullptr}; return i >= 0 && i < 3 ? N[i] : nullptr; }

static int unit() {
  EEPROM.begin(512);
  g_chip = 0x1A2B3C;
  char a[32];
  printf("ids\n");
  uint8_t id[3] = {0x12, 0x34, 0x56}, back[3];
  char text[LINK_ID_LEN + 1];
  linkIdToText(id, text);
  CHECK(linkIdFromText(text, back) && !memcmp(id, back, 3), "an id written as text reads back");
  char lower[LINK_ID_LEN + 1]; for (int i = 0; i < 6; i++) lower[i] = (text[i] >= 'A' && text[i] <= 'Z') ? text[i] + 32 : text[i];
  CHECK(linkIdFromText(lower, back) && !memcmp(id, back, 3), "lower case is accepted");
  CHECK(!linkIdFromText("ABCD", back) && !linkIdFromText("ABCDEF", back), "the wrong length is refused");
  CHECK(!linkIdFromText("AB0DE", back) && !linkIdFromText("ABIDE", back), "0, 1, I and O are refused (they are not in the alphabet)");
  CHECK(!linkIdFromText("ZZZZZ", back), "a value above 24 bits is refused");

  printf("aliases\n");
  strcpy(a, "  dad  "); CHECK(!contactCheckAlias(a) && !strcmp(a, "DAD"), "trimmed and capitalised");
  strcpy(a, "ABCDEFGHIJ"); CHECK(!contactCheckAlias(a), "10 characters fit");
  strcpy(a, "ABCDEFGHIJK"); CHECK(contactCheckAlias(a) != nullptr, "11 characters are too long (would crowd the screen)");
  strcpy(a, ""); CHECK(contactCheckAlias(a) != nullptr, "empty is refused");
  strcpy(a, "   "); CHECK(contactCheckAlias(a) != nullptr, "blank is refused");
  strcpy(a, "A<B"); CHECK(contactCheckAlias(a) != nullptr, "characters that need escaping are refused");
  strcpy(a, "A\"B"); CHECK(contactCheckAlias(a) != nullptr, "a quote is refused");
  strcpy(a, "MR. X-1!"); CHECK(!contactCheckAlias(a), "ordinary punctuation is allowed");

  printf("contacts\n");
  contactsClear();
  CHECK(contactCount() == 0 && contactAlias(id) == nullptr, "starts empty");
  strcpy(a, "dad"); CHECK(!contactSet(id, a) && !strcmp(contactAlias(id), "DAD"), "a contact is added");
  strcpy(a, "papa"); CHECK(!contactSet(id, a) && contactCount() == 1 && !strcmp(contactAlias(id), "PAPA"), "the same id gets a new alias, not a second entry");
  initContacts();
  CHECK(contactCount() == 1 && !strcmp(contactAlias(id), "PAPA"), "it survives a restart (read back from the EEPROM)");
  uint8_t own[3]; linkOwnIdBytes(own);
  strcpy(a, "me"); CHECK(contactSet(own, a) != nullptr, "this console's own id is refused");
  for (int i = 0; i < 7; i++) { uint8_t o[3] = {1, 2, (uint8_t)(10 + i)}; strcpy(a, "X"); contactSet(o, a); }
  uint8_t extra[3] = {9, 9, 9}; strcpy(a, "FULL"); CHECK(contactSet(extra, a) != nullptr && contactCount() == 8, "the 9th contact is refused");
  CHECK(contactRemove(id) && contactCount() == 7 && !contactAlias(id), "a contact is removed");
  contactsClear(); CHECK(contactCount() == 0, "all contacts can be cleared");

  printf("messages\n");
  chatSetGameNames(3, names);
  ChatMsg m = {}; char out[64];
  m.code = CM_PLAY_GAME; m.arg = 1; chatFormat(m, out, sizeof(out)); CHECK(!strcmp(out, "LET'S PLAY TETRIS"), "LET'S PLAY names the game from the list");
  m.arg = 2; chatFormat(m, out, sizeof(out)); CHECK(!strcmp(out, "LET'S PLAY A GAME"), "a game that is not in the list reads as A GAME");
  m.arg = 40; chatFormat(m, out, sizeof(out)); CHECK(!strcmp(out, "LET'S PLAY A GAME"), "so does an unknown index (a newer firmware)");
  m.code = CM_BEAT_SCORE; m.arg = 0; m.val = 12345; chatFormat(m, out, sizeof(out)); CHECK(!strcmp(out, "BEAT MY SCORE IN SNAKE: 12345"), "the score is part of the text");
  m.code = 200; chatFormat(m, out, sizeof(out)); CHECK(strstr(out, "CAN'T READ") != nullptr, "an unknown message code is shown as unreadable");
  for (int c = 0; c < CM_COUNT; c++) { m.code = c; chatFormat(m, out, sizeof(out)); if (strlen(out) > 63 || !out[0]) { CHECK(false, "every message formats"); } }
  CHECK(chatCodeNeedsGame(CM_PLAY_GAME) && chatCodeNeedsGame(CM_BEAT_SCORE) && !chatCodeNeedsGame(CM_GG), "two messages take a game");

  printf("%s\n", fails ? "UNIT TESTS FAILED" : "unit tests passed");
  return fails ? 1 : 0;
}

// chat_test <rx|tx> <index> <loss%> <seconds> handoff
// Chat has the radio, then a two-console session takes it (tx joins rx), a message goes across
// during the session, the session ends, and chat has to be back on its own for the third message.
static int handoff(bool rx, int secs) {
  initContacts(); initChat(); chatSetGameNames(3, names);
  uint8_t rxId[3] = {0x1A, 0x2B, 0x3C};
  unsigned long t0 = millis(); int step = 0; unsigned long stepAt = 0; int lastCount = 0;
  while (millis() - t0 < (unsigned long)secs * 1000) {
    air_pump();
    unsigned long now = millis();
    updateChat(false);
    if (step >= 2 && step <= 4) gameLink.poll();   // the session's owner polls the radio, as a game does
    if (rx && chatInboxCount() != lastCount) {
      lastCount = chatInboxCount(); char text[64]; chatFormat(chatInboxAt(0), text, sizeof text);
      printf("[rx] inbox %d: %s\n", lastCount, text); fflush(stdout);
    }
    LinkState ls = gameLink.state();
    switch (step) {
      case 0:   // chat has the radio; tx sends the first message when it sees rx
        if (!rx && ls == LINK_IDLE && gameLink.gameTag() == LINK_TAG_CHAT) { chatPing(); step = 1; stepAt = now; }
        else if (rx && now - t0 > 5000) step = 2;
        break;
      case 1:
        if (chatNearbyCount() == 0 && now - stepAt > 300) { chatPing(); stepAt = now; }
        else if (chatNearbyCount() > 0 && chatSendState() == SEND_IDLE) { chatSend(rxId, CM_HELLO, 0, 0); stepAt = now; }
        else if (chatSendState() == SEND_DELIVERED) { printf("[tx] 1st: DELIVERED\n"); chatSendClear(); step = 2; stepAt = now; }
        break;
      case 2:   // a session takes the radio
        if (now - stepAt < 800 && !rx) break;
        {   // begin() takes the radio over from chat by itself, as the lobbies rely on
          gameLink.begin(5);
          if (rx) gameLink.host(); else gameLink.scan();
          step = 3; stepAt = now;
        }
        break;
      case 3:
        if (!rx && ls == LINK_SCANNING && gameLink.hostCount() > 0) gameLink.join(0);
        if (rx && ls == LINK_REQUESTED) gameLink.accept();
        if (ls == LINK_CONNECTED) { printf("[%s] session up, chat tag now %d\n", rx ? "rx" : "tx", gameLink.gameTag()); step = 4; stepAt = now; }
        break;
      case 4:   // chat inside the session
        if (!rx && chatSendState() == SEND_IDLE && now - stepAt > 500) chatSend(rxId, CM_YES, 0, 0);
        if (!rx && chatSendState() == SEND_DELIVERED) { printf("[tx] 2nd (in session): DELIVERED\n"); chatSendClear(); step = 5; stepAt = now; }
        if (rx && now - stepAt > 4000) step = 5;
        break;
      case 5:   // the session ends: chat must start again by itself
        gameLink.end(); step = 6; stepAt = now;
        break;
      case 6:
        if (!rx && ls == LINK_IDLE && gameLink.gameTag() == LINK_TAG_CHAT && now - stepAt > 1200) { chatSend(rxId, CM_BYE, 0, 0); step = 7; stepAt = now; }
        else if (rx && now - stepAt > 6000) step = 8;
        break;
      case 7:
        if (chatSendState() == SEND_DELIVERED) { printf("[tx] 3rd (after the session): DELIVERED\n"); step = 8; stepAt = now; }
        else if (chatSendState() == SEND_FAILED) { printf("[tx] 3rd: FAILED\n"); step = 8; }
        break;
      case 8:
        if (now - stepAt > 600) { printf("[%s] done\n", rx ? "rx" : "tx"); return 0; }
        break;
    }
    air_sleep_us(16000);
  }
  printf("[%s] timed out at step %d\n", rx ? "rx" : "tx", step);
  return 1;
}

int main(int argc, char** argv) {
  if (!strcmp(argv[1], "unit")) return unit();
  bool rx = !strcmp(argv[1], "rx"); g_index = atoi(argv[2]); g_loss = atoi(argv[3]); int secs = atoi(argv[4]);
  g_chip = rx ? 0x1A2B3C : 0x00FF01; srand(plat::processId() * 7 + g_index);
  EEPROM.begin(512);
  if (argc > 5 && !strcmp(argv[5], "handoff")) return handoff(rx, secs);
  initContacts(); initChat(); chatSetGameNames(3, names);
  uint8_t rxId[3] = {0x1A, 0x2B, 0x3C}, nobody[3] = {0x77, 0x77, 0x77};
  if (rx) { uint8_t txId[3] = {0x00, 0xFF, 0x01}; char al[16] = "SIS"; contactSet(txId, al); }
  chatMenuOpen(true);
  unsigned long t0 = millis(); int step = 0; unsigned long stepAt = 0; int lastCount = 0;
  while (millis() - t0 < (unsigned long)secs * 1000) {
    air_pump();
    updateChat(false);
    if (rx) {
      if (chatInboxCount() != lastCount) {
        lastCount = chatInboxCount(); const ChatMsg& m = chatInboxAt(0); char who[16], text[64];
        chatWho(m.from, who); chatFormat(m, text, sizeof text);
        printf("[rx] inbox %d: from %s: %s (unread %d)\n", lastCount, who, text, chatUnread()); fflush(stdout);
      }
    } else if (gameLink.state() != LINK_OFF) {
      unsigned long now = millis();
      if (step == 0) { chatPing(); step = 1; stepAt = now; }
      else if (step == 1 && chatNearbyCount() == 0 && now - stepAt > 300) { chatPing(); stepAt = now; }   // like the picker, which asks every two seconds
      else if (step == 1 && chatNearbyCount() > 0) {
        printf("[tx] nearby %d\n", chatNearbyCount());
        uint8_t id[3]; if (chatNearbyAt(0, id)) printf("[tx] nearby id matches rx: %d\n", !memcmp(id, rxId, 3));
        chatSend(rxId, CM_BEAT_SCORE, 1, 4321); step = 2; stepAt = now; }
      else if (step == 2 && chatSendState() != SEND_WAIT) { printf("[tx] to rx: %s\n", chatSendState() == SEND_DELIVERED ? "DELIVERED" : "FAILED"); chatSend(nullptr, CM_HELLO, 0, 0); step = 3; }
      else if (step == 3 && chatSendState() != SEND_WAIT) { printf("[tx] to all: %s\n", chatSendState() == SEND_SENT ? "SENT" : "?"); chatSend(nobody, CM_GG, 0, 0); step = 4; }
      else if (step == 4 && chatSendState() != SEND_WAIT) { printf("[tx] to nobody: %s\n", chatSendState() == SEND_FAILED ? "NO REPLY" : "?"); step = 5; stepAt = now; }
      else if (step == 5 && now - stepAt > 800) break;
    }
    air_sleep_us(16000);
  }
  printf("[%s] done\n", argv[1]);
}
