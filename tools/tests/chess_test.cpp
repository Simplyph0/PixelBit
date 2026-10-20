// Checks the chess engine (GameSystem/chessengine.cpp): the rules against the standard perft
// counts, the end-of-game cases, and the computer player.
#include "chessengine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); fails++; } } while (0)

static bool fromFen(ChessPos& p, const char* fen) {
  memset(&p, 0, sizeof(p));
  int r = 7, f = 0;
  const char* c = fen;
  for (; *c && *c != ' '; c++) {
    if (*c == '/') { r--; f = 0; continue; }
    if (*c >= '1' && *c <= '8') { f += *c - '0'; continue; }
    uint8_t t = 0;
    switch (*c | 32) { case 'p': t = CH_P; break; case 'n': t = CH_N; break; case 'b': t = CH_B; break; case 'r': t = CH_R; break; case 'q': t = CH_Q; break; case 'k': t = CH_K; break; default: return false; }
    uint8_t pc = t | ((*c >= 'a') ? CH_BLACK : 0);
    p.sq[r * 16 + f] = pc;
    if (t == CH_K) p.king[(*c >= 'a') ? 1 : 0] = r * 16 + f;
    f++;
  }
  c++;
  p.side = (*c == 'b') ? 1 : 0;
  c += 2;
  p.castle = 0;
  for (; *c && *c != ' '; c++) { if (*c == 'K') p.castle |= 1; if (*c == 'Q') p.castle |= 2; if (*c == 'k') p.castle |= 4; if (*c == 'q') p.castle |= 8; }
  c++;
  p.ep = -1;
  if (*c && *c != '-') { p.ep = (c[1] - '1') * 16 + (c[0] - 'a'); c += 2; } else c++;
  c++;
  p.half = *c ? (uint8_t)atoi(c) : 0;
  return true;
}

// "e2e4", "e7e8q"
static uint16_t mv(const char* s) {
  uint8_t promo = 0;
  if (s[4]) promo = s[4] == 'q' ? CH_Q : s[4] == 'r' ? CH_R : s[4] == 'b' ? CH_B : CH_N;
  return chessMove((s[1] - '1') * 8 + (s[0] - 'a'), (s[3] - '1') * 8 + (s[2] - 'a'), promo);
}

static void perft(const char* name, const char* fen, std::vector<uint32_t> expected) {
  ChessPos p;
  fromFen(p, fen);
  for (size_t d = 0; d < expected.size(); d++) {
    uint32_t n = chessPerft(p, (int)d + 1);
    if (n != expected[d]) { printf("FAIL: perft %s depth %zu: %u, expected %u\n", name, d + 1, n, expected[d]); fails++; return; }
  }
  printf("  ok   perft %s to depth %zu\n", name, expected.size());
}

static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) % n; }
static unsigned long fakeNow() { return (unsigned long)clock() * 1000 / CLOCKS_PER_SEC; }
static ChessHooks hooks = {fakeNow, nullptr, rnd};

// Plays two sides to the end; returns +1 White won, -1 Black won, 0 draw
static int play(const ChessAiLevel& white, const ChessAiLevel& black, void* work, int* plies = nullptr) {
  ChessPos p;
  chessStart(p);
  std::vector<uint32_t> hist;
  hist.push_back(chessHash(p));
  for (int ply = 0; ply < 300; ply++) {
    uint16_t legal[CHESS_MAX_MOVES];
    int n = chessLegalMoves(p, legal, CHESS_MAX_MOVES);
    ChessResult r = chessResultOf(p, n);
    if (r == CR_CHECKMATE) { if (plies) *plies = ply; return p.side ? 1 : -1; }
    if (r != CR_PLAYING) { if (plies) *plies = ply; return 0; }
    int reps = 0;
    for (uint32_t h : hist) if (h == hist.back()) reps++;
    if (reps >= 3) { if (plies) *plies = ply; return 0; }
    uint16_t m = chessChooseMove(p, p.side ? black : white, hist.data(), (int)hist.size(), work, hooks);
    if (!chessPlay(p, m)) { printf("FAIL: the computer played an illegal move\n"); fails++; return 0; }
    hist.push_back(chessHash(p));
    if (p.sq[p.king[0]] != CH_K || p.sq[p.king[1]] != (CH_K | CH_BLACK)) { printf("FAIL: a king square is wrong\n"); fails++; return 0; }
  }
  if (plies) *plies = 300;
  return 0;
}

int main() {
  printf("rules\n");
  perft("start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {20, 400, 8902, 197281});
  perft("kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", {48, 2039, 97862});
  perft("position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {14, 191, 2812, 43238});
  perft("position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {6, 264, 9467});
  perft("position 4 mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", {6, 264, 9467});
  perft("position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379});
  perft("position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {46, 2079, 89890});

  printf("end of the game\n");
  ChessPos p;
  chessStart(p);
  const char* fools[] = {"f2f3", "e7e5", "g2g4", "d8h4"};
  for (const char* m : fools) CHECK(chessPlay(p, mv(m)), "the fool's mate moves are legal");
  uint16_t legal[CHESS_MAX_MOVES];
  CHECK(chessResultOf(p, chessLegalMoves(p, legal, CHESS_MAX_MOVES)) == CR_CHECKMATE, "fool's mate is checkmate");
  fromFen(p, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
  CHECK(chessResultOf(p, chessLegalMoves(p, legal, CHESS_MAX_MOVES)) == CR_STALEMATE, "no moves and no check is stalemate");
  fromFen(p, "8/8/8/4k3/8/8/3K4/8 w - - 0 1");
  CHECK(chessResultOf(p, chessLegalMoves(p, legal, CHESS_MAX_MOVES)) == CR_INSUFFICIENT, "king against king is a draw");
  fromFen(p, "8/8/8/4k3/8/8/3KB3/8 w - - 0 1");
  CHECK(chessResultOf(p, chessLegalMoves(p, legal, CHESS_MAX_MOVES)) == CR_INSUFFICIENT, "king and bishop against king is a draw");
  fromFen(p, "8/8/8/4k3/8/8/3KR3/8 w - - 0 1");
  CHECK(chessResultOf(p, chessLegalMoves(p, legal, CHESS_MAX_MOVES)) == CR_PLAYING, "king and rook against king goes on");
  fromFen(p, "8/8/8/4k3/8/8/3KR3/8 w - - 100 80");
  CHECK(chessResultOf(p, chessLegalMoves(p, legal, CHESS_MAX_MOVES)) == CR_FIFTY, "fifty moves without a capture or a pawn move is a draw");

  printf("special moves\n");
  fromFen(p, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  CHECK(chessPlay(p, mv("e1g1")) && p.sq[0x06] == CH_K && p.sq[0x05] == CH_R && p.sq[0x07] == 0, "White castles kingside");
  CHECK(chessPlay(p, mv("e8c8")) && p.sq[0x72] == (CH_K | CH_BLACK) && p.sq[0x73] == (CH_R | CH_BLACK) && p.sq[0x70] == 0, "Black castles queenside");
  fromFen(p, "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  chessPlay(p, mv("a1a8"));
  CHECK(!(p.castle & 2) && !(p.castle & 8), "taking a rook takes away that castling right, and so does moving one");
  fromFen(p, "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
  fromFen(p, "4k3/4r3/8/8/8/8/8/R3K2R w KQ - 0 1");
  CHECK(!chessPlay(p, mv("e1g1")), "no castling out of check");
  fromFen(p, "4k3/5r2/8/8/8/8/8/R3K2R w KQ - 0 1");
  CHECK(!chessPlay(p, mv("e1g1")), "no castling through an attacked square");
  fromFen(p, "rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
  CHECK(chessPlay(p, mv("d4e3")) && p.sq[0x34] == 0 && p.sq[0x24] == (CH_P | CH_BLACK) && p.sq[0x33] == 0, "en passant takes the pawn that moved two");
  fromFen(p, "rnbqkbnr/pppp1ppp/8/8/4p3/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  chessPlay(p, mv("d2d4"));
  CHECK(p.ep == 0x23, "a two-square pawn move sets the en passant square (d3)");
  fromFen(p, "8/P7/8/8/8/8/8/k6K w - - 0 1");
  int promos = 0;
  int n = chessLegalMoves(p, legal, CHESS_MAX_MOVES);
  for (int i = 0; i < n; i++) if (chessMovePromo(legal[i])) promos++;
  CHECK(promos == 4, "a pawn on the 7th rank has four promotions");
  CHECK(chessPlay(p, mv("a7a8n")) && p.sq[0x70] == CH_N, "under-promotion to a knight");
  // a pinned piece cannot move off the pin
  fromFen(p, "4k3/8/8/8/4r3/8/4N3/4K3 w - - 0 1");
  CHECK(!chessPlay(p, mv("e2c3")), "a pinned knight cannot move");
  // undo restores everything
  chessStart(p);
  ChessPos before = p;
  n = chessLegalMoves(p, legal, CHESS_MAX_MOVES);
  bool restored = true;
  for (int i = 0; i < n; i++) { ChessUndo u; chessMake(p, legal[i], u); chessUnmake(p, legal[i], u); if (memcmp(&p, &before, sizeof(p)) != 0) restored = false; }
  CHECK(restored, "unmaking a move restores the position");
  // repetition: the same position gives the same hash
  chessStart(p);
  uint32_t h0 = chessHash(p);
  chessPlay(p, mv("g1f3")); chessPlay(p, mv("g8f6")); chessPlay(p, mv("f3g1")); chessPlay(p, mv("f6g8"));
  CHECK(chessHash(p) == h0, "the same position has the same hash");
  chessPlay(p, mv("e2e4"));
  CHECK(chessHash(p) != h0, "a different position has a different hash");

  printf("the computer player\n");
  void* work = malloc(chessWorkSize());
  const ChessAiLevel veryEasy = {0, 0, 0, 70}, easy = {1, 200, 80, 15}, medium = {2, 300, 30, 5}, hard = {3, 400, 0, 0}, super = {4, 600, 0, 0};
  // mate in one: the back-rank mate
  fromFen(p, "6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1");
  for (const ChessAiLevel* lv : {&medium, &hard, &super}) {
    rng = 99;
    CHECK(chessChooseMove(p, *lv, nullptr, 0, work, hooks) == mv("a1a8"), "levels medium and up find a mate in one");
  }
  // mate in two (a ladder): the strong levels find it
  fromFen(p, "7k/8/8/8/8/8/1R6/R3K3 w - - 0 1");
  { ChessPos q = p; rng = 5; uint16_t m = chessChooseMove(q, super, nullptr, 0, work, hooks); CHECK(chessPlay(q, m), "the strongest level plays a legal move in a won ending"); }
  // every level always plays a legal move, in many positions
  const ChessAiLevel* levels[4] = {&veryEasy, &easy, &medium, &hard};
  bool allLegal = true;
  for (int game = 0; game < 6 && allLegal; game++) {
    ChessPos q; chessStart(q);
    for (int ply = 0; ply < 80; ply++) {
      n = chessLegalMoves(q, legal, CHESS_MAX_MOVES);
      if (chessResultOf(q, n) != CR_PLAYING) break;
      const ChessAiLevel* lv = levels[(game + ply) % 4];
      uint16_t m = chessChooseMove(q, *lv, nullptr, 0, work, hooks);
      if (!chessPlay(q, m)) { allLegal = false; break; }
    }
  }
  CHECK(allLegal, "every level always plays a legal move");
  // with no work space it still plays
  chessStart(p);
  CHECK(chessChooseMove(p, super, nullptr, 0, nullptr, hooks) != 0, "without memory for a search it still plays a move");

  // the levels are ordered: a stronger level beats a weaker one
  int win = 0, lose = 0, draw = 0;
  for (int g = 0; g < 6; g++) {
    rng = 1000 + g;
    int r = (g % 2 == 0) ? play(medium, veryEasy, work) : -play(veryEasy, medium, work);
    if (r > 0) win++; else if (r < 0) lose++; else draw++;
  }
  printf("  medium against very easy: %d won, %d drawn, %d lost\n", win, draw, lose);
  CHECK(win >= 5, "medium beats very easy almost every time");
  win = lose = draw = 0;
  for (int g = 0; g < 4; g++) {
    rng = 2000 + g;
    int r = (g % 2 == 0) ? play(hard, easy, work) : -play(easy, hard, work);
    if (r > 0) win++; else if (r < 0) lose++; else draw++;
  }
  printf("  hard against easy: %d won, %d drawn, %d lost\n", win, draw, lose);
  CHECK(win >= 3 && lose == 0, "hard beats easy");
  free(work);

  printf("%s (%d failures)\n", fails ? "SOME CHECKS FAILED" : "all chess checks passed", fails);
  return fails != 0;
}
