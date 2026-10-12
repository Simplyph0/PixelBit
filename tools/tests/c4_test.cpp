#define private public
#include "connect4.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
extern unsigned long g_ms;
static unsigned long wall() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000UL + t.tv_nsec / 1000000; }
static void swapMarks(Connect4Game& g) { for (int r = 0; r < 6; r++) for (int c = 0; c < 7; c++) if (g.board[r][c]) g.board[r][c] = 3 - g.board[r][c]; }
// play one game: who1 / who2 are levels (-1 = random player). Returns 1, 2 or 3 (draw)
static int play(Connect4Game& g, int who1, int who2, unsigned long* maxMs) {
  memset(g.board, 0, sizeof(g.board)); g.roundResult = 0; g.turn = 1; g.round = 0; g.ptsX = g.ptsO = 0; g.seriesOver = false;
  while (!g.roundResult) {
    int who = g.turn == 1 ? who1 : who2; uint8_t col;
    if (who < 0) { uint8_t open[7], n = 0; for (int c = 0; c < 7; c++) if (g.columnFree(c)) open[n++] = c; col = open[rand() % n]; }
    else {
      g.level = who; unsigned long t0 = wall();
      if (g.turn == 1) swapMarks(g);        // the search always plays the ring side
      col = g.chooseAiMove();
      if (g.turn == 1) swapMarks(g);
      unsigned long dt = wall() - t0; if (dt > *maxMs) *maxMs = dt;
    }
    g.drop(col, g.turn);
  }
  return g.roundResult;
}
int main(int argc, char** argv) { srand(4);
  auto& g = connect4Game;
  // rules: all four directions win
  memset(g.board, 0, sizeof(g.board)); for (int i = 0; i < 4; i++) g.board[5][i] = 1; bool ok = g.findWin(g.board, 5, 2, nullptr);
  memset(g.board, 0, sizeof(g.board)); for (int i = 0; i < 4; i++) g.board[i][3] = 2; ok &= g.findWin(g.board, 2, 3, nullptr);
  memset(g.board, 0, sizeof(g.board)); for (int i = 0; i < 4; i++) g.board[i][i] = 1; ok &= g.findWin(g.board, 1, 1, nullptr);
  memset(g.board, 0, sizeof(g.board)); for (int i = 0; i < 4; i++) g.board[i][3 - i] = 2; ok &= g.findWin(g.board, 3, 0, nullptr);
  memset(g.board, 0, sizeof(g.board)); for (int i = 0; i < 3; i++) g.board[5][i] = 1; ok &= !g.findWin(g.board, 5, 1, nullptr);
  printf("win detection in all four directions: %s\n", ok ? "ok" : "FAIL");
  // the AI takes an immediate win and blocks an immediate loss
  memset(g.board, 0, sizeof(g.board)); for (int i = 0; i < 3; i++) g.board[5][i] = 2; g.level = 2; printf("AI completes its own three: column %d (want 3)\n", g.chooseAiMove());
  memset(g.board, 0, sizeof(g.board)); for (int i = 0; i < 3; i++) g.board[5][i] = 1; g.board[4][6] = 2; g.level = 2; printf("AI blocks the human's three (open on one side): column %d (want 3)\n", g.chooseAiMove());
  if (argc > 1) return 0;   // "quick": skip the long strength tables
  printf("\nAI (ring, moves second) against a random player, 200 games each:\n");
  unsigned long mx = 0;
  for (int lv = 0; lv < 5; lv++) { int w = 0, l = 0, d = 0; int N = lv >= 4 ? 8 : (lv == 3 ? 30 : 150);
    for (int i = 0; i < N; i++) { int r = play(g, -1, lv, &mx); if (r == 2) w++; else if (r == 1) l++; else d++; }
    printf("  level %d: AI wins %3d%%, draws %2d%%, loses %2d%%  (%d games)\n", lv, w * 100 / N, d * 100 / N, l * 100 / N, N); }
  printf("\nlevel against level (row = ring side, 20 games each, ring moves second):\n");
  for (int b = 1; b < 5; b++) for (int a = 0; a < b; a++) { int w = 0, l = 0, d = 0; for (int i = 0; i < (b >= 4 ? 4 : 10); i++) { int r = play(g, a, b, &mx); if (r == 2) w++; else if (r == 1) l++; else d++; }
    printf("  level %d (ring) vs level %d (solid): ring wins %2d, solid wins %2d, draws %2d\n", b, a, w, l, d); }
  printf("\nslowest single move on this PC: %lu ms\n", mx); }
