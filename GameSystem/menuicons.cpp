#include "menuicons.h"
#include "display.h"

// Each icon is a list of rectangles on a 16x16 grid: x, y, w, h, colour
// (1 = draw, 0 = erase, used for eyes and windows). Later rectangles paint
// over earlier ones. The order matches GameID in config.h.
struct IconRect { uint8_t x, y, w, h, on; };

static const IconRect ICON_SNAKE[] = {
  {4,2,9,2,1}, {4,4,2,3,1}, {4,7,8,2,1}, {10,9,2,3,1}, {3,12,9,2,1}, {13,12,2,2,1}, {11,2,1,1,0}
};
static const IconRect ICON_TETRIS[] = {
  {2,1,9,3,1}, {5,4,3,3,1}, {2,10,3,4,1}, {5,10,3,4,1}, {8,10,3,4,1}, {11,10,3,4,1}, {8,7,3,3,1}
};
static const IconRect ICON_FLAPPY[] = {
  {1,5,8,6,1}, {6,6,1,1,0}, {9,7,3,2,1}, {2,8,3,1,0}, {13,0,3,4,1}, {13,12,3,4,1}
};
static const IconRect ICON_2048[] = {
  {1,1,6,6,1}, {9,1,6,6,1}, {1,9,6,6,1}, {9,9,6,6,1}, {3,3,2,2,0}, {11,11,2,2,0}
};
static const IconRect ICON_BREAKOUT[] = {
  {1,1,4,2,1}, {6,1,4,2,1}, {11,1,4,2,1}, {1,4,4,2,1}, {6,4,4,2,1}, {11,4,4,2,1},
  {8,9,2,2,1}, {5,14,7,2,1}
};
static const IconRect ICON_FROGGER[] = {
  {4,6,8,6,1}, {4,4,3,2,1}, {9,4,3,2,1}, {2,9,2,4,1}, {12,9,2,4,1}, {5,5,1,1,0}, {10,5,1,1,0},
  {6,9,4,1,0}
};
static const IconRect ICON_HELICOPTER[] = {
  {3,7,8,5,1}, {5,8,3,2,0}, {11,8,4,2,1}, {14,6,1,3,1}, {1,5,13,1,1}, {7,6,1,1,1}, {3,13,8,1,1}
};
static const IconRect ICON_PACMAN[] = {
  {4,2,8,2,1}, {2,4,12,2,1}, {2,6,10,1,1}, {2,7,7,2,1}, {2,9,10,1,1}, {2,10,12,2,1}, {4,12,8,2,1},
  {7,4,2,1,0}, {13,7,2,2,1}
};
static const IconRect ICON_PONG[] = {
  {7,0,2,3,1}, {7,5,2,3,1}, {7,10,2,3,1}, {1,4,2,7,1}, {13,6,2,7,1}, {10,8,2,2,1}
};
static const IconRect ICON_INVADERS[] = {
  {4,2,1,2,1}, {11,2,1,2,1}, {3,4,10,2,1}, {2,6,12,3,1}, {5,6,2,2,0}, {9,6,2,2,0},
  {3,9,2,2,1}, {11,9,2,2,1}, {6,9,4,1,1}, {5,14,6,2,1}, {7,13,2,1,1}
};
static const IconRect ICON_DINO[] = {
  {9,1,6,4,1}, {12,2,1,1,0}, {5,5,6,5,1}, {9,5,4,3,1}, {2,6,4,2,1}, {6,10,2,4,1}, {9,10,2,4,1},
  {0,15,16,1,1}
};

static const IconRect ICON_TICTACTOE[] = {
  {5,1,1,14,1}, {10,1,1,14,1}, {1,5,14,1,1}, {1,10,14,1,1},
  {1,1,3,3,1}, {2,2,1,1,0}, {6,6,3,3,1}, {7,7,1,1,0}, {11,11,3,3,1}, {12,12,1,1,0}, {12,1,3,3,1}, {1,12,3,3,1}
};
static const IconRect ICON_RPS[] = {
  {2,6,6,8,1}, {2,5,2,2,1}, {4,5,2,2,1}, {6,5,2,2,1}, {3,9,1,1,0}, {5,9,1,1,0},
  {9,2,2,7,1}, {12,2,2,7,1}, {9,9,5,5,1}, {10,10,3,1,0}
};

static const IconRect ICON_CONNECT4[] = {
  {0,1,16,14,1}, {1,2,3,3,0}, {6,2,3,3,0}, {11,2,3,3,0}, {1,7,3,3,0}, {6,7,3,3,0}, {11,7,3,3,0},
  {1,11,3,3,0}, {2,12,1,1,1}, {6,11,3,3,0}, {7,12,1,1,1}, {11,11,3,3,0}, {12,12,1,1,1}
};

static const IconRect ICON_CHESS[] = {
  {3,1,2,3,1}, {7,1,2,3,1}, {11,1,2,3,1}, {3,4,10,2,1}, {5,6,6,5,1}, {3,11,10,2,1}, {2,13,12,2,1}
};

static const IconRect ICON_VIDEO[] = {
  {1,1,14,14,1}, {2,2,12,12,0}, {6,4,1,8,1}, {7,5,1,6,1}, {8,6,1,4,1}, {9,7,1,2,1}
};

static const IconRect ICON_CHAT[] = {
  {1,1,14,10,1}, {2,2,12,8,0}, {4,11,3,3,1}, {3,13,2,2,1}, {4,5,2,2,1}, {7,5,2,2,1}, {10,5,2,2,1}
};

static const IconRect ICON_SETTINGS[] = {
  {4,4,8,8,1}, {6,6,4,4,0}, {7,0,2,4,1}, {7,12,2,4,1}, {0,7,4,2,1}, {12,7,4,2,1},
  {2,2,3,3,1}, {11,2,3,3,1}, {2,11,3,3,1}, {11,11,3,3,1}
};

struct IconDef { const IconRect* rects; uint8_t count; };
#define ICON(a) { a, sizeof(a) / sizeof(a[0]) }

static const IconDef ICONS[MAX_GAMES] = {
  ICON(ICON_SNAKE), ICON(ICON_TETRIS), ICON(ICON_FLAPPY), ICON(ICON_2048), ICON(ICON_BREAKOUT),
  ICON(ICON_FROGGER), ICON(ICON_HELICOPTER), ICON(ICON_PACMAN), ICON(ICON_PONG),
  ICON(ICON_INVADERS), ICON(ICON_DINO), ICON(ICON_TICTACTOE), ICON(ICON_RPS), ICON(ICON_CONNECT4),
  ICON(ICON_CHESS),
  ICON(ICON_VIDEO), ICON(ICON_CHAT), ICON(ICON_SETTINGS)
};

void drawGameIcon(int gameId, int x, int y, int scale, bool dim) {
  if (gameId < 0 || gameId >= MAX_GAMES) return;
  const IconDef& icon = ICONS[gameId];

  for (uint8_t i = 0; i < icon.count; i++) {
    const IconRect& r = icon.rects[i];
    int rx = x + r.x * scale, ry = y + r.y * scale;
    int rw = r.w * scale, rh = r.h * scale;
    uint16_t colour = r.on ? SSD1306_WHITE : SSD1306_BLACK;

    if (!dim) {
      display.fillRect(rx, ry, rw, rh, colour);
    } else {
      for (int py = 0; py < rh; py++)
        for (int px = 0; px < rw; px++)
          if (((rx + px + ry + py) & 1) == 0) display.drawPixel(rx + px, ry + py, colour);
    }
  }
}
