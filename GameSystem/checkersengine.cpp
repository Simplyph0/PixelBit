#include "checkersengine.h"
#include <string.h>

static const int8_t DC[2] = {-1, 1};

static inline int rowOf(int s) { return s >> 3; }
static inline int colOf(int s) { return s & 7; }
static inline bool isKing(uint8_t piece) { return (piece & 1) == 0; }   // 2 and 4

void ckStart(CkPos& p) {
  memset(&p, 0, sizeof(p));
  for (int s = 0; s < 64; s++) {
    if (((rowOf(s) + colOf(s)) & 1) == 0) continue;
    if (rowOf(s) < 3) p.sq[s] = CK_MAN1;
    else if (rowOf(s) > 4) p.sq[s] = CK_MAN0;
  }
  p.chain = CK_NO_CHAIN;
}

// Row directions a piece may move in: side 0 goes up the screen (-1), side 1 down (+1)
static int rowDirs(uint8_t piece, int8_t out[2]) {
  if (isKing(piece)) { out[0] = -1; out[1] = 1; return 2; }
  out[0] = ckSideOf(piece) == 0 ? -1 : 1;
  return 1;
}

static int jumpsFrom(const CkPos& p, int s, uint16_t* out, int n, int max) {
  uint8_t piece = p.sq[s];
  int8_t dr[2];
  int nd = rowDirs(piece, dr);
  for (int i = 0; i < nd; i++) {
    for (int j = 0; j < 2; j++) {
      int r1 = rowOf(s) + dr[i], c1 = colOf(s) + DC[j], r2 = r1 + dr[i], c2 = c1 + DC[j];
      if (r2 < 0 || r2 > 7 || c2 < 0 || c2 > 7) continue;
      uint8_t over = p.sq[r1 * 8 + c1];
      if (over != CK_EMPTY && ckSideOf(over) != ckSideOf(piece) && p.sq[r2 * 8 + c2] == CK_EMPTY && n < max)
        out[n++] = ckStep(s, r2 * 8 + c2);
    }
  }
  return n;
}

static int movesFrom(const CkPos& p, int s, uint16_t* out, int n, int max) {
  int8_t dr[2];
  int nd = rowDirs(p.sq[s], dr);
  for (int i = 0; i < nd; i++) {
    for (int j = 0; j < 2; j++) {
      int r = rowOf(s) + dr[i], c = colOf(s) + DC[j];
      if (r >= 0 && r <= 7 && c >= 0 && c <= 7 && p.sq[r * 8 + c] == CK_EMPTY && n < max) out[n++] = ckStep(s, r * 8 + c);
    }
  }
  return n;
}

int ckSteps(const CkPos& p, uint16_t* out, int max) {
  int n = 0;
  if (p.chain != CK_NO_CHAIN) return jumpsFrom(p, p.chain, out, 0, max);
  for (int s = 0; s < 64; s++)
    if (p.sq[s] != CK_EMPTY && ckSideOf(p.sq[s]) == p.side) n = jumpsFrom(p, s, out, n, max);
  if (n > 0) return n;
  for (int s = 0; s < 64; s++)
    if (p.sq[s] != CK_EMPTY && ckSideOf(p.sq[s]) == p.side) n = movesFrom(p, s, out, n, max);
  return n;
}

void ckApply(CkPos& p, uint16_t step) {
  int from = ckFrom(step), to = ckTo(step);
  uint8_t piece = p.sq[from];
  bool jump = ckIsJump(step), man = !isKing(piece), crowned = false;
  p.sq[from] = CK_EMPTY;
  if (jump) p.sq[(from + to) / 2] = CK_EMPTY;
  if (man && rowOf(to) == (p.side == 0 ? 0 : 7)) { piece++; crowned = true; }
  p.sq[to] = piece;

  uint16_t more[1];
  if (jump && !crowned && jumpsFrom(p, to, more, 0, 1) > 0) {   // the same piece jumps on
    p.chain = to;
    p.quiet = 0;
    return;
  }
  p.chain = CK_NO_CHAIN;
  p.quiet = (jump || man) ? 0 : (p.quiet < 255 ? p.quiet + 1 : 255);
  p.side ^= 1;
}

bool ckPlay(CkPos& p, uint16_t step) {
  uint16_t steps[CK_MAX_STEPS];
  int n = ckSteps(p, steps, CK_MAX_STEPS);
  for (int i = 0; i < n; i++) if (steps[i] == step) { ckApply(p, step); return true; }
  return false;
}

CkResult ckResultOf(const CkPos& p) {
  uint16_t steps[CK_MAX_STEPS];
  if (ckSteps(p, steps, CK_MAX_STEPS) == 0) return p.side == 0 ? CK_WIN1 : CK_WIN0;
  return p.quiet >= CK_DRAW_QUIET ? CK_DRAW : CK_PLAYING;
}

uint32_t ckPerft(const CkPos& p, int depth) {
  if (depth == 0) return 1;
  uint16_t steps[CK_MAX_STEPS];
  int n = ckSteps(p, steps, CK_MAX_STEPS);
  uint32_t total = 0;
  for (int i = 0; i < n; i++) {
    CkPos c = p;
    ckApply(c, steps[i]);
    total += ckPerft(c, c.chain == CK_NO_CHAIN ? depth - 1 : depth);   // the rest of a jump is the same move
  }
  return total;
}

// ---------------------------------------------------------------------------
// The computer player: alpha-beta search, deepened one move at a time until the
// time is up. The search keeps going through forced captures, so it never stops
// halfway through an exchange.
// ---------------------------------------------------------------------------
#define CK_MAXPLY 32
#define CK_INF 30000
#define CK_WIN 10000

struct CkWork {
  CkPos pos[CK_MAXPLY];
  uint16_t steps[CK_MAXPLY][CK_MAX_STEPS];
  const CkHooks* hooks;
  unsigned long deadline;
  uint32_t nodes;
  bool aborted;
};

unsigned ckWorkSize() { return sizeof(CkWork); }

// Material, how far the men have got, the back row kept home, and the centre; for side 0 minus side 1
static int evaluate(const CkPos& p) {
  int score = 0;
  for (int s = 0; s < 64; s++) {
    uint8_t piece = p.sq[s];
    if (piece == CK_EMPTY) continue;
    int side = ckSideOf(piece), r = rowOf(s), c = colOf(s);
    int v = isKing(piece) ? 170 : 100;
    if (!isKing(piece)) {
      v += 4 * (side == 0 ? 7 - r : r);                       // nearer to a crown
      if (r == (side == 0 ? 7 : 0)) v += 10;                  // the back row stops enemy crowns
    }
    if (c >= 2 && c <= 5 && r >= 2 && r <= 5) v += 3;
    score += side == 0 ? v : -v;
  }
  return p.side == 0 ? score : -score;
}

static int search(CkWork& w, int ply, int depth, int alpha, int beta) {
  if ((++w.nodes & 127) == 0) {
    if (w.hooks->poll) w.hooks->poll();
    if (w.hooks->now() >= w.deadline) w.aborted = true;
  }
  if (w.aborted) return 0;

  const CkPos& p = w.pos[ply];
  if (p.quiet >= CK_DRAW_QUIET) return 0;
  uint16_t* steps = w.steps[ply];
  int n = ckSteps(p, steps, CK_MAX_STEPS);
  if (n == 0) return -(CK_WIN - ply);                       // nothing to move: lost
  if ((depth <= 0 && !ckIsJump(steps[0])) || ply >= CK_MAXPLY - 2) return evaluate(p);

  int best = -CK_INF;
  for (int i = 0; i < n; i++) {
    w.pos[ply + 1] = p;
    ckApply(w.pos[ply + 1], steps[i]);
    bool same = w.pos[ply + 1].side == p.side;              // a jump that goes on
    int v = same ? search(w, ply + 1, depth, alpha, beta) : -search(w, ply + 1, depth - 1, -beta, -alpha);
    if (w.aborted) return 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

uint16_t ckChooseStep(const CkPos& p, const CkAiLevel& level, void* work, const CkHooks& hooks) {
  uint16_t steps[CK_MAX_STEPS];
  int vals[CK_MAX_STEPS];
  int n = ckSteps(p, steps, CK_MAX_STEPS);
  if (n == 0) return 0;
  if (n == 1) return steps[0];
  if (!work || level.depth == 0 || (int)hooks.random(100) < level.randomPct) return steps[hooks.random(n)];

  CkWork& w = *(CkWork*)work;
  w.hooks = &hooks;
  w.deadline = hooks.now() + level.ms;
  w.nodes = 0;
  w.aborted = false;

  bool have = false;
  for (int depth = 1; depth <= level.depth; depth++) {
    int tmp[CK_MAX_STEPS], bestVal = -CK_INF, bestIdx = 0;
    for (int i = 0; i < n; i++) {
      w.pos[0] = p;
      w.pos[1] = p;
      ckApply(w.pos[1], steps[i]);
      bool same = w.pos[1].side == p.side;
      int alpha = level.noise ? -CK_INF : bestVal;          // with noise every move needs its true value
      int v = same ? search(w, 1, depth, alpha, CK_INF) : -search(w, 1, depth - 1, -CK_INF, -alpha);
      if (w.aborted) break;
      tmp[i] = v;
      if (v > bestVal) { bestVal = v; bestIdx = i; }
    }
    if (w.aborted) break;                                   // keep the last finished pass
    memcpy(vals, tmp, sizeof(int) * n);
    have = true;
    uint16_t s = steps[0]; steps[0] = steps[bestIdx]; steps[bestIdx] = s;   // best first next time
    int t = vals[0]; vals[0] = vals[bestIdx]; vals[bestIdx] = t;
    if (bestVal >= CK_WIN - CK_MAXPLY || bestVal <= -(CK_WIN - CK_MAXPLY)) break;   // decided
  }
  if (!have) return steps[hooks.random(n)];

  int pick = 0, pickVal = -CK_INF * 2;
  for (int i = 0; i < n; i++) {
    int v = vals[i] + (level.noise ? (int)hooks.random(2 * level.noise + 1) - level.noise : 0);
    if (v > pickVal) { pickVal = v; pick = i; }
  }
  return steps[pick];
}
