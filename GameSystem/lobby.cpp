#include "lobby.h"
#include "input.h"
#include "ui.h"
#include "display.h"

void Lobby::begin(uint8_t gameTag, const char* gameTitle) {
  end();
  tag = gameTag;
  title = gameTitle;
  roleChoice = 0;
  joinIndex = 0;
  phase = PH_ROLE;
}

void Lobby::end() {
  if (!radioOn) return;
  gameLink.end();
  radioOn = false;
}

void Lobby::notice(const char* t, const char* l1, const char* l2, bool rescan) {
  noticeTitle = t;
  noticeLine1 = l1;
  noticeLine2 = l2;
  noticeRescan = rescan;
  phase = PH_NOTICE;
}

void Lobby::fail(const char* t, const char* l1, const char* l2) {
  end();
  notice(t, l1, l2, false);
}

LobbyResult Lobby::update() {
  bool step = buttons.leftPressed || buttons.rightPressed;
  int dir = buttons.rightPressed ? 1 : -1;

  switch (phase) {
    case PH_ROLE:
      if (step) roleChoice ^= 1;
      if (buttons.upPressed) {
        if (!gameLink.begin(tag)) {
          notice("RADIO ERROR", "Could not start", "the link", false);
          break;
        }
        radioOn = true;
        if (roleChoice == 0) {
          gameLink.host();
          phase = PH_HOST;
        } else {
          gameLink.scan();
          joinIndex = 0;
          phase = PH_JOIN;
        }
      }
      else if (buttons.downPressed) return LOBBY_BACK;
      break;

    case PH_HOST:
      gameLink.poll();
      switch (gameLink.state()) {
        case LINK_REQUESTED:
          if (buttons.upPressed) gameLink.accept();
          else if (buttons.downPressed) gameLink.deny();
          break;
        case LINK_CONNECTED:
          phase = PH_PLAYING;
          return LOBBY_HOST;
        default:
          if (buttons.downPressed) {
            end();
            phase = PH_ROLE;
          }
          break;
      }
      break;

    case PH_JOIN:
      gameLink.poll();
      switch (gameLink.state()) {
        case LINK_SCANNING:
          if (gameLink.hostCount() > 0) {
            if (step) joinIndex = (joinIndex + dir + gameLink.hostCount()) % gameLink.hostCount();
            if (joinIndex >= gameLink.hostCount()) joinIndex = 0;
            if (buttons.upPressed) gameLink.join(joinIndex);
          }
          if (buttons.downPressed) {
            end();
            phase = PH_ROLE;
          }
          break;
        case LINK_ASKING:
          if (buttons.downPressed) {
            gameLink.cancel();
            gameLink.scan();
          }
          break;
        case LINK_DENIED:
          notice("DENIED", "The host said no", "", true);
          break;
        case LINK_BUSY:
          notice("HOST IS BUSY", "Talking to someone", "else. Try again.", true);
          break;
        case LINK_NO_ANSWER:
          notice("NO ANSWER", "The host did not", "reply in time", true);
          break;
        case LINK_CONNECTED:
          phase = PH_PLAYING;
          return LOBBY_CLIENT;
        default:
          break;
      }
      break;

    case PH_NOTICE:
      if (buttons.upPressed) {
        if (noticeRescan && radioOn) {
          gameLink.scan();
          joinIndex = 0;
          phase = PH_JOIN;
        } else {
          return LOBBY_BACK;
        }
      }
      break;

    case PH_PLAYING:
      break;
  }
  return LOBBY_BUSY;
}

void Lobby::draw() {
  char line[24];

  switch (phase) {
    case PH_ROLE:
      linkOwnId(line + 9);
      memcpy(line, "Your ID: ", 9);
      drawChoiceScreen(title, roleChoice == 0 ? "HOST GAME" : "JOIN GAME", line,
                       "UP:OK  DOWN:BACK", true, true);
      break;

    case PH_HOST:
      switch (gameLink.state()) {
        case LINK_REQUESTED:
          drawMessageScreen("JOIN REQUEST", gameLink.peerId(), "wants to play", "", "UP:ACCEPT DOWN:DENY");
          break;
        case LINK_ACCEPTING:
          drawMessageScreen("HOSTING GAME", gameLink.peerId(), "Connecting...", "", "DOWN: CANCEL");
          break;
        default:
          drawMessageScreen("HOSTING GAME", gameLink.localId(), "Waiting for a player", "Tell them your ID", "DOWN: CANCEL");
          break;
      }
      break;

    case PH_JOIN:
      switch (gameLink.state()) {
        case LINK_SCANNING:
          if (gameLink.hostCount() == 0) {
            snprintf(line, sizeof(line), "Your ID: %s", gameLink.localId());
            drawMessageScreen("FIND GAME", "", "Searching for hosts...", line, "DOWN: BACK");
          } else {
            snprintf(line, sizeof(line), "%d of %d", joinIndex + 1, gameLink.hostCount());
            drawChoiceScreen("FIND GAME", gameLink.hostId(joinIndex), line, "UP:JOIN  DOWN:BACK",
                             gameLink.hostCount() > 1, gameLink.hostCount() > 1);
          }
          break;
        default:
          drawMessageScreen("JOINING", gameLink.peerId(), "Waiting for the host", "to accept...", "DOWN: CANCEL");
          break;
      }
      break;

    case PH_NOTICE:
      drawMessageScreen(noticeTitle, "", noticeLine1, noticeLine2, "UP: OK");
      break;

    case PH_PLAYING:
      break;
  }
}
