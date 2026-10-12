#ifndef LOBBY_H
#define LOBBY_H

#include "config.h"
#include "gamelink.h"

// The screens for starting a two-console game, shared by every versus game:
// choose HOST GAME or JOIN GAME, the host sees join requests and accepts or
// denies, the joiner picks a host by its id. Once connected, update() reports
// which side this console is and the game takes over the link (gameLink).
//
//   lobby.begin(GAME_PONG, "PONG");
//   each frame:  switch (lobby.update()) { ... }   and   lobby.draw();
//   when the game ends or is left:  lobby.end();

enum LobbyResult : uint8_t {
  LOBBY_BUSY,     // still showing screens
  LOBBY_HOST,     // connected, this console runs the game
  LOBBY_CLIENT,   // connected, the other console runs the game
  LOBBY_BACK      // the player left, or a notice was dismissed: go back to the main menu
};

class Lobby {
public:
  void begin(uint8_t gameTag, const char* gameTitle);
  // Optional wording for the screens (defaults suit two-player games)
  void setWording(const char* hostChoice, const char* joinChoice, const char* hostTitle,
                  const char* hostLine1, const char* findTitle, const char* requestLine);
  LobbyResult update();
  void draw();   // does not present; the manager does
  // Shows a message (for example a lost connection); update() returns
  // LOBBY_BACK once it is dismissed.
  void fail(const char* title, const char* line1, const char* line2);
  void end();    // radio off; safe to call at any time

private:
  enum Phase : uint8_t { PH_ROLE, PH_HOST, PH_JOIN, PH_NOTICE, PH_PLAYING };
  Phase phase = PH_ROLE;
  uint8_t tag = 0;
  const char* title = "";
  uint8_t roleChoice = 0;   // 0 = host, 1 = join
  int joinIndex = 0;
  bool radioOn = false;
  const char* hostChoice = "HOST GAME";
  const char* joinChoice = "JOIN GAME";
  const char* hostTitle = "HOSTING GAME";
  const char* hostLine1 = "Waiting for a player";
  const char* findTitle = "FIND GAME";
  const char* requestLine = "wants to play";
  bool noticeRescan = false;   // after the notice: look for hosts again (else leave)
  const char* noticeTitle = "";
  const char* noticeLine1 = "";
  const char* noticeLine2 = "";

  void notice(const char* t, const char* l1, const char* l2, bool rescan);
};

#endif
