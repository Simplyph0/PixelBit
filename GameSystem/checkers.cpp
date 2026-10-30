#include "checkers.h"
#include "display.h"
#include "input.h"

CheckersGame checkersGame;

// ---------------------------------------------------------------------------
// Levels. depth 0 plays at random; the others look that many moves ahead, with
// some random error at the easy levels, and the top level thinks for as long as it is allowed.
// ---------------------------------------------------------------------------
static const char* const LEVEL_NAMES[DIFFICULTY_LEVELS] = {"VERY EASY", "EASY", "MEDIUM", "HARD", "SUPER HARD"};
static const GameOption OPT_CHECKERS_LEVEL = {"DIFFICULTY", DIFFICULTY_LEVELS, LEVEL_NAMES, 1, true};

static const CkAiLevel AI_LEVEL[DIFFICULTY_LEVELS] = {
  {0, 0, 0, 100},       // VERY EASY: random moves (captures are compulsory, so it takes what it must)
  {2, 150, 40, 15},     // EASY: two moves ahead, often careless
  {4, 400, 12, 3},      // MEDIUM
  {7, 1200, 0, 0},      // HARD
  {14, 4000, 0, 0}      // SUPER HARD: as far ahead as four seconds allow
};

static unsigned long hookNow() { return millis(); }
static void hookPoll() { yield(); }
static uint32_t hookRandom(uint32_t n) { return (uint32_t)random((long)n); }
static const CkHooks HOOKS = {hookNow, hookPoll, hookRandom};

static const int BOARD_X = 0, CELL = 8, PANEL_X = 68;
static const unsigned long END_SHOWN_MS = 2200;

const GameOption* CheckersGame::option(int i) { return i == 0 ? &OPT_MODE_AI : &OPT_CHECKERS_LEVEL; }

// ---------------------------------------------------------------------------
// Setting up
// ---------------------------------------------------------------------------
void CheckersGame::init() {
  exitRequested = false;
  wantRematch = peerRematch = false;
  gotState = false;
  gameNo = 0;
  if (mode == 1) {
    phase = PH_LOBBY;
    lobby.begin(GAME_CHECKERS, "CHECKERS");
  } else {
    phase = PH_LOCAL;
    myColor = 0;
    newGame();
  }
}

// A new game in the same mode: the sides swap
void CheckersGame::restart() {
  if (mode == 0) {
    exitRequested = false;
    phase = PH_LOCAL;
    gameNo++;
    myColor = gameNo & 1;
    newGame();
  } else {
    init();
  }
}

void CheckersGame::newGame() {
  ckStart(pos);
  steps = 0;
  lastStep = 0;
  over = false;
  winner = -1;
  thinkingShown = false;
  wantStep = 0;
  wantRematch = peerRematch = false;
  refresh();
  nextAt = millis() + 500;
}

// Where a square is on the screen, in reading order: this console's side is at the bottom
uint8_t CheckersGame::screenKey(uint8_t sq) const {
  uint8_t row = sq >> 3, col = sq & 7;
  if (myColor) { row = 7 - row; col = 7 - col; }
  return row * 8 + col;
}

void CheckersGame::sortByScreen(uint8_t* sq, int n) const {
  for (int i = 1; i < n; i++) {
    uint8_t v = sq[i];
    int j = i - 1;
    while (j >= 0 && screenKey(sq[j]) > screenKey(v)) { sq[j + 1] = sq[j]; j--; }
    sq[j + 1] = v;
  }
}

// After every step: what can move, who won, and where the player's choice starts
void CheckersGame::refresh() {
  legalCount = ckSteps(pos, legal, CK_MAX_STEPS);
  result = ckResultOf(pos);
  bool was = over;
  over = result != CK_PLAYING;
  winner = result == CK_WIN0 ? 0 : result == CK_WIN1 ? 1 : -1;
  if (over && !was) nextAt = millis() + END_SHOWN_MS;

  pieceCount = 0;
  for (int i = 0; i < legalCount; i++) {
    uint8_t from = ckFrom(legal[i]);
    bool seen = false;
    for (int k = 0; k < pieceCount; k++) seen |= pieces[k] == from;
    if (!seen && pieceCount < 12) pieces[pieceCount++] = from;
  }
  sortByScreen(pieces, pieceCount);
  pieceIdx = 0;
  stage = ST_PIECE;
  if (pos.chain != CK_NO_CHAIN && myTurn()) { buildDests(); stage = ST_DEST; }   // the piece must jump on
}

void CheckersGame::buildDests() {
  uint8_t to[4];
  uint16_t by[4];
  destCount = 0;
  for (int i = 0; i < legalCount && destCount < 4; i++) {
    if (ckFrom(legal[i]) != pieces[pieceIdx]) continue;
    to[destCount] = ckTo(legal[i]);
    by[destCount++] = legal[i];
  }
  for (int i = 1; i < destCount; i++) {          // reading order of where they go
    uint8_t t = to[i];
    uint16_t b = by[i];
    int j = i - 1;
    while (j >= 0 && screenKey(to[j]) > screenKey(t)) { to[j + 1] = to[j]; by[j + 1] = by[j]; j--; }
    to[j + 1] = t;
    by[j + 1] = b;
  }
  for (int i = 0; i < destCount; i++) dests[i] = by[i];
  destIdx = 0;
}

bool CheckersGame::playStep(uint16_t step) {
  if (!ckPlay(pos, step)) return false;
  steps++;
  lastStep = step;
  refresh();
  return true;
}

uint16_t CheckersGame::thinkAi() {
  void* work = malloc(ckWorkSize());     // only for the length of the search; null just plays a random step
  uint16_t s = ckChooseStep(pos, AI_LEVEL[level], work, HOOKS);
  free(work);
  return s;
}

// ---------------------------------------------------------------------------
// Input and turns
// ---------------------------------------------------------------------------
uint16_t CheckersGame::handleInput() {
  int dir = buttons.rightPressed ? 1 : buttons.leftPressed ? -1 : 0;
  if (stage == ST_PIECE) {
    if (!pieceCount) return 0;
    if (dir) pieceIdx = (pieceIdx + pieceCount + dir) % pieceCount;
    if (buttons.upPressed) { buildDests(); stage = ST_DEST; }
  } else if (stage == ST_DEST) {
    if (dir && destCount) destIdx = (destIdx + destCount + dir) % destCount;
    if (buttons.downPressed && pos.chain == CK_NO_CHAIN) stage = ST_PIECE;
    else if (buttons.upPressed && destCount) return dests[destIdx];
  }
  return 0;
}

void CheckersGame::updateLocal() {
  if (over || millis() < nextAt) return;
  if (myTurn()) {
    uint16_t s = handleInput();
    if (s) playStep(s);
  } else {
    if (!thinkingShown) { thinkingShown = true; return; }      // let "THINKING" be on the screen first
    thinkingShown = false;
    uint16_t s = thinkAi();
    if (s) playStep(s);
    nextAt = millis() + 300;                                   // a multiple jump shows one jump at a time
  }
}

// ---------------------------------------------------------------------------
// Two consoles. The host decides and sends the whole position each frame; the
// joiner keeps asking for its step until the position shows it.
// ---------------------------------------------------------------------------
bool CheckersGame::linkAlive() {
  LinkState s = gameLink.state();
  if (s == LINK_CONNECTED) return true;
  lobby.fail(s == LINK_PEER_LEFT ? "OPPONENT LEFT" : "CONNECTION LOST", "The match has ended", "");
  phase = PH_LOBBY;
  return false;
}

// Dark squares in reading order, 3 bits each
static void packBoard(const CkPos& p, uint8_t* out) {
  memset(out, 0, 12);
  int i = 0;
  for (int s = 0; s < 64; s++) {
    if ((((s >> 3) + (s & 7)) & 1) == 0) continue;
    for (int b = 0; b < 3; b++) if ((p.sq[s] >> b) & 1) out[(3 * i + b) / 8] |= 1 << ((3 * i + b) % 8);
    i++;
  }
}

void CheckersGame::unpackBoard(const uint8_t* in) {
  memset(pos.sq, 0, sizeof(pos.sq));
  int i = 0;
  for (int s = 0; s < 64; s++) {
    if ((((s >> 3) + (s & 7)) & 1) == 0) continue;
    uint8_t v = 0;
    for (int b = 0; b < 3; b++) v |= ((in[(3 * i + b) / 8] >> ((3 * i + b) % 8)) & 1) << b;
    pos.sq[s] = v > CK_KING1 ? CK_EMPTY : v;      // a damaged packet cannot make an unknown piece
    i++;
  }
}

void CheckersGame::updateHost() {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 6 && in[1] == gameNo) {
    peerRematch = in[0] & 1;
    uint16_t seen = in[2] | (in[3] << 8), want = in[4] | (in[5] << 8);
    // the joiner moves only on its turn, for the position it saw, and only legally
    if (want && seen == steps && !over && pos.side != myColor) playStep(want);
  }

  if (over) {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
    if (wantRematch && peerRematch) {
      gameNo++;
      myColor = gameNo & 1;
      newGame();
    }
  } else if (myTurn()) {
    uint16_t s = handleInput();
    if (s) playStep(s);
  }

  uint8_t out[21];
  out[0] = wantRematch ? 1 : 0;
  out[1] = gameNo;
  out[2] = steps;
  out[3] = steps >> 8;
  out[4] = pos.side;
  out[5] = pos.chain;
  out[6] = pos.quiet;
  packBoard(pos, out + 7);
  out[19] = lastStep;
  out[20] = lastStep >> 8;
  gameLink.send(out, sizeof(out));
}

void CheckersGame::updateClient() {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 21) {
    bool fresh = false;
    if (!gotState || in[1] != gameNo) {      // a new game: the host's number says which side we are
      gameNo = in[1];
      myColor = (gameNo & 1) ? 0 : 1;
      newGame();
      gotState = true;
      fresh = true;
    }
    peerRematch = in[0] & 1;
    uint16_t host = in[2] | (in[3] << 8);
    if (host != steps || fresh) {
      steps = host;
      pos.side = in[4] & 1;
      pos.chain = in[5];
      pos.quiet = in[6];
      unpackBoard(in + 7);
      lastStep = in[19] | (in[20] << 8);
      refresh();
    }
  }

  if (gotState) {
    if (over) {
      if (buttons.rightPressed) wantRematch = true;
      if (buttons.upPressed) exitRequested = true;
    } else if (myTurn() && stage != ST_WAIT) {
      uint16_t s = handleInput();
      if (s) { wantStep = s; wantSince = millis(); stage = ST_WAIT; }
    }
    if (stage == ST_WAIT && !over && millis() - wantSince > 3000) { wantStep = 0; stage = ST_PIECE; }   // the host did not take it: choose again
    if (stage != ST_WAIT) wantStep = 0;
  }

  uint8_t out[6];
  out[0] = wantRematch ? 1 : 0;
  out[1] = gameNo;
  out[2] = steps;
  out[3] = steps >> 8;
  out[4] = wantStep;
  out[5] = wantStep >> 8;
  gameLink.send(out, sizeof(out));
}

void CheckersGame::update() {
  switch (phase) {
    case PH_LOBBY: {
      LobbyResult r = lobby.update();
      if (r == LOBBY_BACK) exitRequested = true;
      else if (r == LOBBY_HOST || r == LOBBY_CLIENT) {
        phase = (r == LOBBY_HOST) ? PH_HOST : PH_CLIENT;
        gameNo = 0;
        myColor = (r == LOBBY_HOST) ? 0 : 1;
        gotState = false;
        newGame();
      }
      break;
    }
    case PH_LOCAL:  updateLocal(); break;
    case PH_HOST:   updateHost(); break;
    case PH_CLIENT: updateClient(); break;
  }
}

int CheckersGame::getScore() {
  if (!over) return 0;
  if (winner == (int8_t)myColor) return 3 * (level + 1);
  if (winner < 0) return level + 1;
  return 0;
}

// ---------------------------------------------------------------------------
// Drawing. My pieces are solid discs, the other side's are rings; a king has a dot in the middle.
// ---------------------------------------------------------------------------
void CheckersGame::drawPiece(int cx, int cy, uint8_t piece) {
  bool king = (piece & 1) == 0;
  display.fillCircle(cx, cy, 3, SSD1306_BLACK);
  if (ckSideOf(piece) == myColor) {
    display.fillCircle(cx, cy, 3, SSD1306_WHITE);
    if (king) display.fillCircle(cx, cy, 1, SSD1306_BLACK);
  } else {
    display.drawCircle(cx, cy, 3, SSD1306_WHITE);
    if (king) display.fillCircle(cx, cy, 1, SSD1306_WHITE);
  }
}

static void cellXY(uint8_t key, int& x, int& y) {
  x = BOARD_X + (key & 7) * CELL;
  y = (key >> 3) * CELL;
}

void CheckersGame::drawBoard() {
  bool blink = (millis() / 300) % 2;
  int x, y;
  for (int sq = 0; sq < 64; sq++) {
    if ((((sq >> 3) + (sq & 7)) & 1) == 0) continue;
    cellXY(screenKey(sq), x, y);
    for (int py = 0; py < CELL; py += 2)                   // the dark squares are dotted
      for (int px = 0; px < CELL; px += 2) display.drawPixel(x + px, y + py, SSD1306_WHITE);
    if (pos.sq[sq]) drawPiece(x + 3, y + 3, pos.sq[sq]);
  }
  if (lastStep) {                                          // a mark on the square the last piece left
    cellXY(screenKey(ckFrom(lastStep)), x, y);
    display.fillRect(x + 3, y + 3, 2, 2, SSD1306_WHITE);
  }
  display.drawFastVLine(BOARD_X + 8 * CELL, 0, SCREEN_HEIGHT, SSD1306_WHITE);

  if (!myTurn() || (stage == ST_WAIT)) return;
  if (stage == ST_PIECE && pieceCount) {
    cellXY(screenKey(pieces[pieceIdx]), x, y);
    if (blink) display.drawRect(x, y, CELL, CELL, SSD1306_WHITE);
  } else if (stage == ST_DEST && destCount) {
    cellXY(screenKey(pieces[pieceIdx]), x, y);
    display.drawRect(x, y, CELL, CELL, SSD1306_WHITE);
    cellXY(screenKey(ckTo(dests[destIdx])), x, y);
    if (blink) display.fillRect(x + 2, y + 2, 4, 4, SSD1306_WHITE);
  }
}

void CheckersGame::drawPanel() {
  int mine = 0, theirs = 0;
  for (int s = 0; s < 64; s++) if (pos.sq[s]) { if (ckSideOf(pos.sq[s]) == myColor) mine++; else theirs++; }
  drawPiece(PANEL_X + 3, 3, myColor == 0 ? CK_MAN0 : CK_MAN1);
  display.setCursor(PANEL_X + 12, 0);
  display.print(F("YOU x"));
  display.print(mine);
  drawPiece(PANEL_X + 3, 13, myColor == 0 ? CK_MAN1 : CK_MAN0);
  display.setCursor(PANEL_X + 12, 10);
  display.print(phase == PH_LOCAL ? F("CPU x") : F("OPP x"));
  display.print(theirs);

  const char* l1 = "", *l2 = "", *l3 = "";
  if (over) {
    l1 = winner == (int8_t)myColor ? "YOU WIN!" : winner < 0 ? "DRAW" : "YOU LOSE";
    if (phase != PH_LOCAL) {
      l2 = "UP: LEAVE";
      l3 = wantRematch ? "WAITING..." : peerRematch ? "THEY WANT IT" : "RIGHT: AGAIN";
    }
  } else if (stage == ST_WAIT) {
    l1 = "SENDING..";
  } else if (myTurn()) {
    l1 = pos.chain != CK_NO_CHAIN ? "JUMP ON!" : "YOUR MOVE";
    l2 = stage == ST_PIECE ? "<> UP:PICK" : "<> UP:GO";
    l3 = stage == ST_DEST && pos.chain == CK_NO_CHAIN ? "DOWN:BACK" : "";
  } else {
    l1 = phase == PH_LOCAL ? "THINKING.." : "THEIR MOVE";
  }
  display.setCursor(PANEL_X, 30);
  display.print(l1);
  display.setCursor(PANEL_X, 42);
  display.print(l2);
  display.setCursor(PANEL_X, 52);
  display.print(l3);
}

void CheckersGame::draw() {
  if (phase == PH_LOBBY) { lobby.draw(); return; }
  if (phase == PH_CLIENT && !gotState) {
    drawMessageScreen("CONNECTED", "", "Waiting for the host", "to start...", "");
    return;
  }
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  drawBoard();
  drawPanel();
}
