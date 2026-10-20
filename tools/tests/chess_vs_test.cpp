// Chess on two consoles over a simulated radio, through the real button flow, and against the computer.
//   chess_vs_test <host|join> <index> <loss%> <seconds>      one side of a two-console game (run both)
//   chess_vs_test ai <level> <max plies>                     one console against the computer
// Both sides play the same scripted fool's mate first (checkmate, the result screen, a rematch with the
// sides swapped), then moves chosen at random. At the end each side says where it stands; chess_check.py
// compares the two.
#define private public
#include "chess.h"
#include "input.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "platform.h"
void air_sleep_us(unsigned long long);
extern int g_index, g_port, g_loss;
extern uint32_t g_chip;
void air_pump();

static uint16_t scriptedMove(const char* s) {
  uint8_t promo = 0;
  return chessMove((s[1] - '1') * 8 + (s[0] - 'a'), (s[3] - '1') * 8 + (s[2] - 'a'), promo);
}

// Presses the buttons that make the move `want` on this console, one press per call
static void driveMove(ChessGame& c, uint16_t want, int frame) {
  const bool press = frame % 2 == 0;                       // a press every other frame, like a finger
  if (c.stage == ChessGame::ST_PIECE) {
    int idx = -1;
    for (int i = 0; i < c.pieceCount; i++) if (c.pieces[i] == chessMoveFrom(want)) idx = i;
    if (idx < 0) return;
    if (c.pieceIdx != idx) { if (press) buttons.rightPressed = true; }
    else if (press) buttons.upPressed = true;
  } else if (c.stage == ChessGame::ST_DEST) {
    int idx = -1;
    for (int i = 0; i < c.destCount; i++) if (chessMoveTo(c.destMove[i]) == chessMoveTo(want)) idx = i;
    if (idx < 0) { if (press) buttons.downPressed = true; return; }
    if (c.destIdx != idx) { if (press) buttons.rightPressed = true; }
    else if (press) buttons.upPressed = true;
  } else if (c.stage == ChessGame::ST_PROMO) {
    // the queen is first in the list; pick a knight now and then so under-promotion is played too
    const int wantIdx = chessMovePromo(want) == CH_N ? 3 : 0;
    if (c.promoIdx != wantIdx) { if (press) buttons.rightPressed = true; }
    else if (press) buttons.upPressed = true;
  }
}

int main(int argc, char** argv) {
  const bool vsAi = !strcmp(argv[1], "ai");
  bool host = !strcmp(argv[1], "host");
  int secs = 0, maxPlies = 0;
  if (vsAi) {
    g_index = 0; g_loss = 0; g_chip = 0x1A2B3C;
    chessGame.setOption(0, 0);
    chessGame.setOption(1, atoi(argv[2]));
    maxPlies = atoi(argv[3]);
    secs = 1000000;
  } else {
    g_index = atoi(argv[2]); g_loss = atoi(argv[3]); secs = atoi(argv[4]);
    g_chip = host ? 0x1A2B3C : 0x00FF01;
    chessGame.setOption(0, 1);
  }
  srand(plat::processId() * 7 + g_index);
  const char* me = vsAi ? "ai" : argv[1];
  auto& c = chessGame;
  c.init();
  buttons = {};

  const char* script[4] = {"f2f3", "e7e5", "g2g4", "d8h4"};     // fool's mate
  unsigned long t0 = millis();
  int frames = 0, settle = 0, rematches = 0, mates = 0;
  bool wasOver = false;
  uint16_t target = 0; int targetPly = -1; int targetGame = -1;
  bool bots = true;

  while (true) {
    unsigned long elapsed = millis() - t0;
    if (!vsAi) {
      if (bots && elapsed >= (unsigned long)secs * 1000) bots = false;            // stop playing; let the two sides catch up
      if (!bots && settle++ > 100) break;                                        // about four seconds
    } else if (c.over || c.plies >= maxPlies) break;
    air_pump();
    buttons.upPressed = buttons.downPressed = buttons.leftPressed = buttons.rightPressed = false;
    buttons.up = buttons.down = false;

    if (!vsAi && c.phase == ChessGame::PH_LOBBY) {
      LinkState ls = gameLink.state();
      if (ls == LINK_OFF || ls == LINK_IDLE) {
        if (c.lobby.phase == 0) { if (!host && c.lobby.roleChoice == 0) buttons.rightPressed = true; else buttons.upPressed = true; }
      } else if (host && ls == LINK_REQUESTED) buttons.upPressed = true;
      else if (!host && ls == LINK_SCANNING && gameLink.hostCount() > 0) buttons.upPressed = true;
    } else if (bots) {
      if (c.over && !vsAi) {
        if (frames % 8 == 0 && rematches < 1) buttons.rightPressed = true;        // both ask for a rematch
      } else if (c.myTurn() && c.stage != ChessGame::ST_WAIT) {
        if (targetPly != c.plies || targetGame != c.gameNo) {
          targetPly = c.plies; targetGame = c.gameNo;
          // game 0 starts with the scripted mate, then random moves
          target = (!vsAi && c.gameNo == 0 && c.plies < 4) ? scriptedMove(script[c.plies]) : c.legal[rand() % c.legalCount];
        }
        driveMove(c, target, frames);
      }
    }

    c.update();
    c.draw();

    if (c.over && !wasOver) {
      wasOver = true;
      if (c.result == CR_CHECKMATE) mates++;
      printf("[%s] game %d over: result %d, winner %d, after %d plies\n", me, c.gameNo, c.result, c.winner, c.plies);
    }
    if (!c.over && wasOver) { wasOver = false; rematches++; printf("[%s] rematch: game %d, I am %s\n", me, c.gameNo, c.myColor ? "Black" : "White"); }
    if (c.exitRequested) { printf("[%s] exit\n", me); break; }
    frames++;
    air_sleep_us(40000);
  }
  c.stop();
  printf("[%s] final game=%d plies=%d hash=%08x mates=%d rematches=%d over=%d\n", me, c.gameNo, c.plies, chessHash(c.pos), mates, rematches, c.over ? 1 : 0);
  return 0;
}
