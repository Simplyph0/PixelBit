// Checks the checkers engine (GameSystem/checkersengine.cpp): the rules against the standard perft
// counts, the capture and crowning cases, the end of a game, and the computer player.
#include "checkersengine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); fails++; } } while (0)

// Eight rows from the top: . empty, x / X a man / king of side 0 (starts at the bottom), o / O of side 1
static void setup(CkPos& p, uint8_t side, const char* const rows[8]) {
  memset(&p, 0, sizeof(p));
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++) {
      switch (rows[r][c]) {
        case 'x': p.sq[r * 8 + c] = CK_MAN0; break;
        case 'X': p.sq[r * 8 + c] = CK_KING0; break;
        case 'o': p.sq[r * 8 + c] = CK_MAN1; break;
        case 'O': p.sq[r * 8 + c] = CK_KING1; break;
      }
    }
  p.side = side;
  p.chain = CK_NO_CHAIN;
}

static uint16_t st(int r1, int c1, int r2, int c2) { return ckStep(r1 * 8 + c1, r2 * 8 + c2); }

static bool has(const CkPos& p, uint16_t s) {
  uint16_t steps[CK_MAX_STEPS];
  int n = ckSteps(p, steps, CK_MAX_STEPS);
  for (int i = 0; i < n; i++) if (steps[i] == s) return true;
  return false;
}

static int count(const CkPos& p) { uint16_t s[CK_MAX_STEPS]; return ckSteps(p, s, CK_MAX_STEPS); }

static void rules() {
  CkPos p;
  ckStart(p);
  CHECK(count(p) == 7, "seven opening moves");
  int a = 0, b = 0;
  for (int s = 0; s < 64; s++) { if (p.sq[s] == CK_MAN0) a++; if (p.sq[s] == CK_MAN1) b++; }
  CHECK(a == 12 && b == 12, "twelve men each");
  CHECK(p.sq[7 * 8 + 0] == CK_MAN0 && p.sq[0 * 8 + 1] == CK_MAN1, "dark square at the bottom left, side 1 on top");

  // a capture is compulsory: the other man's plain moves are not offered
  const char* force[8] = {"........", "........", "........", "....o...", "...x....", "......x.", "........", "........"};
  setup(p, 0, force);
  CHECK(count(p) == 1 && has(p, st(4, 3, 2, 5)), "the jump is the only move");

  // men do not jump or move backwards, kings do
  const char* rear[8] = {"........", "........", "........", "........", "...x....", "....o...", "........", "........"};
  setup(p, 0, rear);
  CHECK(!has(p, st(4, 3, 6, 5)) && count(p) == 2, "a man steps forward only and cannot jump backwards (the enemy is behind it)");
  const char* king[8] = {"........", "........", "........", "........", "...X....", "....o...", "........", "........"};
  setup(p, 0, king);
  CHECK(has(p, st(4, 3, 6, 5)) && count(p) == 1, "a king can jump backwards");

  // a multiple jump: same side moves on, with the piece chosen for it
  const char* multi[8] = {"........", "........", "...o....", "........", ".o......", "x.......", "........", "........"};
  setup(p, 0, multi);
  CHECK(count(p) == 1, "one jump to begin with");
  ckApply(p, st(5, 0, 3, 2));
  CHECK(p.side == 0 && p.chain == 3 * 8 + 2, "the piece must jump on");
  CHECK(count(p) == 1 && has(p, st(3, 2, 1, 4)), "only the same piece, only a jump");
  ckApply(p, st(3, 2, 1, 4));
  CHECK(p.side == 1 && p.chain == CK_NO_CHAIN && p.sq[4 * 8 + 1] == 0 && p.sq[2 * 8 + 3] == 0, "both men taken, the turn passes");

  // reaching the far row crowns and ends the move, even if a king could jump on
  const char* crown[8] = {"........", "..o.o...", ".x......", "........", "........", "........", "........", "........"};
  setup(p, 0, crown);
  ckApply(p, st(2, 1, 0, 3));
  CHECK(p.sq[3] == CK_KING0 && p.side == 1 && p.chain == CK_NO_CHAIN, "crowned, and the move is over");

  // a man that steps onto the far row is crowned too
  const char* step[8] = {"........", "..x.....", "........", "........", "........", "........", "........", "........"};
  setup(p, 0, step);
  ckApply(p, st(1, 2, 0, 1));
  CHECK(p.sq[1] == CK_KING0, "a step to the last row crowns");

  // the end
  const char* gone[8] = {"........", "........", "........", "........", "........", "........", "........", "x......."};
  setup(p, 1, gone);
  CHECK(ckResultOf(p) == CK_WIN0, "side 1 has no piece: side 0 wins");
  const char* blocked[8] = {".o......", "x.x.....", "...x....", "........", "........", "........", "........", "........"};
  setup(p, 1, blocked);
  CHECK(ckResultOf(p) == CK_WIN0, "side 1 has a piece but no move: side 0 wins");
  const char* kings[8] = {"........", "........", "...X....", "........", ".....O..", "........", "........", "........"};
  setup(p, 0, kings);
  p.quiet = CK_DRAW_QUIET - 1;
  CHECK(ckResultOf(p) == CK_PLAYING, "not yet a draw");
  ckApply(p, st(2, 3, 3, 4));
  CHECK(p.quiet == CK_DRAW_QUIET && ckResultOf(p) == CK_DRAW, "forty moves each with no capture or man's move: a draw");
  CHECK(!ckPlay(p, st(0, 0, 1, 1)) && ckPlay(p, st(4, 5, 2, 3)), "ckPlay refuses an illegal step and plays a legal one");
}

static void perft() {
  CkPos p;
  ckStart(p);
  static const uint32_t expected[] = {7, 49, 302, 1469, 7361, 36768, 179740, 845931};
  for (int d = 0; d < 8; d++) {
    uint32_t n = ckPerft(p, d + 1);
    if (n != expected[d]) { printf("FAIL: perft depth %d: %u, expected %u\n", d + 1, n, expected[d]); fails++; return; }
  }
  printf("  ok   perft to depth 8\n");
}

static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) % n; }
static unsigned long fakeNow() { return (unsigned long)clock() * 1000 / CLOCKS_PER_SEC; }
static CkHooks hooks = {fakeNow, nullptr, rnd};

// Plays two sides to the end; returns +1 side 0 won, -1 side 1 won, 0 draw
static int play(const CkAiLevel& a, const CkAiLevel& b, void* work) {
  CkPos p;
  ckStart(p);
  for (int n = 0; n < 1000; n++) {
    CkResult r = ckResultOf(p);
    if (r == CK_WIN0) return 1;
    if (r == CK_WIN1) return -1;
    if (r == CK_DRAW) return 0;
    uint16_t s = ckChooseStep(p, p.side == 0 ? a : b, work, hooks);
    if (!s || !ckPlay(p, s)) { printf("FAIL: the computer played an illegal step\n"); fails++; return 0; }
  }
  return 0;
}

static void ai() {
  void* work = malloc(ckWorkSize());
  const CkAiLevel rand_ = {0, 0, 0, 100}, strong = {5, 150, 0, 0}, weak = {1, 50, 0, 0};

  // in a position with a capture on offer the chosen step is a legal one, also in the middle of a jump
  CkPos p;
  const char* multi[8] = {"........", "........", "...o....", "........", ".o......", "x.......", "........", "........"};
  setup(p, 0, multi);
  uint16_t s = ckChooseStep(p, strong, work, hooks);
  CHECK(s == st(5, 0, 3, 2), "the only jump is chosen");
  ckApply(p, s);
  s = ckChooseStep(p, strong, work, hooks);
  CHECK(s == st(3, 2, 1, 4), "it keeps jumping with the same piece");
  CHECK(ckChooseStep(p, strong, nullptr, hooks) == st(3, 2, 1, 4), "no memory: still a legal step");

  int won = 0, lost = 0;
  for (int g = 0; g < 8; g++) {
    int r = (g & 1) ? -play(rand_, strong, work) : play(strong, rand_, work);
    if (r > 0) won++; else if (r < 0) lost++;
  }
  printf("  strong vs random: won %d, lost %d of 8\n", won, lost);
  CHECK(lost == 0 && won >= 6, "the search beats random moves");

  won = lost = 0;
  for (int g = 0; g < 6; g++) {
    int r = (g & 1) ? -play(weak, strong, work) : play(strong, weak, work);
    if (r > 0) won++; else if (r < 0) lost++;
  }
  printf("  depth 5 vs depth 1: won %d, lost %d of 6\n", won, lost);
  CHECK(won >= 2 && lost == 0, "a deeper search never loses to a shallow one (the rest are draws)");
  free(work);
}

int main() {
  rules();
  perft();
  ai();
  printf("%s\n", fails ? "checkers: SOME CHECKS FAILED" : "checkers: all checks passed");
  return fails != 0;
}
