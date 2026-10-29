// Checks the Minesweeper rules (GameSystem/mines.cpp): the first open is safe, the flood, flags, chords and the win.
#include "mines.h"
#include <cstdio>
#include <cstring>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); fails++; } } while (0)

static uint32_t rng = 1;
static uint32_t rnd(uint32_t n) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) % n; }

static int mineCount(const MineField& f) { int n = 0; for (int i = 0; i < MINES_CELLS; i++) if (f.cell[i] & MC_MINE) n++; return n; }

int main() {
  for (int seed = 1; seed <= 200; seed++) {                       // the first open never hits a mine, nor any around it
    rng = seed;
    MineField f;
    minesStart(f, 30);
    int first = rnd(MINES_CELLS);
    CHECK(minesOpen(f, first, rnd), "the first open is safe");
    CHECK(mineCount(f) == 30 && minesAround(f, first) == 0 && !(f.cell[first] & MC_MINE), "30 mines, none next to the first cell");
    CHECK(f.opened >= 1, "the first cell, and the empty area around it, opened");
  }

  // a hand-made field: one mine in the corner. Opening the far corner floods everything else.
  MineField f;
  minesStart(f, 1);
  f.placed = true;
  f.cell[0] |= MC_MINE;
  CHECK(minesAround(f, 1) == 1 && minesAround(f, MINES_COLS) == 1 && minesAround(f, MINES_COLS + 1) == 1 && minesAround(f, 2) == 0, "counts around the mine");
  CHECK(minesOpen(f, MINES_CELLS - 1, rnd), "the far corner is safe");
  CHECK(f.opened == MINES_CELLS - 1 && !(f.cell[0] & MC_OPEN) && minesWon(f), "the flood opens every cell but the mine: won");

  // flags, and a chord (open the cells around a number that has its flags)
  minesStart(f, 1);
  f.placed = true;
  f.cell[0] |= MC_MINE;
  minesOpen(f, 1, rnd);
  CHECK(f.opened == 1 && !minesWon(f), "a number opens alone");
  minesFlag(f, 0);
  CHECK(f.flags == 1 && (f.cell[0] & MC_FLAG), "flag");
  CHECK(minesOpen(f, 0, rnd) && !(f.cell[0] & MC_OPEN), "a flagged cell does not open");
  minesFlag(f, 1);
  CHECK(f.flags == 1, "an open cell takes no flag");
  minesFlag(f, 0);
  CHECK(f.flags == 0, "unflag");
  CHECK(minesChord(f, 1, rnd) && f.opened == 1, "a chord without its flag does nothing");
  minesFlag(f, 0);
  CHECK(minesChord(f, 1, rnd) && minesWon(f), "a chord with its flag opens the cells around and wins");

  MineField g;                                                     // the flag is on the wrong cell
  minesStart(g, 1);
  g.placed = true;
  g.cell[0] |= MC_MINE;
  minesOpen(g, 1, rnd);
  minesFlag(g, MINES_COLS + 1);
  CHECK(!minesChord(g, 1, rnd) && g.exploded && !minesWon(g), "a chord with a wrong flag opens the mine");
  CHECK(minesOpen(g, 2, rnd) == false, "nothing opens after the explosion");

  // more mines than the field can hold away from the first cell
  minesStart(f, 120);
  CHECK(minesOpen(f, 50, rnd) && mineCount(f) == f.mines && f.mines < 120, "too many mines: as many as fit");
  printf("%s\n", fails ? "mines: SOME CHECKS FAILED" : "mines: all checks passed");
  return fails != 0;
}
