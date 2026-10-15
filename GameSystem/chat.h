#ifndef CHAT_H
#define CHAT_H

#include "config.h"
#include "gamelink.h"

// Console-to-console chat: ready-made messages sent over ESP-NOW (no router).
//
// A chat packet is one broadcast, authenticated with the link secret like the pairing
// packets (gamelink.h), that names its sender and its receiver by id, so any console in range
// can hear it but only the named one keeps it (or everyone, for "to all"). Messages to one
// console are acknowledged, and the sender says DELIVERED or NO REPLY. A console that is
// not listening (chat off in SETTINGS, or the radio in use for the Wi-Fi pages) does not
// answer. Nothing is encrypted: the messages are a fixed list of phrases, and the secret
// only keeps strangers' consoles out, like everywhere else on the link.
//
// The radio: while chat is on and nothing else has the radio, chat keeps it open and
// listening (updateChat() from the main loop). Every other user of the radio just takes it
// (GameLink::begin()) and gives it back (end()), and chat packets keep arriving in all
// states, because the link hands them to chat from whichever poll() runs.
//
// Incoming messages go to a small inbox in RAM (the last CHAT_INBOX); unread ones make the
// blip appear in the corner of the screen (display.h).

#define CHAT_INBOX 8
#define CHAT_NEARBY 6
#define CHAT_NEARBY_MS 30000UL    // a console heard this recently counts as nearby

enum ChatCode : uint8_t {
  CM_PLAY_GAME,     // "LET'S PLAY <game>"
  CM_BEAT_SCORE,    // "BEAT MY SCORE IN <game>: <score>"
  CM_WANNA_PLAY,
  CM_REMATCH,
  CM_GG,
  CM_NICE,
  CM_YES,
  CM_NO,
  CM_LATER,
  CM_WAIT,
  CM_HELLO,
  CM_BYE,
  CM_COUNT
};

struct ChatMsg {
  uint8_t from[3];
  uint8_t code;
  uint8_t arg;          // the game, for the two messages that name one
  uint32_t val;         // the score
  unsigned long at;     // millis() when it arrived
  bool read;
};

enum ChatSendState : uint8_t {
  SEND_IDLE,
  SEND_WAIT,        // sent, waiting for the acknowledgement
  SEND_DELIVERED,
  SEND_SENT,        // to everyone: no acknowledgement to wait for
  SEND_FAILED       // nobody answered
};

// The game list for "<game>": chat asks for the name of game i (nullptr = not a game that can be named)
void chatSetGameNames(int total, const char* (*nameOf)(int));
const char* chatGameName(int i);
int chatGameTotal();

void initChat();                              // after the EEPROM and the contacts
void updateChat(bool radioBlocked);           // once per frame; radioBlocked: the Wi-Fi pages need the radio
void chatMenuOpen(bool open);                 // the CHAT tile is open: the radio is on even if listening is off
void chatReleaseRadio();                      // gives the radio up before something else switches it to Wi-Fi

// Messages
bool chatCodeNeedsGame(uint8_t code);
bool chatCodeHasScore(uint8_t code);
void chatLabel(uint8_t code, char* out, size_t n);          // short, for a list: "LET'S PLAY..."
void chatFormat(const ChatMsg& m, char* out, size_t n);     // the whole text
void chatWho(const uint8_t id[3], char* out);               // the alias, or the id; out holds ALIAS_MAX + 1

// Inbox, newest first
int chatInboxCount();
const ChatMsg& chatInboxAt(int i);
void chatMarkRead(int i);
int chatUnread();

// Sending. to == nullptr sends to everyone.
bool chatSend(const uint8_t* to, uint8_t code, uint8_t arg, uint32_t val);
ChatSendState chatSendState();
void chatSendClear();

// Who is around: consoles heard within CHAT_NEARBY_MS. chatPing() asks the ones in range to say so.
void chatPing();
int chatNearbyCount();
bool chatNearbyAt(int i, uint8_t id[3]);

#endif
