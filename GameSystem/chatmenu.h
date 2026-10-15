#ifndef CHATMENU_H
#define CHATMENU_H

#include "config.h"
#include "game.h"
#include "chat.h"
#include "contacts.h"

// CHAT: the tile after VIDEOS. Send ready-made messages to another console and read the ones
// that came in. Lists scroll with UP/DOWN, RIGHT selects, LEFT goes back (and leaves from the
// first list).
//
//   INBOX        the last messages; UP/DOWN in a message steps to the next, RIGHT replies
//   NEW MESSAGE  1. to whom: everyone, a saved contact, or a console that answered a ping
//                2. what: a phrase (two of them name a game, taken from the game list)
//                3. the result: DELIVERED, SENT (to everyone) or NO REPLY
class ChatMenu : public Game {
public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return false; }
  int getScore() override { return 0; }
  const char* getName() override { return "CHAT"; }
  bool hasScore() override { return false; }
  const char* menuNote() override;
  bool wantsExit() override { return exitRequested; }
  bool keepsScreenOn() override { return mode == M_SEND; }
  void stop() override;

private:
  enum Mode : uint8_t { M_MAIN, M_INBOX, M_READ, M_TO, M_MSG, M_GAME, M_SEND };
  Mode mode = M_MAIN;
  bool exitRequested = false;
  uint8_t sel = 0;                 // the selected row of the current list
  uint8_t readIndex = 0;           // the message being read
  // the recipient and what is being sent
  bool toAll = false;
  uint8_t toId[3];
  uint8_t code = 0;
  uint8_t game = 0;
  // the recipient list, rebuilt every frame
  struct Target { uint8_t id[3]; bool all; bool saved; bool near; };
  Target targets[1 + MAX_CONTACTS + CHAT_NEARBY];
  uint8_t targetCount = 0;
  unsigned long lastPing = 0, noticeUntil = 0, sentAt = 0;
  const char* notice = nullptr;

  void buildTargets();
  int rows();
  void row(int i, char* left, size_t leftN, char* right, size_t rightN, bool* mark);
  void go(Mode m);
  void startSend();
  void drawList(const char* title, const char* titleRight, const char* hint);
  void drawRead();
  void drawSend();
};

extern ChatMenu chatMenu;

#endif
