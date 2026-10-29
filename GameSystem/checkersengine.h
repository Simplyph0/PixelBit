#ifndef CHECKERSENGINE_H
#define CHECKERSENGINE_H

#include <stdint.h>

// The rules of checkers (American / English draughts) and a small search, with
// nothing from the Arduino core in it, so the PC tests run the very same code.
// Used by checkers.cpp.
//
// 8 x 8 board, the dark squares are the ones where row + column is odd; square =
// row * 8 + column and row 0 is at the top of the screen. Side 0 starts at the
// bottom and moves first, side 1 starts at the top. Men move one square diagonally
// forward, kings both ways. A capture is a jump over an enemy piece and is
// compulsory; a piece that can jump again keeps jumping. A man that reaches the
// far row becomes a king and its move ends. You lose when you cannot move.
//
// A move is played one step at a time: a step is from | to << 6. After a jump that
// can go on, the position keeps the same side to move and `chain` holds the square
// of the piece that must continue.

enum : uint8_t { CK_EMPTY = 0, CK_MAN0 = 1, CK_KING0 = 2, CK_MAN1 = 3, CK_KING1 = 4 };

#define CK_NO_CHAIN 0xFF
#define CK_DRAW_QUIET 80       // half moves without a capture or a man moving: a draw
#define CK_MAX_STEPS 48

struct CkPos {
  uint8_t sq[64];
  uint8_t side;        // 0 or 1, who is to move
  uint8_t chain;       // square of the piece in the middle of a multiple jump, or CK_NO_CHAIN
  uint8_t quiet;       // half moves since a capture or a man's move
};

enum CkResult : uint8_t { CK_PLAYING, CK_WIN0, CK_WIN1, CK_DRAW };

inline uint16_t ckStep(uint8_t from, uint8_t to) { return (uint16_t)(from | (to << 6)); }
inline uint8_t ckFrom(uint16_t s) { return s & 63; }
inline uint8_t ckTo(uint16_t s) { return (s >> 6) & 63; }
inline bool ckIsJump(uint16_t s) { int d = ((ckTo(s) >> 3) - (ckFrom(s) >> 3)); return d == 2 || d == -2; }
inline uint8_t ckSideOf(uint8_t piece) { return (piece - 1) >> 1; }   // piece != CK_EMPTY

void ckStart(CkPos& p);
// Every legal step (compulsory captures applied); returns how many
int ckSteps(const CkPos& p, uint16_t* out, int max);
// Plays the step, which must be one of ckSteps()
void ckApply(CkPos& p, uint16_t step);
// Plays the step if it is legal
bool ckPlay(CkPos& p, uint16_t step);
CkResult ckResultOf(const CkPos& p);
// Number of complete moves ahead (all the steps of a multiple jump count as one), for the tests
uint32_t ckPerft(const CkPos& p, int depth);

// ---- the computer player -------------------------------------------------

struct CkAiLevel {
  uint8_t depth;        // 0: random moves, else how many moves it may look ahead
  uint16_t ms;          // time limit for the whole search
  int16_t noise;        // up to this much random error on every move it weighs (a man is worth 100)
  uint8_t randomPct;    // how often it just plays a random move
};

struct CkHooks {
  unsigned long (*now)();           // milliseconds
  void (*poll)();                   // called now and then during a long search (yield on the console)
  uint32_t (*random)(uint32_t n);   // 0 .. n - 1
};

// The step to play next (the first of a multiple jump when the position has none in progress),
// or 0 if there is none. work: scratch for the search (ckWorkSize() bytes), or null to play at random.
uint16_t ckChooseStep(const CkPos& p, const CkAiLevel& level, void* work, const CkHooks& hooks);
unsigned ckWorkSize();

#endif
