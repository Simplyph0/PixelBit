#include "chess.h"
#include "display.h"
#include "input.h"

ChessGame chessGame;

// ---------------------------------------------------------------------------
// Levels. depth 0 plays at random (and now and then grabs the biggest piece on
// offer); the others look that many half moves ahead, with some random error
// at the easy levels, and the top level thinks for as long as it is allowed.
// ---------------------------------------------------------------------------
static const char* const LEVEL_NAMES[DIFFICULTY_LEVELS] = {"VERY EASY", "EASY", "MEDIUM", "HARD", "SUPER HARD"};
static const GameOption OPT_CHESS_LEVEL = {"DIFFICULTY", DIFFICULTY_LEVELS, LEVEL_NAMES, 1, true};
static const char* const PIECE_STYLES[2] = {"SHAPES", "LETTERS"};
static const GameOption OPT_CHESS_PIECES = {"PIECES", 2, PIECE_STYLES, 0, false};

static const ChessAiLevel AI_LEVEL[DIFFICULTY_LEVELS] = {
  {0, 0, 0, 70},        // VERY EASY: random moves, now and then the biggest capture it can see
  {1, 300, 90, 12},     // EASY: one move ahead, often careless
  {2, 600, 35, 4},      // MEDIUM: two moves ahead
  {4, 1500, 0, 0},      // HARD: four moves ahead, a second and a half
  {8, 5000, 0, 0}       // SUPER HARD: as far ahead as five seconds allow
};

static unsigned long hookNow() { return millis(); }
static void hookPoll() { yield(); }
static uint32_t hookRandom(uint32_t n) { return (uint32_t)random((long)n); }
static const ChessHooks HOOKS = {hookNow, hookPoll, hookRandom};

static const int BOARD_X = 1, CELL = 8, PANEL_X = 68;

const GameOption* ChessGame::option(int i) { return i == 0 ? &OPT_MODE_AI : i == 1 ? &OPT_CHESS_LEVEL : &OPT_CHESS_PIECES; }

// ---------------------------------------------------------------------------
// Setting up
// ---------------------------------------------------------------------------
void ChessGame::freeBuffers() {
  free(history);
  free(hashes);
  history = nullptr;
  hashes = nullptr;
}

bool ChessGame::allocBuffers() {
  freeBuffers();
  history = (uint16_t*)malloc(sizeof(uint16_t) * CHESS_MAX_PLIES);
  hashes = (uint32_t*)malloc(sizeof(uint32_t) * (CHESS_MAX_PLIES + 1));
  if (!history || !hashes) { freeBuffers(); return false; }
  return true;
}

void ChessGame::init() {
  exitRequested = false;
  wantRematch = peerRematch = false;
  gotState = false;
  gameNo = 0;
  noMemory = !allocBuffers();
  if (noMemory) { phase = PH_LOCAL; return; }
  if (mode == 1) {
    phase = PH_LOBBY;
    lobby.begin(GAME_CHESS, "CHESS");
  } else {
    phase = PH_LOCAL;
    myColor = 0;
    newGame();
  }
}

// A new game in the same mode: the sides swap
void ChessGame::restart() {
  if (mode == 0 && !noMemory) {
    exitRequested = false;
    phase = PH_LOCAL;
    gameNo++;
    myColor = gameNo & 1;
    newGame();
  } else {
    init();
  }
}

void ChessGame::newGame() {
  chessStart(pos);
  plies = 0;
  hashes[0] = chessHash(pos);
  lastMove = 0;
  haveLast = false;
  result = CR_PLAYING;
  over = false;
  winner = -1;
  stage = ST_PIECE;
  thinkingShown = false;
  wantMove = 0xFFFF;
  wantRematch = peerRematch = false;
  nextAt = millis() + 500;
  refreshLegal();
  buildPieces();
}

void ChessGame::refreshLegal() {
  legalCount = chessLegalMoves(pos, legal, CHESS_MAX_MOVES);
}

// Where a square is on the screen, in reading order: this console's side is at the bottom
uint8_t ChessGame::screenKey(uint8_t sq64) const {
  const uint8_t file = sq64 & 7, rank = sq64 >> 3;
  const uint8_t row = myColor == 0 ? 7 - rank : rank, col = myColor == 0 ? file : 7 - file;
  return row * 8 + col;
}

void ChessGame::screenXY(uint8_t sq64, int& x, int& y) const {
  const uint8_t key = screenKey(sq64);
  x = BOARD_X + (key & 7) * CELL;
  y = (key >> 3) * CELL;
}

void ChessGame::buildPieces() {
  bool seen[64] = {false};
  pieceCount = 0;
  for (int i = 0; i < legalCount; i++) {
    const uint8_t from = chessMoveFrom(legal[i]);
    if (seen[from] || pieceCount >= 16) continue;
    seen[from] = true;
    pieces[pieceCount++] = from;
  }
  for (int i = 1; i < pieceCount; i++) {        // reading order
    const uint8_t v = pieces[i];
    int j = i - 1;
    while (j >= 0 && screenKey(pieces[j]) > screenKey(v)) { pieces[j + 1] = pieces[j]; j--; }
    pieces[j + 1] = v;
  }
  pieceIdx = 0;
}

void ChessGame::buildDests() {
  const uint8_t from = pieces[pieceIdx];
  destCount = 0;
  for (int i = 0; i < legalCount && destCount < 28; i++) {
    if (chessMoveFrom(legal[i]) != from) continue;
    const uint8_t promo = chessMovePromo(legal[i]);
    if (promo && promo != CH_Q) continue;       // a promotion shows once; the piece is asked for afterwards
    destMove[destCount++] = legal[i];
  }
  for (int i = 1; i < destCount; i++) {
    const uint16_t v = destMove[i];
    int j = i - 1;
    while (j >= 0 && screenKey(chessMoveTo(destMove[j])) > screenKey(chessMoveTo(v))) { destMove[j + 1] = destMove[j]; j--; }
    destMove[j + 1] = v;
  }
  destIdx = 0;
}

// ---------------------------------------------------------------------------
// Playing
// ---------------------------------------------------------------------------
bool ChessGame::playMove(uint16_t move) {
  if (!history || !hashes || over || plies >= CHESS_MAX_PLIES) return false;
  if (!chessPlay(pos, move)) return false;
  history[plies++] = move;
  hashes[plies] = chessHash(pos);
  lastMove = move;
  haveLast = true;
  refreshLegal();
  buildPieces();
  stage = ST_PIECE;
  checkEnd();
  return true;
}

void ChessGame::checkEnd() {
  result = chessResultOf(pos, legalCount);
  if (result == CR_PLAYING) {
    int seen = 0;
    for (int i = 0; i <= plies; i++) if (hashes[i] == hashes[plies]) seen++;
    if (seen >= 3) result = CR_REPETITION;
    else if (plies >= CHESS_MAX_PLIES) result = CR_FIFTY;
  }
  if (result != CR_PLAYING) {
    over = true;
    winner = result == CR_CHECKMATE ? (int8_t)(pos.side ^ 1) : -1;
    nextAt = millis() + 1800;
    stage = ST_WAIT;
  }
}

uint16_t ChessGame::thinkAi() {
  void* work = malloc(chessWorkSize());      // only while thinking; without it the computer still plays, just not deeply
  const uint16_t m = chessChooseMove(pos, AI_LEVEL[level], hashes, plies + 1, work, HOOKS);
  free(work);
  return m;
}

// LEFT / RIGHT step through the pieces, then through where the piece can go; UP picks, DOWN goes back
void ChessGame::handleInput() {
  const int step = buttons.rightPressed ? 1 : buttons.leftPressed ? -1 : 0;
  if (stage == ST_PIECE) {
    if (pieceCount == 0) return;
    if (step) pieceIdx = (pieceIdx + pieceCount + step) % pieceCount;
    if (buttons.upPressed) { buildDests(); stage = ST_DEST; }
  } else if (stage == ST_DEST) {
    if (destCount == 0) { stage = ST_PIECE; return; }
    if (step) destIdx = (destIdx + destCount + step) % destCount;
    if (buttons.downPressed) { stage = ST_PIECE; return; }
    if (buttons.upPressed) {
      if (chessMovePromo(destMove[destIdx])) { stage = ST_PROMO; promoIdx = 0; return; }
      const uint16_t m = destMove[destIdx];
      if (phase == PH_CLIENT) { wantMove = m; wantPly = plies; wantSince = millis(); stage = ST_WAIT; }
      else playMove(m);
    }
  } else if (stage == ST_PROMO) {
    if (step) promoIdx = (promoIdx + 4 + step) % 4;
    if (buttons.downPressed) { stage = ST_DEST; return; }
    if (buttons.upPressed) {
      static const uint8_t PROMO[4] = {CH_Q, CH_R, CH_B, CH_N};
      const uint16_t base = destMove[destIdx];
      const uint16_t m = chessMove(chessMoveFrom(base), chessMoveTo(base), PROMO[promoIdx]);
      if (phase == PH_CLIENT) { wantMove = m; wantPly = plies; wantSince = millis(); stage = ST_WAIT; }
      else playMove(m);
    }
  }
}

void ChessGame::updateLocal() {
  if (noMemory) {
    if (buttons.upPressed || buttons.downPressed || buttons.leftPressed || buttons.rightPressed) exitRequested = true;
    return;
  }
  if (over || millis() < nextAt) return;
  if (myTurn()) {
    handleInput();
  } else {
    if (!thinkingShown) { thinkingShown = true; return; }      // let "THINKING" be on the screen first
    thinkingShown = false;
    const uint16_t m = thinkAi();
    if (m) playMove(m);
    nextAt = millis() + 150;
  }
}

// ---------------------------------------------------------------------------
// Two consoles
// ---------------------------------------------------------------------------
bool ChessGame::linkAlive() {
  LinkState s = gameLink.state();
  if (s == LINK_CONNECTED) return true;
  lobby.fail(s == LINK_PEER_LEFT ? "OPPONENT LEFT" : "CONNECTION LOST", "The match has ended", "");
  phase = PH_LOBBY;
  return false;
}

void ChessGame::updateHost() {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  uint16_t joinerPly = 0;
  if (gameLink.receive(in, n) && n >= 8) {
    if (in[1] == gameNo) {
      peerRematch = in[0] & 1;
      clientPly = in[2] | (in[3] << 8);
      const uint16_t want = in[4] | (in[5] << 8), at = in[6] | (in[7] << 8);
      // the joiner moves only on its turn, for the position it saw, and only legally
      if (want != 0xFFFF && at == plies && !over && pos.side != myColor) playMove(want);
    } else {
      clientPly = 0;                         // it has not started this game yet
    }
  }
  joinerPly = clientPly;

  if (over) {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
    if (wantRematch && peerRematch) {
      gameNo++;
      myColor = gameNo & 1;
      newGame();
      clientPly = 0;
      joinerPly = 0;
    }
  } else if (myTurn()) {
    handleInput();
  }

  if (joinerPly > plies) joinerPly = 0;
  uint8_t out[7 + 16];
  const uint8_t count = (plies - joinerPly) > 8 ? 8 : (uint8_t)(plies - joinerPly);
  out[0] = wantRematch ? 1 : 0;
  out[1] = gameNo;
  out[2] = plies;
  out[3] = plies >> 8;
  out[4] = joinerPly;
  out[5] = joinerPly >> 8;
  out[6] = count;
  for (int i = 0; i < count; i++) {
    out[7 + 2 * i] = history[joinerPly + i];
    out[8 + 2 * i] = history[joinerPly + i] >> 8;
  }
  gameLink.send(out, 7 + 2 * count);
}

void ChessGame::updateClient() {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 7) {
    if (!gotState || in[1] != gameNo) {      // a new game: the host's number says which side we are
      gameNo = in[1];
      myColor = (gameNo & 1) ? 0 : 1;
      newGame();
      gotState = true;
    }
    peerRematch = in[0] & 1;
    const uint16_t start = in[4] | (in[5] << 8);
    const uint8_t count = in[6];
    if (start == plies && n >= 7 + 2 * count) {
      for (int i = 0; i < count; i++) {
        if (!playMove(in[7 + 2 * i] | (in[8 + 2 * i] << 8))) break;
      }
    }
  }

  if (gotState) {
    if (over) {
      if (buttons.rightPressed) wantRematch = true;
      if (buttons.upPressed) exitRequested = true;
    } else if (myTurn() && stage != ST_WAIT) {
      handleInput();
    }
    if (stage == ST_WAIT && !over) {
      if (plies > wantPly) wantMove = 0xFFFF;
      else if (millis() - wantSince > 3000) { wantMove = 0xFFFF; stage = ST_PIECE; }    // the host did not take it: choose again
    }
  }

  uint8_t out[8];
  out[0] = wantRematch ? 1 : 0;
  out[1] = gameNo;
  out[2] = plies;
  out[3] = plies >> 8;
  out[4] = wantMove;
  out[5] = wantMove >> 8;
  out[6] = wantPly;
  out[7] = wantPly >> 8;
  gameLink.send(out, sizeof(out));
}

void ChessGame::update() {
  if (noMemory) { updateLocal(); return; }
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

int ChessGame::getScore() {
  if (!over) return 0;
  if (winner == (int8_t)myColor) return 3 * (level + 1);
  if (winner < 0) return level + 1;
  return 0;
}

// ---------------------------------------------------------------------------
// Drawing. The pieces are 8 x 8 pictures: White solid, Black drawn as an outline.
// ---------------------------------------------------------------------------
// 7 x 7 pictures in an 8 x 8 cell (the last row and column stay empty, so neighbouring pieces do not touch).
// White pieces are drawn lit on the dark square. Black pieces are drawn dark on a lit square, so the two sides
// cannot be mixed up even at a glance.
static const uint8_t SPRITE[6][8] = {
  {0b00111000, 0b00111000, 0b00010000, 0b00111000, 0b00111000, 0b01111100, 0b11111110, 0b00000000},   // pawn
  {0b00111000, 0b01111110, 0b11111100, 0b11011100, 0b10011100, 0b00011100, 0b00111110, 0b00000000},   // knight
  {0b00010000, 0b00111000, 0b01101100, 0b01111100, 0b00111000, 0b01111100, 0b11111110, 0b00000000},   // bishop
  {0b10101010, 0b11111110, 0b01111100, 0b01111100, 0b01111100, 0b01111100, 0b11111110, 0b00000000},   // rook
  {0b10101010, 0b11111110, 0b01111100, 0b00111000, 0b00111000, 0b01111100, 0b11111110, 0b00000000},   // queen
  {0b00010000, 0b01111100, 0b00010000, 0b01111100, 0b00111000, 0b01111100, 0b11111110, 0b00000000}    // king
};

void ChessGame::drawSprite(int x, int y, uint8_t type, bool black) {
  if (type < CH_P || type > CH_K) return;
  const uint16_t ink = black ? SSD1306_BLACK : SSD1306_WHITE;
  if (black) display.fillRect(x, y, CELL, CELL, SSD1306_WHITE);
  if (pieceStyle == 1) {                       // letters: P N B R Q K
    static const char LETTERS[] = " PNBRQK";
    display.drawChar(x + 1, y, LETTERS[type], ink, ink, 1);
    return;
  }
  const uint8_t* rows = SPRITE[type - 1];
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++)
      if ((rows[r] >> (7 - c)) & 1) display.drawPixel(x + c, y + r, ink);
}

const char* ChessGame::resultText() const {
  switch (result) {
    case CR_CHECKMATE: return "CHECKMATE";
    case CR_STALEMATE: return "STALEMATE";
    case CR_REPETITION: return "REPEATED";
    default: return "DRAW";
  }
}

void ChessGame::drawBoard() {
  const bool check = !over && chessInCheck(pos, pos.side);
  for (int sq = 0; sq < 64; sq++) {
    int x, y;
    screenXY(sq, x, y);
    const uint8_t pc = pos.sq[chessTo88(sq)];
    if (!pc && (((sq & 7) + (sq >> 3)) & 1) == 0) display.drawPixel(x + 3, y + 3, SSD1306_WHITE);   // a dark square: one dot
    if (pc) drawSprite(x, y, pc & 7, pc & CH_BLACK);
  }

  int x, y;
  if (check && ((millis() / 400) & 1)) {
    screenXY(chessFrom88(pos.king[pos.side]), x, y);
    display.drawRect(x - 1, y - 1, CELL + 2, CELL + 2, SSD1306_WHITE);
  }

  if (!myTurn() || noMemory) return;
  if (stage == ST_PIECE && pieceCount) {
    screenXY(pieces[pieceIdx], x, y);
    display.fillRect(x, y, CELL, CELL, SSD1306_INVERSE);
  } else if (stage == ST_DEST || stage == ST_PROMO) {
    screenXY(pieces[pieceIdx], x, y);
    display.drawRect(x, y, CELL, CELL, SSD1306_WHITE);
    for (int i = 0; i < destCount; i++) {
      const uint8_t to = chessMoveTo(destMove[i]);
      screenXY(to, x, y);
      if (i == destIdx) { display.fillRect(x, y, CELL, CELL, SSD1306_INVERSE); continue; }
      if (pos.sq[chessTo88(to)]) {             // a capture: ticks in the corners
        display.drawFastHLine(x, y, 3, SSD1306_INVERSE);
        display.drawFastVLine(x, y + 1, 2, SSD1306_INVERSE);
        display.drawFastHLine(x + 5, y + 7, 3, SSD1306_INVERSE);
        display.drawFastVLine(x + 7, y + 5, 2, SSD1306_INVERSE);
      } else {
        display.fillRect(x + 3, y + 3, 2, 2, SSD1306_INVERSE);
      }
    }
  }
}

void ChessGame::drawPanel() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.drawFastVLine(PANEL_X - 2, 0, SCREEN_HEIGHT, SSD1306_WHITE);
  char line[16];

  display.setCursor(PANEL_X, 0);
  display.print(phase == PH_LOCAL ? LEVEL_NAMES[level] : "2 PLAYERS");
  display.setCursor(PANEL_X, 9);
  display.print(myColor == 0 ? "YOU: WHITE" : "YOU: BLACK");

  if (over) {
    display.setCursor(PANEL_X, 19);
    display.print(resultText());
    display.setCursor(PANEL_X, 28);
    display.print(winner == (int8_t)myColor ? "YOU WIN!" : winner >= 0 ? "YOU LOSE" : "DRAW GAME");
    if (phase != PH_LOCAL) {
      display.setCursor(PANEL_X, 47);
      display.print(wantRematch ? "WAITING..." : "R:REMATCH");
      display.setCursor(PANEL_X, 56);
      display.print("UP:MENU");
    }
    return;
  }

  display.setCursor(PANEL_X, 19);
  if (myTurn()) display.print(stage == ST_WAIT ? "SENDING..." : "YOUR MOVE");
  else display.print(phase == PH_LOCAL ? "THINKING..." : "THEIR MOVE");

  display.setCursor(PANEL_X, 28);
  if (stage == ST_PROMO && myTurn()) {
    static const uint8_t PROMO[4] = {CH_Q, CH_R, CH_B, CH_N};
    for (int i = 0; i < 4; i++) {
      drawSprite(PANEL_X + i * 12, 28, PROMO[i], myColor);
      if (i == promoIdx) display.drawRect(PANEL_X + i * 12 - 2, 26, 12, 12, SSD1306_WHITE);
    }
  } else if (chessInCheck(pos, pos.side)) {
    display.print("CHECK!");
  } else if (haveLast) {
    const uint8_t f = chessMoveFrom(lastMove), t = chessMoveTo(lastMove);
    snprintf(line, sizeof(line), "%c%c-%c%c", 'A' + (f & 7), '1' + (f >> 3), 'A' + (t & 7), '1' + (t >> 3));
    display.print(line);
  }

  if (!myTurn()) return;
  display.setCursor(PANEL_X, 40);
  if (stage == ST_PIECE) {
    display.print("L/R:PIECE");
    display.setCursor(PANEL_X, 49);
    display.print("UP:SELECT");
  } else if (stage == ST_DEST) {
    display.print("L/R:MOVE");
    display.setCursor(PANEL_X, 49);
    display.print("UP:GO");
    display.setCursor(PANEL_X, 56);
    display.print("DN:BACK");
  } else if (stage == ST_PROMO) {
    display.print("L/R:CHOOSE");
    display.setCursor(PANEL_X, 49);
    display.print("UP:OK");
    display.setCursor(PANEL_X, 56);
    display.print("DN:BACK");
  }
}

void ChessGame::draw() {
  if (noMemory) {
    drawMessageScreen("CHESS", "", "Not enough memory", "to start a game", "ANY BUTTON: BACK");
    return;
  }
  if (phase == PH_LOBBY) { lobby.draw(); return; }
  if (phase == PH_CLIENT && !gotState) {
    drawMessageScreen("CONNECTED", "", "Waiting for the host", "to start...", "");
    return;
  }
  clearDisplay();
  drawBoard();
  drawPanel();
}
