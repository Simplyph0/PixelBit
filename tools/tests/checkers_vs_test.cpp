// Checkers on two consoles over a simulated radio, through the real button flow, and against the computer.
//   checkers_vs_test <host|join> <index> <loss%> <seconds>   one side of a two-console game (run both). It plays until the
//                                                            rematch game has TARGET_STEPS steps, and gives up after <seconds>
//   checkers_vs_test ai <level> <max steps>                  one console against the computer
// Both sides play random legal steps. The first game is cut short on purpose: after a few steps the host swaps in a
// position where its next jump takes the last enemy man (checkmate in chess terms), so the end of a game, the result
// screen and a rematch with the sides swapped are played through. At the end each side says where it stands;
// checkers_check.py compares the two.
#define private public
#include "checkers.h"
#include "input.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "platform.h"
void air_sleep_us(unsigned long long);
extern int g_index, g_port, g_loss;
extern uint32_t g_chip;
void air_pump();

// Played until the rematch game has this many steps (not for a fixed time: see chess_vs_test.cpp)
static const int TARGET_STEPS = 24;

static uint32_t hashOf(const CkPos& p) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < 64; i++) h = (h ^ p.sq[i]) * 16777619u;
  return (((h ^ p.side) * 16777619u) ^ p.chain) * 16777619u;
}

// Presses the buttons that play the step `want` on this console, one press per call
static void driveStep(CheckersGame& c, uint16_t want, int frame) {
  const bool press = frame % 2 == 0;                       // a press every other frame, like a finger
  if (c.stage == CheckersGame::ST_PIECE) {
    int idx = -1;
    for (int i = 0; i < c.pieceCount; i++) if (c.pieces[i] == ckFrom(want)) idx = i;
    if (idx < 0) return;
    if (c.pieceIdx != idx) { if (press) buttons.rightPressed = true; }
    else if (press) buttons.upPressed = true;
  } else if (c.stage == CheckersGame::ST_DEST) {
    int idx = -1;
    for (int i = 0; i < c.destCount; i++) if (c.dests[i] == want) idx = i;
    if (idx < 0) { if (press) buttons.downPressed = true; return; }
    if (c.destIdx != idx) { if (press) buttons.rightPressed = true; }
    else if (press) buttons.upPressed = true;
  }
}

int main(int argc, char** argv) {
  const bool vsAi = !strcmp(argv[1], "ai");
  bool host = !strcmp(argv[1], "host");
  int secs = 0, maxSteps = 0;
  if (vsAi) {
    g_index = 0; g_loss = 0; g_chip = 0x1A2B3C;
    checkersGame.setOption(0, 0);
    checkersGame.setOption(1, atoi(argv[2]));
    maxSteps = atoi(argv[3]);
    secs = 1000000;
  } else {
    g_index = atoi(argv[2]); g_loss = atoi(argv[3]); secs = atoi(argv[4]);
    g_chip = host ? 0x1A2B3C : 0x00FF01;
    checkersGame.setOption(0, 1);
  }
  srand(plat::processId() * 7 + g_index);
  const char* me = vsAi ? "ai" : argv[1];
  auto& c = checkersGame;
  c.init();
  buttons = {};

  unsigned long t0 = millis();
  int frames = 0, settle = 0, rematches = 0, wins = 0;
  bool wasOver = false, cut = false;
  uint16_t target = 0; int targetSteps = -1; int targetGame = -1; int targetChain = -1;
  bool bots = true;

  while (true) {
    unsigned long elapsed = millis() - t0;
    if (!vsAi) {
      const bool enough = rematches >= 1 && (c.steps >= TARGET_STEPS || c.over);
      if (bots && (enough || elapsed >= (unsigned long)secs * 1000)) bots = false;   // stop playing; let the two sides catch up
      if (!bots && settle++ > 100) break;                                        // about four seconds
    } else if (c.over || c.steps >= maxSteps) break;
    air_pump();
    buttons.upPressed = buttons.downPressed = buttons.leftPressed = buttons.rightPressed = false;
    buttons.up = buttons.down = false;

    if (!vsAi && c.phase == CheckersGame::PH_LOBBY) {
      LinkState ls = gameLink.state();
      if (ls == LINK_OFF || ls == LINK_IDLE) {
        if (c.lobby.phase == 0) { if (!host && c.lobby.roleChoice == 0) buttons.rightPressed = true; else buttons.upPressed = true; }
      } else if (host && ls == LINK_REQUESTED) buttons.upPressed = true;
      else if (!host && ls == LINK_SCANNING && gameLink.hostCount() > 0) buttons.upPressed = true;
    } else if (bots) {
      if (c.over && !vsAi) {
        if (frames % 8 == 0 && rematches < 1) buttons.rightPressed = true;        // both ask for a rematch
      } else if (c.myTurn() && c.stage != CheckersGame::ST_WAIT) {
        // Game 0, host: after 6 steps swap in a position where the next jump takes the last enemy man
        if (!vsAi && host && !cut && c.gameNo == 0 && c.steps >= 6 && c.pos.chain == CK_NO_CHAIN) {
          memset(c.pos.sq, 0, sizeof(c.pos.sq));
          c.pos.sq[5 * 8 + 0] = CK_MAN0;
          c.pos.sq[4 * 8 + 1] = CK_MAN1;
          c.pos.side = 0;
          c.steps++;                                      // the joiner takes the new position when the count changes
          c.refresh();
          cut = true;
        }
        if (targetSteps != c.steps || targetGame != c.gameNo || targetChain != c.pos.chain) {
          targetSteps = c.steps; targetGame = c.gameNo; targetChain = c.pos.chain;
          target = c.legal[rand() % c.legalCount];
        }
        driveStep(c, target, frames);
      }
    }

    c.update();
    c.draw();

    if (c.over && !wasOver) {
      wasOver = true;
      if (c.winner == (int8_t)c.myColor) wins++;
      printf("[%s] game %d over: result %d, winner %d, after %d steps\n", me, c.gameNo, c.result, c.winner, c.steps);
    }
    if (!c.over && wasOver) { wasOver = false; rematches++; printf("[%s] rematch: game %d, I am side %d\n", me, c.gameNo, c.myColor); }
    if (c.exitRequested) { printf("[%s] exit\n", me); break; }
    frames++;
    air_sleep_us(40000);
  }
  c.stop();
  printf("[%s] final game=%d steps=%d hash=%08x wins=%d rematches=%d over=%d\n", me, c.gameNo, c.steps, hashOf(c.pos), wins, rematches, c.over ? 1 : 0);
  return 0;
}
