#define private public
#include "pong.h"
#include "tictactoe.h"
#include "rps.h"
#include "connect4.h"
#include "input.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "platform.h"
void air_sleep_us(unsigned long long);
extern int g_index, g_port, g_loss; extern uint32_t g_chip; void air_pump();
// vs_test <pong|ttt|rps|c4> <host|join> <index> <loss%> <seconds>
int main(int argc, char** argv) {
  const char* game = argv[1]; bool host = !strcmp(argv[2], "host"); g_index = atoi(argv[3]); g_loss = atoi(argv[4]); int secs = atoi(argv[5]);
  g_chip = host ? 0x1A2B3C : 0x00FF01; srand(plat::processId() * 3 + g_index);
  Game* g = !strcmp(game, "pong") ? (Game*)&pongGame : !strcmp(game, "ttt") ? (Game*)&ticTacToeGame : !strcmp(game, "c4") ? (Game*)&connect4Game : (Game*)&rpsGame;
  if (!strcmp(game, "pong")) { pongGame.setOption(0, 1); pongGame.setOption(2, 1); }
  if (!strcmp(game, "ttt")) ticTacToeGame.setOption(0, 1);
  if (!strcmp(game, "rps")) rpsGame.setOption(0, 1);
  if (!strcmp(game, "c4")) connect4Game.setOption(0, 1);
  g->init(); buttons = {};
  unsigned long t0 = millis(); int phaseSeen = -1; bool acted = false; int lastReport = 0; int rematches = 0; bool wasOver = false; int frames = 0;
  while (millis() - t0 < (unsigned long)secs * 1000) {
    air_pump(); buttons.upPressed = buttons.downPressed = buttons.leftPressed = buttons.rightPressed = false; buttons.up = buttons.down = false;
    LinkState ls = gameLink.state();
    // lobby script
    int ph = !strcmp(game, "pong") ? pongGame.phase : !strcmp(game, "ttt") ? ticTacToeGame.phase : !strcmp(game, "c4") ? connect4Game.phase : rpsGame.phase;
    if (ph == 0 /*PH_LOBBY*/) {
      if (ls == LINK_OFF || ls == LINK_IDLE) { Lobby& lb = !strcmp(game, "pong") ? pongGame.lobby : !strcmp(game, "ttt") ? ticTacToeGame.lobby : !strcmp(game, "c4") ? connect4Game.lobby : rpsGame.lobby;
        if (lb.phase == 0) { if (!host && lb.roleChoice == 0) buttons.rightPressed = true; else buttons.upPressed = true; } }
      else if (host && ls == LINK_REQUESTED) buttons.upPressed = true;
      else if (!host && ls == LINK_SCANNING && gameLink.hostCount() > 0) buttons.upPressed = true;
    } else {
      // in a match: simple bots
      if (!strcmp(game, "pong")) {
        float c = pongGame.playerY + pongGame.playerH / 2; if (host) { if (pongGame.ballY > c + 1) buttons.down = true; else if (pongGame.ballY < c - 1) buttons.up = true; }
        if (pongGame.matchOver) { if (frames % 8 == 0 && rematches < 1) buttons.rightPressed = true; }
      } else if (!strcmp(game, "ttt")) {
        auto& t = ticTacToeGame; static int target = -1;
        if (t.seriesOver && t.roundResult) { if (frames % 8 == 0 && rematches < 1) buttons.rightPressed = true; target = -1; }
        else if (!t.roundResult && t.turn == t.myMark) {
          if (target < 0 || t.board[target]) { int e[9], n = 0; for (int i = 0; i < 9; i++) if (!t.board[i]) e[n++] = i; target = n ? e[rand() % n] : -1; }
          if (target >= 0) { if (t.cursor != target) buttons.rightPressed = (frames % 2 == 0); else buttons.upPressed = true; }
        } else target = -1;
      } else if (!strcmp(game, "c4")) {
        auto& c = connect4Game; static int want = -1;
        if (c.seriesOver && c.roundResult) { if (frames % 8 == 0 && rematches < 1) buttons.rightPressed = true; want = -1; }
        else if (!c.roundResult && c.turn == c.myMark && millis() >= c.nextAt) {
          if (want < 0 || !c.columnFree(want)) { uint8_t open[7], n = 0; for (int k = 0; k < 7; k++) if (c.columnFree(k)) open[n++] = k; want = n ? open[rand() % n] : -1; }
          if (want >= 0) { if (c.cursor != want) buttons.rightPressed = (frames % 2 == 0); else buttons.upPressed = true; }
        } else want = -1;
      } else {
        auto& r = rpsGame;
        if (r.stage == 2) { if (frames % 8 == 0 && rematches < 1) buttons.rightPressed = true; }
        else if (r.stage == 0 && r.myChoice == 0xFF) { static int want = -1; if (want < 0) want = rand() % 3; if (r.selection != want) buttons.rightPressed = (frames % 2 == 0); else { buttons.upPressed = true; want = -1; } }
      }
    }
    g->update(); g->draw();
    if (ph != phaseSeen) { phaseSeen = ph; printf("[%s] phase %d t=%lu\n", argv[2], ph, millis() - t0); }
    // report scores / results
    if (!strcmp(game, "pong")) { if (pongGame.matchOver && !wasOver) { wasOver = true; printf("[%s] pong over %d-%d\n", argv[2], pongGame.playerScore, pongGame.aiScore); } if (!pongGame.matchOver && wasOver) { wasOver = false; rematches++; printf("[%s] rematch started\n", argv[2]); } }
    if (!strcmp(game, "ttt")) { bool over = ticTacToeGame.seriesOver && ticTacToeGame.roundResult; if (over && !wasOver) { wasOver = true; printf("[%s] ttt series over: me %d - opp %d\n", argv[2], ticTacToeGame.myMark == 1 ? ticTacToeGame.ptsX : ticTacToeGame.ptsO, ticTacToeGame.myMark == 1 ? ticTacToeGame.ptsO : ticTacToeGame.ptsX); } if (!over && wasOver) { wasOver = false; rematches++; printf("[%s] rematch started\n", argv[2]); } }
    if (!strcmp(game, "c4")) { bool over = connect4Game.seriesOver && connect4Game.roundResult; if (over && !wasOver) { wasOver = true; printf("[%s] c4 series over: me %d - opp %d\n", argv[2], connect4Game.myMark == 1 ? connect4Game.ptsX : connect4Game.ptsO, connect4Game.myMark == 1 ? connect4Game.ptsO : connect4Game.ptsX); } if (!over && wasOver) { wasOver = false; rematches++; printf("[%s] rematch started\n", argv[2]); } }
    if (!strcmp(game, "rps")) { bool over = rpsGame.stage == 2; if (over && !wasOver) { wasOver = true; printf("[%s] rps series over: me %d - opp %d\n", argv[2], rpsGame.myWins, rpsGame.opWins); } if (!over && wasOver) { wasOver = false; rematches++; printf("[%s] rematch started\n", argv[2]); } }
    if (g->wantsExit()) { printf("[%s] exit\n", argv[2]); break; }
    frames++; air_sleep_us(40000);
  }
  g->stop(); printf("[%s] done\n", argv[2]); }
