#include "chessengine.h"
#include <string.h>

#if defined(ARDUINO)
  #include <Arduino.h>
#else
  #define PROGMEM
  #define pgm_read_byte(a) (*(const uint8_t*)(a))
#endif

static const int8_t KNIGHT_STEP[8] = {33, 31, 18, 14, -14, -18, -31, -33};
// Queen directions. The first four are the rook's, the last four the bishop's.
static const int8_t DIR[8] = {1, -1, 16, -16, 15, -15, 17, -17};

static const int VALUE[7] = {0, 100, 320, 330, 500, 900, 0};

// ---------------------------------------------------------------------------
// Setting up, attacks
// ---------------------------------------------------------------------------
void chessStart(ChessPos& p) {
  memset(&p, 0, sizeof(p));
  static const uint8_t back[8] = {CH_R, CH_N, CH_B, CH_Q, CH_K, CH_B, CH_N, CH_R};
  for (int f = 0; f < 8; f++) {
    p.sq[f] = back[f];
    p.sq[16 + f] = CH_P;
    p.sq[96 + f] = CH_P | CH_BLACK;
    p.sq[112 + f] = back[f] | CH_BLACK;
  }
  p.side = 0;
  p.castle = 15;
  p.ep = -1;
  p.half = 0;
  p.king[0] = 0x04;
  p.king[1] = 0x74;
}

bool chessAttacked(const ChessPos& p, uint8_t sq, uint8_t byColour) {
  const uint8_t c = byColour ? CH_BLACK : 0;
  int s = sq + (byColour ? 15 : -15);
  if (!(s & 0x88) && p.sq[s] == (CH_P | c)) return true;
  s = sq + (byColour ? 17 : -17);
  if (!(s & 0x88) && p.sq[s] == (CH_P | c)) return true;
  for (int i = 0; i < 8; i++) {
    s = sq + KNIGHT_STEP[i];
    if (!(s & 0x88) && p.sq[s] == (CH_N | c)) return true;
    s = sq + DIR[i];
    if (!(s & 0x88) && p.sq[s] == (CH_K | c)) return true;
  }
  for (int i = 0; i < 8; i++) {
    int d = DIR[i];
    for (s = sq + d; !(s & 0x88); s += d) {
      uint8_t pc = p.sq[s];
      if (!pc) continue;
      if ((pc & CH_BLACK) == c) {
        uint8_t t = pc & 7;
        if (t == CH_Q || (i < 4 ? t == CH_R : t == CH_B)) return true;
      }
      break;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Making and unmaking moves
// ---------------------------------------------------------------------------
void chessMake(ChessPos& p, uint16_t m, ChessUndo& u) {
  const uint8_t from = chessTo88(chessMoveFrom(m)), to = chessTo88(chessMoveTo(m)), promo = chessMovePromo(m);
  const uint8_t piece = p.sq[from], type = piece & 7, colourBit = piece & CH_BLACK;
  const int dir = colourBit ? -16 : 16;

  u.captured = p.sq[to];
  u.castle = p.castle;
  u.ep = p.ep;
  u.half = p.half;
  u.epCapture = false;

  if (type == CH_P && to == (uint8_t)p.ep && p.ep >= 0 && !p.sq[to] && ((from ^ to) & 7)) {
    u.epCapture = true;
    u.captured = p.sq[to - dir];
    p.sq[to - dir] = 0;
  }
  p.sq[to] = promo ? (uint8_t)(promo | colourBit) : piece;
  p.sq[from] = 0;

  if (type == CH_K) {
    p.king[colourBit ? 1 : 0] = to;
    if (to - from == 2) { p.sq[from + 1] = p.sq[from + 3]; p.sq[from + 3] = 0; }
    else if (from - to == 2) { p.sq[from - 1] = p.sq[from - 4]; p.sq[from - 4] = 0; }
  }

  // castling rights go when a king or a rook moves, or a rook is taken
  if (from == 0x04) p.castle &= ~3;
  if (from == 0x74) p.castle &= ~12;
  if (from == 0x00 || to == 0x00) p.castle &= ~2;
  if (from == 0x07 || to == 0x07) p.castle &= ~1;
  if (from == 0x70 || to == 0x70) p.castle &= ~8;
  if (from == 0x77 || to == 0x77) p.castle &= ~4;

  p.ep = (type == CH_P && (to - from == 32 || from - to == 32)) ? (int8_t)(from + dir) : -1;
  p.half = (type == CH_P || u.captured) ? 0 : (p.half < 255 ? p.half + 1 : 255);
  p.side ^= 1;
}

void chessUnmake(ChessPos& p, uint16_t m, const ChessUndo& u) {
  const uint8_t from = chessTo88(chessMoveFrom(m)), to = chessTo88(chessMoveTo(m)), promo = chessMovePromo(m);
  p.side ^= 1;
  uint8_t piece = p.sq[to];
  const uint8_t colourBit = piece & CH_BLACK;
  if (promo) piece = CH_P | colourBit;
  const uint8_t type = piece & 7;
  const int dir = colourBit ? -16 : 16;

  p.sq[from] = piece;
  if (u.epCapture) { p.sq[to] = 0; p.sq[to - dir] = u.captured; }
  else p.sq[to] = u.captured;

  if (type == CH_K) {
    p.king[colourBit ? 1 : 0] = from;
    if (to - from == 2) { p.sq[from + 3] = p.sq[from + 1]; p.sq[from + 1] = 0; }
    else if (from - to == 2) { p.sq[from - 4] = p.sq[from - 1]; p.sq[from - 1] = 0; }
  }
  p.castle = u.castle;
  p.ep = u.ep;
  p.half = u.half;
}

// ---------------------------------------------------------------------------
// Move generation. Each entry is the move in the low 16 bits and a sort key in
// the high 16 (captures first, the most valuable victim by the least valuable
// attacker), so the search can try the likely good moves first.
// ---------------------------------------------------------------------------
static inline void add(uint32_t* out, int& n, int cap, uint8_t from, uint8_t to, uint8_t promo, int key) {
  if (n < cap) out[n++] = (uint32_t)chessMove(chessFrom88(from), chessFrom88(to), promo) | ((uint32_t)key << 16);
}

static int genPseudo(const ChessPos& p, uint32_t* out, int cap, bool capsOnly) {
  int n = 0;
  const uint8_t me = p.side, mine = me ? CH_BLACK : 0;
  for (int s = 0; s < 128; s++) {
    if (s & 0x88) { s += 7; continue; }
    const uint8_t pc = p.sq[s];
    if (!pc || (pc & CH_BLACK) != mine) continue;
    const uint8_t type = pc & 7;

    if (type == CH_P) {
      const int dir = me ? -16 : 16, startRank = me ? 6 : 1, lastRank = me ? 0 : 7;
      int t = s + dir;
      if (!p.sq[t]) {
        if ((t >> 4) == lastRank) {
          for (uint8_t pr = CH_Q; pr >= CH_N; pr--) add(out, n, cap, s, t, pr, 300 + pr);
        } else if (!capsOnly) {
          add(out, n, cap, s, t, 0, 0);
          if ((s >> 4) == startRank && !p.sq[t + dir]) add(out, n, cap, s, t + dir, 0, 0);
        }
      }
      for (int k = -1; k <= 1; k += 2) {
        t = s + dir + k;
        if (t & 0x88) continue;
        const uint8_t tp = p.sq[t];
        if (tp && (tp & CH_BLACK) != mine) {
          const int key = 100 + (tp & 7) * 8 - 1;
          if ((t >> 4) == lastRank) { for (uint8_t pr = CH_Q; pr >= CH_N; pr--) add(out, n, cap, s, t, pr, key + 300 + pr); }
          else add(out, n, cap, s, t, 0, key);
        } else if (!tp && t == p.ep) {
          add(out, n, cap, s, t, 0, 100 + CH_P * 8 - 1);
        }
      }
      continue;
    }

    if (type == CH_N || type == CH_K) {
      for (int i = 0; i < 8; i++) {
        const int t = s + (type == CH_N ? KNIGHT_STEP[i] : DIR[i]);
        if (t & 0x88) continue;
        const uint8_t tp = p.sq[t];
        if (!tp) { if (!capsOnly) add(out, n, cap, s, t, 0, 0); }
        else if ((tp & CH_BLACK) != mine) add(out, n, cap, s, t, 0, 100 + (tp & 7) * 8 - type);
      }
      if (type == CH_K && !capsOnly) {
        const uint8_t home = me ? 0x74 : 0x04;
        const uint8_t foe = me ? 0 : 1;
        if (s == home && !chessAttacked(p, home, foe)) {
          if ((p.castle & (me ? 4 : 1)) && !p.sq[home + 1] && !p.sq[home + 2] && !chessAttacked(p, home + 1, foe))
            add(out, n, cap, s, home + 2, 0, 0);
          if ((p.castle & (me ? 8 : 2)) && !p.sq[home - 1] && !p.sq[home - 2] && !p.sq[home - 3] && !chessAttacked(p, home - 1, foe))
            add(out, n, cap, s, home - 2, 0, 0);
        }
      }
      continue;
    }

    const int first = type == CH_B ? 4 : 0, last = type == CH_R ? 4 : 8;
    for (int i = first; i < last; i++) {
      const int d = DIR[i];
      for (int t = s + d; !(t & 0x88); t += d) {
        const uint8_t tp = p.sq[t];
        if (!tp) { if (!capsOnly) add(out, n, cap, s, t, 0, 0); continue; }
        if ((tp & CH_BLACK) != mine) add(out, n, cap, s, t, 0, 100 + (tp & 7) * 8 - type);
        break;
      }
    }
  }
  return n;
}

int chessLegalMoves(const ChessPos& pos, uint16_t* out, int max) {
  uint32_t pseudo[CHESS_MAX_MOVES];
  ChessPos p = pos;
  const int n = genPseudo(p, pseudo, CHESS_MAX_MOVES, false);
  const uint8_t mover = p.side;
  int count = 0;
  for (int i = 0; i < n && count < max; i++) {
    const uint16_t m = (uint16_t)pseudo[i];
    ChessUndo u;
    chessMake(p, m, u);
    if (!chessAttacked(p, p.king[mover], mover ^ 1)) out[count++] = m;
    chessUnmake(p, m, u);
  }
  return count;
}

bool chessPlay(ChessPos& p, uint16_t move) {
  uint16_t legal[CHESS_MAX_MOVES];
  const int n = chessLegalMoves(p, legal, CHESS_MAX_MOVES);
  for (int i = 0; i < n; i++) {
    if (legal[i] == move) { ChessUndo u; chessMake(p, move, u); return true; }
  }
  return false;
}

static bool insufficientMaterial(const ChessPos& p) {
  int minors = 0;
  for (int s = 0; s < 128; s++) {
    if (s & 0x88) { s += 7; continue; }
    const uint8_t t = p.sq[s] & 7;
    if (t == CH_P || t == CH_R || t == CH_Q) return false;
    if (t == CH_N || t == CH_B) minors++;
  }
  return minors <= 1;
}

ChessResult chessResultOf(const ChessPos& p, int legalCount) {
  if (legalCount == 0) return chessInCheck(p, p.side) ? CR_CHECKMATE : CR_STALEMATE;
  if (insufficientMaterial(p)) return CR_INSUFFICIENT;
  if (p.half >= 100) return CR_FIFTY;
  return CR_PLAYING;
}

uint32_t chessHash(const ChessPos& p) {
  uint32_t h = 2166136261u;
  for (int s = 0; s < 128; s++) {
    if (s & 0x88) { s += 7; continue; }
    if (p.sq[s]) h = (h ^ (uint32_t)(p.sq[s] | (chessFrom88(s) << 4))) * 16777619u;
  }
  h = (h ^ p.side) * 16777619u;
  h = (h ^ p.castle) * 16777619u;
  h = (h ^ (uint32_t)(p.ep < 0 ? 0xFF : (p.ep & 7))) * 16777619u;
  return h;
}

uint32_t chessPerft(ChessPos& p, int depth) {
  if (depth == 0) return 1;
  uint32_t pseudo[CHESS_MAX_MOVES];
  const int n = genPseudo(p, pseudo, CHESS_MAX_MOVES, false);
  const uint8_t mover = p.side;
  uint32_t total = 0;
  for (int i = 0; i < n; i++) {
    const uint16_t m = (uint16_t)pseudo[i];
    ChessUndo u;
    chessMake(p, m, u);
    if (!chessAttacked(p, p.king[mover], mover ^ 1)) total += chessPerft(p, depth - 1);
    chessUnmake(p, m, u);
  }
  return total;
}

// ---------------------------------------------------------------------------
// Evaluation: material and where the pieces stand (the simplified evaluation
// tables, written from White's side with the 8th rank first)
// ---------------------------------------------------------------------------
static const int8_t PST_PAWN[64] PROGMEM = {
   0,  0,  0,  0,  0,  0,  0,  0,
  50, 50, 50, 50, 50, 50, 50, 50,
  10, 10, 20, 30, 30, 20, 10, 10,
   5,  5, 10, 25, 25, 10,  5,  5,
   0,  0,  0, 20, 20,  0,  0,  0,
   5, -5,-10,  0,  0,-10, -5,  5,
   5, 10, 10,-20,-20, 10, 10,  5,
   0,  0,  0,  0,  0,  0,  0,  0};
static const int8_t PST_KNIGHT[64] PROGMEM = {
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50};
static const int8_t PST_BISHOP[64] PROGMEM = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -20,-10,-10,-10,-10,-10,-10,-20};
static const int8_t PST_ROOK[64] PROGMEM = {
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10, 10, 10, 10, 10,  5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   0,  0,  0,  5,  5,  0,  0,  0};
static const int8_t PST_QUEEN[64] PROGMEM = {
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5,  5,  5,  5,  0,-10,
  -5,  0,  5,  5,  5,  5,  0, -5,
   0,  0,  5,  5,  5,  5,  0, -5,
 -10,  5,  5,  5,  5,  5,  0,-10,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20};
static const int8_t PST_KING[64] PROGMEM = {
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -10,-20,-20,-20,-20,-20,-20,-10,
  20, 20,  0,  0,  0,  0, 20, 20,
  20, 30, 10,  0,  0, 10, 30, 20};
static const int8_t PST_KING_END[64] PROGMEM = {
 -50,-40,-30,-20,-20,-30,-40,-50,
 -30,-20,-10,  0,  0,-10,-20,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-30,  0,  0,  0,  0,-30,-30,
 -50,-30,-30,-30,-30,-30,-30,-50};

static const int8_t* const PST[7] = {nullptr, PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, PST_KING};

// From the point of view of the side to move
static int evaluate(const ChessPos& p) {
  int score = 0, nonPawn = 0, bishops[2] = {0, 0};
  for (int s = 0; s < 128; s++) {
    if (s & 0x88) { s += 7; continue; }
    const uint8_t pc = p.sq[s];
    if (!pc) continue;
    const uint8_t t = pc & 7;
    if (t != CH_P && t != CH_K) nonPawn += VALUE[t];
  }
  const bool endgame = nonPawn < 2600;
  for (int s = 0; s < 128; s++) {
    if (s & 0x88) { s += 7; continue; }
    const uint8_t pc = p.sq[s];
    if (!pc) continue;
    const uint8_t t = pc & 7, black = (pc & CH_BLACK) ? 1 : 0;
    const int r = s >> 4, f = s & 7;
    const int idx = black ? r * 8 + f : (7 - r) * 8 + f;
    const int8_t* table = (t == CH_K && endgame) ? PST_KING_END : PST[t];
    int v = VALUE[t] + (int8_t)pgm_read_byte(&table[idx]);
    if (t == CH_B) bishops[black]++;
    score += black ? -v : v;
  }
  if (bishops[0] >= 2) score += 30;
  if (bishops[1] >= 2) score -= 30;
  return p.side ? -score : score;
}

// ---------------------------------------------------------------------------
// Search: negamax with alpha-beta, check extensions and a capture search at the
// leaves, run deeper and deeper until the time is up.
// ---------------------------------------------------------------------------
#define SEARCH_PLY 14
#define SEARCH_MOVES 80
static const int INF = 32000, MATE = 30000;

struct Work {
  ChessPos pos;
  uint32_t moves[SEARCH_PLY][SEARCH_MOVES];
  uint32_t path[SEARCH_PLY + 1];
};
unsigned chessWorkSize() { return sizeof(Work); }

struct Search {
  Work* w;
  const ChessHooks* hooks;
  const uint32_t* hist;
  int histLen;
  unsigned long deadline;
  bool timeUp;
  uint32_t nodes;
};

static void tick(Search& s) {
  if ((++s.nodes & 255) == 0) {
    if (s.hooks->poll) s.hooks->poll();
    if ((long)(s.hooks->now() - s.deadline) >= 0) s.timeUp = true;
  }
}

// Has this position come up before, in the search or in the game so far?
static bool repeated(Search& s, int ply) {
  ChessPos& p = s.w->pos;
  if (p.half < 4) { s.w->path[ply] = 0; return false; }
  const uint32_t h = chessHash(p);
  s.w->path[ply] = h;
  for (int k = 2; k <= p.half; k += 2) {
    const int at = ply - k;
    if (at >= 0) { if (s.w->path[at] == h) return true; }
    else {
      const int i = s.histLen - 1 + at;
      if (i < 0) break;
      if (s.hist[i] == h) return true;
    }
  }
  return false;
}

static inline void pickBest(uint32_t* m, int n, int i) {
  int best = i;
  for (int j = i + 1; j < n; j++) if ((m[j] >> 16) > (m[best] >> 16)) best = j;
  if (best != i) { uint32_t t = m[i]; m[i] = m[best]; m[best] = t; }
}

static int quiesce(Search& s, int alpha, int beta, int ply) {
  ChessPos& p = s.w->pos;
  tick(s);
  if (s.timeUp) return 0;
  int stand = evaluate(p);
  if (ply >= SEARCH_PLY - 1) return stand;
  if (stand >= beta) return beta;
  if (stand > alpha) alpha = stand;

  uint32_t* moves = s.w->moves[ply];
  const int n = genPseudo(p, moves, SEARCH_MOVES, true);
  const uint8_t mover = p.side;
  for (int i = 0; i < n; i++) {
    pickBest(moves, n, i);
    const uint16_t m = (uint16_t)moves[i];
    ChessUndo u;
    chessMake(p, m, u);
    if (chessAttacked(p, p.king[mover], mover ^ 1)) { chessUnmake(p, m, u); continue; }
    const int score = -quiesce(s, -beta, -alpha, ply + 1);
    chessUnmake(p, m, u);
    if (s.timeUp) return 0;
    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
  }
  return alpha;
}

static int negamax(Search& s, int depth, int alpha, int beta, int ply) {
  ChessPos& p = s.w->pos;
  tick(s);
  if (s.timeUp) return 0;
  if (p.half >= 100) return 0;
  if (ply > 0 && repeated(s, ply)) return 0;
  if (ply >= SEARCH_PLY - 2) return evaluate(p);

  const uint8_t mover = p.side;
  const bool check = chessInCheck(p, mover);
  if (check) depth++;
  if (depth <= 0) return quiesce(s, alpha, beta, ply);

  uint32_t* moves = s.w->moves[ply];
  const int n = genPseudo(p, moves, SEARCH_MOVES, false);
  int legal = 0;
  for (int i = 0; i < n; i++) {
    pickBest(moves, n, i);
    const uint16_t m = (uint16_t)moves[i];
    ChessUndo u;
    chessMake(p, m, u);
    if (chessAttacked(p, p.king[mover], mover ^ 1)) { chessUnmake(p, m, u); continue; }
    legal++;
    const int score = -negamax(s, depth - 1, -beta, -alpha, ply + 1);
    chessUnmake(p, m, u);
    if (s.timeUp) return 0;
    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
  }
  if (!legal) return check ? -MATE + ply : 0;
  return alpha;
}

// ---------------------------------------------------------------------------
// Choosing a move
// ---------------------------------------------------------------------------
static int captureValue(const ChessPos& p, uint16_t m) {
  const uint8_t to = chessTo88(chessMoveTo(m));
  int v = VALUE[p.sq[to] & 7];
  if (!p.sq[to] && (p.sq[chessTo88(chessMoveFrom(m))] & 7) == CH_P && to == (uint8_t)p.ep && p.ep >= 0) v = VALUE[CH_P];
  return v;
}

uint16_t chessChooseMove(const ChessPos& p, const ChessAiLevel& lv, const uint32_t* hist, int histLen,
                         void* work, const ChessHooks& hooks) {
  uint16_t legal[CHESS_MAX_MOVES];
  const int n = chessLegalMoves(p, legal, CHESS_MAX_MOVES);
  if (n == 0) return 0;
  if (n == 1) return legal[0];

  // Beginners and the odd careless move: a random move, or (depth 0) the biggest capture on offer
  if (lv.randomPct && hooks.random(100) < lv.randomPct) return legal[hooks.random(n)];
  if (lv.depth == 0 || !work) {
    int best = -1, nb = 0, pick = 0;
    for (int i = 0; i < n; i++) {
      const int v = captureValue(p, legal[i]);
      if (v > best) { best = v; nb = 0; }
      if (v == best) { if (hooks.random(++nb) == 0) pick = i; }
    }
    return best > 0 ? legal[pick] : legal[hooks.random(n)];
  }

  Search s;
  s.w = (Work*)work;
  s.hooks = &hooks;
  s.hist = hist;
  s.histLen = histLen;
  s.timeUp = false;
  s.nodes = 0;
  s.deadline = hooks.now() + lv.ms;
  ChessPos& pos = s.w->pos;
  pos = p;
  s.w->path[0] = (p.half >= 4) ? chessHash(p) : 0;

  // Root moves, best-looking first
  int16_t score[CHESS_MAX_MOVES];            // 16 bits: the console's stack is only 4 KB
  for (int i = 0; i < n; i++) score[i] = (int16_t)captureValue(p, legal[i]);
  for (int i = 1; i < n; i++) {              // insertion sort, highest first
    const uint16_t m = legal[i]; const int16_t sc = score[i];
    int j = i - 1;
    while (j >= 0 && score[j] < sc) { legal[j + 1] = legal[j]; score[j + 1] = score[j]; j--; }
    legal[j + 1] = m; score[j + 1] = sc;
  }

  const uint8_t mover = p.side;
  uint16_t best = legal[0];

  if (lv.noise > 0 || lv.depth <= 2) {
    // A fixed look-ahead with every move judged on its own, plus a random error
    int bestScore = -INF;
    for (int i = 0; i < n; i++) {
      ChessUndo u;
      chessMake(pos, legal[i], u);
      int v = -negamax(s, lv.depth - 1, -INF, INF, 1);
      chessUnmake(pos, legal[i], u);
      if (s.timeUp) break;
      if (lv.noise > 0) v += (int)hooks.random(2 * lv.noise + 1) - lv.noise;
      if (v > bestScore) { bestScore = v; best = legal[i]; }
    }
    return best;
  }

  // Deeper and deeper until the depth or the time runs out
  for (int depth = 1; depth <= lv.depth; depth++) {
    int alpha = -INF;
    uint16_t depthBest = legal[0];
    int depthScore = -INF;
    for (int i = 0; i < n; i++) {
      ChessUndo u;
      chessMake(pos, legal[i], u);
      const int v = -negamax(s, depth - 1, -INF, -alpha, 1);
      chessUnmake(pos, legal[i], u);
      if (s.timeUp) break;
      score[i] = (int16_t)v;
      if (v > depthScore) { depthScore = v; depthBest = legal[i]; }
      if (v > alpha) alpha = v;
    }
    if (s.timeUp) break;                      // an unfinished depth is thrown away
    best = depthBest;
    // the best move first next time
    for (int i = 0; i < n; i++) if (legal[i] == best) { for (int j = i; j > 0; j--) legal[j] = legal[j - 1]; legal[0] = best; break; }
    if (depthScore > MATE - 100) break;       // a forced mate: no need to look further
  }
  (void)mover;
  return best;
}
