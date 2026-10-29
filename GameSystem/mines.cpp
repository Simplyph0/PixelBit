#include "mines.h"
#include <string.h>

void minesStart(MineField& f, uint8_t mines) {
  memset(&f, 0, sizeof(f));
  f.mines = mines;
}

static int neighbours(int idx, int out[8]) {
  int n = 0, r = idx / MINES_COLS, c = idx % MINES_COLS;
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      int rr = r + dr, cc = c + dc;
      if ((dr || dc) && rr >= 0 && rr < MINES_ROWS && cc >= 0 && cc < MINES_COLS) out[n++] = rr * MINES_COLS + cc;
    }
  return n;
}

uint8_t minesAround(const MineField& f, int idx) {
  int nb[8], n = neighbours(idx, nb), count = 0;
  for (int i = 0; i < n; i++) if (f.cell[nb[i]] & MC_MINE) count++;
  return count;
}

static void place(MineField& f, int safe, uint32_t (*random)(uint32_t n)) {
  int nb[8], n = neighbours(safe, nb);
  bool keep[MINES_CELLS] = {};
  keep[safe] = true;
  for (int i = 0; i < n; i++) keep[nb[i]] = true;
  int free_ = 0;
  for (int i = 0; i < MINES_CELLS; i++) if (!keep[i]) free_++;
  int left = f.mines < free_ ? f.mines : free_;   // a field too small to keep the start clear still plays
  f.mines = left;
  while (left > 0) {
    int i = random(MINES_CELLS);
    if (keep[i] || (f.cell[i] & MC_MINE)) continue;
    f.cell[i] |= MC_MINE;
    left--;
  }
  f.placed = true;
}

bool minesOpen(MineField& f, int idx, uint32_t (*random)(uint32_t n)) {
  if (idx < 0 || idx >= MINES_CELLS || (f.cell[idx] & (MC_OPEN | MC_FLAG)) || f.exploded) return !f.exploded;
  if (!f.placed) place(f, idx, random);
  if (f.cell[idx] & MC_MINE) {
    f.cell[idx] |= MC_OPEN;
    f.exploded = true;
    return false;
  }
  // flood: an explicit stack of cells to open (each cell is queued at most once)
  uint8_t stack[MINES_CELLS];
  int top = 0;
  stack[top++] = idx;
  f.cell[idx] |= MC_OPEN;
  f.opened++;
  while (top > 0) {
    int cur = stack[--top];
    if (minesAround(f, cur) != 0) continue;
    int nb[8], n = neighbours(cur, nb);
    for (int i = 0; i < n; i++) {
      if (f.cell[nb[i]] & (MC_OPEN | MC_FLAG)) continue;
      f.cell[nb[i]] |= MC_OPEN;
      f.opened++;
      stack[top++] = nb[i];
    }
  }
  return true;
}

bool minesChord(MineField& f, int idx, uint32_t (*random)(uint32_t n)) {
  if (idx < 0 || idx >= MINES_CELLS || !(f.cell[idx] & MC_OPEN) || f.exploded) return !f.exploded;
  int nb[8], n = neighbours(idx, nb), flagged = 0;
  for (int i = 0; i < n; i++) if (f.cell[nb[i]] & MC_FLAG) flagged++;
  if (flagged != minesAround(f, idx)) return true;
  bool ok = true;
  for (int i = 0; i < n; i++) ok = minesOpen(f, nb[i], random) && ok;
  return ok;
}

void minesFlag(MineField& f, int idx) {
  if (idx < 0 || idx >= MINES_CELLS || (f.cell[idx] & MC_OPEN)) return;
  f.cell[idx] ^= MC_FLAG;
  if (f.cell[idx] & MC_FLAG) f.flags++; else f.flags--;
}

bool minesWon(const MineField& f) {
  return f.placed && !f.exploded && f.opened == MINES_CELLS - f.mines;
}
