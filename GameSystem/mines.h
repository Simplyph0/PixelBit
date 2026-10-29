#ifndef MINES_H
#define MINES_H

#include <stdint.h>

// The rules of Minesweeper, with nothing from the Arduino core in it, so the PC
// tests run the very same code. Used by minesweeper.cpp.
#define MINES_COLS 16
#define MINES_ROWS 7
#define MINES_CELLS (MINES_COLS * MINES_ROWS)

enum : uint8_t { MC_MINE = 1, MC_OPEN = 2, MC_FLAG = 4 };

struct MineField {
  uint8_t cell[MINES_CELLS];   // MC_* bits
  uint8_t mines;               // how many there are to place
  uint8_t flags;               // flags on the field
  uint8_t opened;              // safe cells opened
  bool placed;                 // mines go down on the first open, so that one is safe
  bool exploded;
};

void minesStart(MineField& f, uint8_t mines);
// Mines in the 8 cells around
uint8_t minesAround(const MineField& f, int idx);
// Opens a cell; a cell with no mines around opens its neighbours too. The first open
// places the mines away from the cell and its neighbours. Returns false on a mine.
bool minesOpen(MineField& f, int idx, uint32_t (*random)(uint32_t n));
// Opens the neighbours of an opened number whose flags match it (flags may be wrong)
bool minesChord(MineField& f, int idx, uint32_t (*random)(uint32_t n));
void minesFlag(MineField& f, int idx);
bool minesWon(const MineField& f);

#endif
