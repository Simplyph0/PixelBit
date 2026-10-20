#ifndef CHESSENGINE_H
#define CHESSENGINE_H

#include <stdint.h>

// The rules of chess and a small search, with nothing from the Arduino core in
// it, so the PC tests run the very same code. Used by chess.cpp.
//
// The board is a 0x88 array: square = rank * 16 + file, rank 0 is White's back
// rank, file 0 is the a-file. A piece is its type (1 pawn .. 6 king) plus 8 for
// Black. A move is 15 bits: from (0..63) | to (0..63) << 6 | promotion << 12, where
// the squares are rank * 8 + file and the promotion is a piece type (2..5) or 0.
// Castling is a king move of two files; en passant is a pawn moving diagonally
// onto the en passant square. Both are worked out from the move itself.

enum : uint8_t { CH_EMPTY = 0, CH_P = 1, CH_N = 2, CH_B = 3, CH_R = 4, CH_Q = 5, CH_K = 6, CH_BLACK = 8 };

struct ChessPos {
  uint8_t sq[128];
  uint8_t side;        // 0 White to move, 1 Black
  uint8_t castle;      // bit 0 White kingside, 1 White queenside, 2 Black kingside, 3 Black queenside
  int8_t ep;           // en passant target square (0x88), or -1
  uint8_t half;        // half moves since a capture or a pawn move
  uint8_t king[2];     // king squares (0x88)
};

struct ChessUndo {
  uint8_t captured;
  uint8_t castle;
  int8_t ep;
  uint8_t half;
  bool epCapture;
};

enum ChessResult : uint8_t { CR_PLAYING, CR_CHECKMATE, CR_STALEMATE, CR_INSUFFICIENT, CR_FIFTY, CR_REPETITION };

#define CHESS_MAX_MOVES 128

// Square helpers: 0..63 <-> 0x88
inline uint8_t chessTo88(uint8_t s) { return (uint8_t)(((s >> 3) << 4) | (s & 7)); }
inline uint8_t chessFrom88(uint8_t s) { return (uint8_t)(((s >> 4) << 3) | (s & 7)); }
inline uint16_t chessMove(uint8_t from64, uint8_t to64, uint8_t promo = 0) { return (uint16_t)(from64 | (to64 << 6) | (promo << 12)); }
inline uint8_t chessMoveFrom(uint16_t m) { return m & 63; }
inline uint8_t chessMoveTo(uint16_t m) { return (m >> 6) & 63; }
inline uint8_t chessMovePromo(uint16_t m) { return (m >> 12) & 7; }

void chessStart(ChessPos& p);
bool chessAttacked(const ChessPos& p, uint8_t sq88, uint8_t byColour);   // colour 0 White, 1 Black
inline bool chessInCheck(const ChessPos& p, uint8_t colour) { return chessAttacked(p, p.king[colour], colour ^ 1); }

void chessMake(ChessPos& p, uint16_t move, ChessUndo& u);
void chessUnmake(ChessPos& p, uint16_t move, const ChessUndo& u);

// Every legal move; returns how many (at most max)
int chessLegalMoves(const ChessPos& p, uint16_t* out, int max);
// Plays the move if it is legal
bool chessPlay(ChessPos& p, uint16_t move);

ChessResult chessResultOf(const ChessPos& p, int legalCount);   // CR_PLAYING if the game goes on
uint32_t chessHash(const ChessPos& p);                          // position identity, for repetitions
uint32_t chessPerft(ChessPos& p, int depth);                    // for the tests

// ---- the computer player -------------------------------------------------

struct ChessAiLevel {
  uint8_t depth;        // 0: no search (random or greedy), else how many half moves it may look ahead
  uint16_t ms;          // time limit for the whole search
  int16_t noise;        // up to this many centipawns of random error on every move it weighs
  uint8_t randomPct;    // how often it just plays a random move (depth 0: random, else greedy capture)
};

struct ChessHooks {
  unsigned long (*now)();           // milliseconds
  void (*poll)();                   // called now and then during a long search (yield on the console)
  uint32_t (*random)(uint32_t n);   // 0 .. n - 1
};

// hist: hashes of the positions played so far (to spot repetitions). Returns the move, or 0 if there is none.
// work: scratch for the search (chessWorkSize() bytes), or null to play a random legal move.
uint16_t chessChooseMove(const ChessPos& p, const ChessAiLevel& level, const uint32_t* hist, int histLen,
                         void* work, const ChessHooks& hooks);
unsigned chessWorkSize();

#endif
