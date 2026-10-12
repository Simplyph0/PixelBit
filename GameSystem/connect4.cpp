#include "connect4.h"
#include "display.h"
#include "input.h"
#include "fx.h"

Connect4Game connect4Game;

static const int BOARD_X = 36, BOARD_Y = 15, CELL = 8;   // 7 x 6 cells of 8 px: 56 x 48
static const unsigned long ROUND_PAUSE_MS = 1800;
static const unsigned long FALL_MS_PER_ROW = 45;

// How the computer plays, by level: how many moves ahead it may look, how long
// it may think (ms), and how often it just drops somewhere (percent)
static const int SEARCH_DEPTH[DIFFICULTY_LEVELS]  = {1, 3, 5, 7, 10};
static const int SEARCH_MS[DIFFICULTY_LEVELS]     = {40, 150, 300, 600, 900};
static const int BLUNDER_PCT[DIFFICULTY_LEVELS]   = {40, 15, 5, 0, 0};

void Connect4Game::init() {
  exitRequested = false;
  if (mode == 1) {
    phase = PH_LOBBY;
    lobby.begin(GAME_CONNECT4, "CONNECT 4");
    seriesOver = false;
    ptsX = ptsO = 0;
  } else {
    phase = PH_LOCAL;
    myMark = 1;
    startSeries();
  }
}

void Connect4Game::restart() {
  exitRequested = false;
  phase = PH_LOCAL;
  myMark = 1;
  startSeries();
}

void Connect4Game::startSeries() {
  round = 0;
  ptsX = ptsO = 0;
  seriesOver = false;
  wantRematch = peerRematch = false;
  pendingCol = 0xFF;
  startRound();
}

// Solid discs move first in even rounds, rings in odd ones
void Connect4Game::startRound() {
  memset(board, 0, sizeof(board));
  memset(shown, 0, sizeof(shown));
  turn = (round % 2 == 0) ? 1 : 2;
  roundResult = 0;
  animRow = -1;
  cursor = 3;
  thinkingShown = false;
  nextAt = millis() + 500;
}

uint8_t Connect4Game::discCount() {
  uint8_t n = 0;
  for (int r = 0; r < C4_ROWS; r++) for (int c = 0; c < C4_COLS; c++) if (board[r][c]) n++;
  return n;
}

// Four in a row through (row, col)? Fills the cells if so.
bool Connect4Game::findWin(const uint8_t b[C4_ROWS][C4_COLS], int row, int col, uint8_t cells[4][2]) {
  static const int8_t DIRS[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  uint8_t mark = b[row][col];
  if (!mark) return false;

  for (int d = 0; d < 4; d++) {
    int dr = DIRS[d][0], dc = DIRS[d][1];
    // walk back to the start of the run, then count forward
    int r = row, c = col;
    while (r - dr >= 0 && r - dr < C4_ROWS && c - dc >= 0 && c - dc < C4_COLS && b[r - dr][c - dc] == mark) { r -= dr; c -= dc; }
    int n = 0, sr = r, sc = c;
    while (r >= 0 && r < C4_ROWS && c >= 0 && c < C4_COLS && b[r][c] == mark) { n++; r += dr; c += dc; }
    if (n >= 4) {
      if (cells) for (int k = 0; k < 4; k++) { cells[k][0] = sr + k * dr; cells[k][1] = sc + k * dc; }
      return true;
    }
  }
  return false;
}

// Drops a disc into a column; handles the end of a round
bool Connect4Game::drop(uint8_t col, uint8_t mark) {
  if (col >= C4_COLS || roundResult || !columnFree(col)) return false;

  int row = C4_ROWS - 1;
  while (row >= 0 && board[row][col]) row--;
  board[row][col] = mark;

  if (findWin(board, row, col, winCells)) {
    roundResult = mark;
  } else if (discCount() == C4_ROWS * C4_COLS) {
    roundResult = 3;
  } else {
    turn = turn == 1 ? 2 : 1;
    nextAt = millis() + FALL_MS_PER_ROW * (row + 1) + 150;   // wait for the disc to land
    return true;
  }

  if (roundResult == 1) ptsX += 3;
  else if (roundResult == 2) ptsO += 3;
  else { ptsX += 1; ptsO += 1; }
  nextAt = millis() + FALL_MS_PER_ROW * (row + 1) + ROUND_PAUSE_MS;
  if (round + 1 >= C4_ROUNDS) seriesOver = true;
  return true;
}

// ---------------------------------------------------------------------------
// The computer: iterative-deepening negamax with alpha-beta pruning. It looks
// one move further each pass until the level's depth or time is used up, and
// plays the best move of the last pass that finished.
// ---------------------------------------------------------------------------
static const uint8_t COLUMN_ORDER[C4_COLS] = {3, 2, 4, 1, 5, 0, 6};   // centre first prunes best

// Heuristic: every run of four cells that holds only one side's discs counts
// more the more it holds, so threats and chances show up in the score
int Connect4Game::evaluate(uint8_t mark) {
  static const int8_t DIRS[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  uint8_t other = mark == 1 ? 2 : 1;
  int score = 0;

  for (int r = 0; r < C4_ROWS; r++) {
    for (int c = 0; c < C4_COLS; c++) {
      if (work[r][c] == mark && c == 3) score += 3;
      else if (work[r][c] == other && c == 3) score -= 3;

      for (int d = 0; d < 4; d++) {
        int er = r + 3 * DIRS[d][0], ec = c + 3 * DIRS[d][1];
        if (er < 0 || er >= C4_ROWS || ec < 0 || ec >= C4_COLS) continue;
        int mine = 0, theirs = 0;
        for (int k = 0; k < 4; k++) {
          uint8_t v = work[r + k * DIRS[d][0]][c + k * DIRS[d][1]];
          if (v == mark) mine++;
          else if (v == other) theirs++;
        }
        if (mine && theirs) continue;
        if (mine == 3) score += 50;
        else if (mine == 2) score += 6;
        else if (theirs == 3) score -= 60;
        else if (theirs == 2) score -= 6;
      }
    }
  }
  return score;
}

// Value of the position for `mark`, who is to move, `depth` moves from the horizon
int Connect4Game::search(int depth, int alpha, int beta, uint8_t mark, int ply) {
  if ((++nodes & 255) == 0 && millis() > searchDeadline) aborted = true;
  if (aborted) return 0;
  if (depth == 0) return evaluate(mark);

  uint8_t other = mark == 1 ? 2 : 1;
  int best = -100000;
  bool any = false;

  for (int k = 0; k < C4_COLS; k++) {
    uint8_t col = COLUMN_ORDER[k];
    if (filled[col] >= C4_ROWS) continue;
    any = true;

    int row = C4_ROWS - 1 - filled[col];
    work[row][col] = mark;
    filled[col]++;

    int value;
    if (findWin(work, row, col, nullptr)) value = 10000 - ply;   // a quicker win is better
    else value = -search(depth - 1, -beta, -alpha, other, ply + 1);

    filled[col]--;
    work[row][col] = 0;

    if (aborted) return 0;
    if (value > best) best = value;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return any ? best : 0;   // a full board is a draw
}

uint8_t Connect4Game::chooseAiMove() {
  const uint8_t ai = 2;
  uint8_t open[C4_COLS], n = 0;
  for (uint8_t c = 0; c < C4_COLS; c++) if (columnFree(c)) open[n++] = c;

  if (random(100) < BLUNDER_PCT[level]) return open[random(n)];

  memcpy(work, board, sizeof(work));
  for (int c = 0; c < C4_COLS; c++) {
    filled[c] = 0;
    for (int r = 0; r < C4_ROWS; r++) if (board[r][c]) filled[c]++;
  }

  uint8_t bestCol = open[random(n)];
  searchDeadline = millis() + SEARCH_MS[level];

  for (int depth = 1; depth <= SEARCH_DEPTH[level]; depth++) {
    nodes = 0;
    aborted = false;

    int bestScore = -100000, alpha = -100000;
    uint8_t bests[C4_COLS], nb = 0;
    for (int k = 0; k < C4_COLS; k++) {
      uint8_t col = COLUMN_ORDER[k];
      if (filled[col] >= C4_ROWS) continue;

      int row = C4_ROWS - 1 - filled[col];
      work[row][col] = ai;
      filled[col]++;
      int value = findWin(work, row, col, nullptr) ? 10000 : -search(depth - 1, -100000, -alpha, 1, 1);
      filled[col]--;
      work[row][col] = 0;

      if (aborted) break;
      if (value > bestScore) { bestScore = value; nb = 0; alpha = max(alpha, value - 1); }
      if (value == bestScore) bests[nb++] = col;
    }

    if (aborted) break;   // keep the last finished pass
    bestCol = bests[random(nb)];
    if (bestScore >= 9000 || bestScore <= -9000) break;   // decided: a forced win or loss
  }
  return bestCol;
}

// ---------------------------------------------------------------------------
// Input and turns
// ---------------------------------------------------------------------------
void Connect4Game::handleInput() {
  if (buttons.rightPressed) cursor = (cursor + 1) % C4_COLS;
  else if (buttons.leftPressed) cursor = (cursor + C4_COLS - 1) % C4_COLS;
}

void Connect4Game::updateLocal() {
  unsigned long now = millis();

  if (roundResult) {
    if (now >= nextAt && !seriesOver) {
      round++;
      startRound();
    }
    return;
  }
  if (now < nextAt) return;

  if (turn == 1) {
    handleInput();
    if ((buttons.upPressed || buttons.downPressed) && columnFree(cursor)) drop(cursor, 1);
  } else if (!thinkingShown) {
    thinkingShown = true;   // let one frame show "THINKING..." before the search blocks
  } else {
    drop(chooseAiMove(), 2);
    thinkingShown = false;
  }
}

// ---------------------------------------------------------------------------
// Two consoles. The host decides everything and sends the whole state each
// frame; the joiner keeps asking for its column until the board shows it.
//   host -> client: flags, board (11 bytes, 2 bits per cell), round, host points,
//                   client points
//   client -> host: flags, number of discs on the board, wanted column
// flags: host bit0 ring to move, bit1 round over, bit2 series over, bit3 wants
//        rematch, bits 4-5 round result; client bit0 wants rematch
// ---------------------------------------------------------------------------
bool Connect4Game::linkAlive() {
  LinkState s = gameLink.state();
  if (s == LINK_CONNECTED) return true;
  lobby.fail(s == LINK_PEER_LEFT ? "OPPONENT LEFT" : "CONNECTION LOST", "The match has ended", "");
  phase = PH_LOBBY;
  return false;
}

void Connect4Game::updateHost() {
  gameLink.poll();
  if (!linkAlive()) return;
  unsigned long now = millis();

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 3) {
    peerRematch = in[0] & 1;
    gotState = true;
    // The joiner drops rings. It may move only on its turn, and only for the
    // position it saw (the disc count guards against a repeated request)
    if (!roundResult && turn == 2 && now >= nextAt && in[2] < C4_COLS && in[1] == discCount()) drop(in[2], 2);
  }

  if (roundResult) {
    if (seriesOver) {
      if (buttons.rightPressed) wantRematch = true;
      if (buttons.upPressed) exitRequested = true;
      if (wantRematch && peerRematch) startSeries();
    } else if (now >= nextAt) {
      round++;
      startRound();
    }
  } else if (now >= nextAt && turn == 1) {
    handleInput();
    if ((buttons.upPressed || buttons.downPressed) && columnFree(cursor)) drop(cursor, 1);
  }

  uint8_t out[15];
  memset(out, 0, sizeof(out));
  out[0] = (turn == 2 ? 1 : 0) | (roundResult ? 2 : 0) | (seriesOver ? 4 : 0) | (wantRematch ? 8 : 0) | (roundResult << 4);
  for (int i = 0; i < C4_ROWS * C4_COLS; i++) out[1 + i / 4] |= board[i / C4_COLS][i % C4_COLS] << (2 * (i % 4));
  out[12] = round;
  out[13] = ptsX;
  out[14] = ptsO;
  gameLink.send(out, sizeof(out));
}

void Connect4Game::updateClient() {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 15) {
    gotState = true;
    for (int i = 0; i < C4_ROWS * C4_COLS; i++) board[i / C4_COLS][i % C4_COLS] = (in[1 + i / 4] >> (2 * (i % 4))) & 3;
    turn = (in[0] & 1) ? 2 : 1;
    roundResult = (in[0] >> 4) & 3;
    seriesOver = in[0] & 4;
    peerRematch = in[0] & 8;
    round = in[12];
    ptsX = in[13];
    ptsO = in[14];
    if (roundResult && roundResult != 3) {
      // find the winning four again for the highlight
      for (int r = 0; r < C4_ROWS; r++)
        for (int c = 0; c < C4_COLS; c++)
          if (board[r][c] == roundResult && findWin(board, r, c, winCells)) goto found;
    }
    found:
    if (!seriesOver) wantRematch = false;
  }

  uint8_t want = 0xFF;
  if (gotState && !roundResult && turn == 2) {
    handleInput();
    if ((buttons.upPressed || buttons.downPressed) && columnFree(cursor)) want = cursor;
  } else if (seriesOver) {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
  }

  // keep asking until the board shows the move
  if (want != 0xFF) { pendingCol = want; pendingCount = discCount(); }
  if (pendingCol != 0xFF && discCount() != pendingCount) pendingCol = 0xFF;

  uint8_t out[3] = {(uint8_t)(wantRematch ? 1 : 0), pendingCount, pendingCol};
  gameLink.send(out, sizeof(out));
}

void Connect4Game::update() {
  switch (phase) {
    case PH_LOBBY: {
      LobbyResult r = lobby.update();
      if (r == LOBBY_BACK) exitRequested = true;
      else if (r == LOBBY_HOST || r == LOBBY_CLIENT) {
        myMark = (r == LOBBY_HOST) ? 1 : 2;
        phase = (r == LOBBY_HOST) ? PH_HOST : PH_CLIENT;
        startSeries();
        gotState = false;
      }
      break;
    }
    case PH_LOCAL:  updateLocal(); break;
    case PH_HOST:   updateHost(); break;
    case PH_CLIENT: updateClient(); break;
  }
  noteNewDiscs();
}

// A disc that appeared since the last frame falls from the top
void Connect4Game::noteNewDiscs() {
  for (int r = 0; r < C4_ROWS; r++) {
    for (int c = 0; c < C4_COLS; c++) {
      if (board[r][c] && !shown[r][c]) {
        animRow = r;
        animCol = c;
        animStart = millis();
      }
      shown[r][c] = board[r][c];
    }
  }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void drawDisc(int cx, int cy, uint8_t mark) {
  if (mark == 1) {
    display.fillCircle(cx, cy, 3, SSD1306_WHITE);
  } else if (mark == 2) {
    display.drawCircle(cx, cy, 3, SSD1306_WHITE);
    display.drawPixel(cx, cy, SSD1306_WHITE);
  }
}

void Connect4Game::draw() {
  if (phase == PH_LOBBY) { lobby.draw(); return; }
  if (phase == PH_CLIENT && !gotState) {
    drawMessageScreen("CONNECTED", "", "Waiting for the host", "to start...", "");
    return;
  }
  drawBoard();
}

void Connect4Game::drawBoard() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  unsigned long now = millis();

  uint8_t mePts = myMark == 1 ? ptsX : ptsO, opPts = myMark == 1 ? ptsO : ptsX;
  uint8_t opMark = myMark == 1 ? 2 : 1;

  // Header: points and round
  display.setCursor(0, 0);
  display.print(F("YOU "));
  display.print(mePts);
  char text[12];
  snprintf(text, sizeof(text), "RND %d/%d", round + 1, C4_ROUNDS);
  drawCenteredText(text, 0, 1);
  snprintf(text, sizeof(text), "%s %d", phase == PH_LOCAL ? "AI" : "OPP", opPts);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(text), 0);
  display.print(text);

  // Side panels: whose discs, and what is going on
  drawDisc(17, 24, myMark);
  display.setCursor(5, 32);
  display.print(F("YOU"));
  drawDisc(SCREEN_WIDTH - 18, 24, opMark);
  display.setCursor(SCREEN_WIDTH - 31, 32);
  display.print(phase == PH_LOCAL ? "CPU" : "OPP");

  bool myTurn = !roundResult && turn == myMark && now >= nextAt;
  const char* left1 = "", *left2 = "", *right1 = "", *right2 = "";
  if (roundResult) {
    if (roundResult == 3)               { left1 = "DRAW"; right1 = "DRAW"; }
    else if (roundResult == myMark)     { left1 = "YOU"; left2 = "WIN!"; }
    else                                { right1 = "THEY"; right2 = "WIN"; }
  } else if (turn == myMark) {
    left1 = "YOUR"; left2 = "TURN";
  } else {
    right1 = phase == PH_LOCAL ? "THINK" : "THEIR";
    right2 = phase == PH_LOCAL ? "ING.." : "TURN";
  }
  display.setCursor(5, 46);  display.print(left1);
  display.setCursor(5, 55);  display.print(left2);
  display.setCursor(SCREEN_WIDTH - 31, 46);  display.print(right1);
  display.setCursor(SCREEN_WIDTH - 31, 55);  display.print(right2);

  // Board frame and slots
  display.drawRect(BOARD_X - 1, BOARD_Y - 1, C4_COLS * CELL + 2, C4_ROWS * CELL + 2, SSD1306_WHITE);
  for (int r = 0; r < C4_ROWS; r++) {
    for (int c = 0; c < C4_COLS; c++) {
      int cx = BOARD_X + c * CELL + CELL / 2, cy = BOARD_Y + r * CELL + CELL / 2;
      bool falling = animRow == r && animCol == c && now - animStart < FALL_MS_PER_ROW * (r + 1);
      if (board[r][c] && !falling) drawDisc(cx, cy, board[r][c]);
      else if (!board[r][c]) display.drawPixel(cx, cy, SSD1306_WHITE);   // an empty slot
    }
  }

  // The falling disc
  if (animRow >= 0 && board[animRow][animCol] && now - animStart < FALL_MS_PER_ROW * (animRow + 1)) {
    int cx = BOARD_X + animCol * CELL + CELL / 2;
    int cy = BOARD_Y + CELL / 2 + (int)((now - animStart) * CELL / FALL_MS_PER_ROW);
    drawDisc(cx, cy, board[animRow][animCol]);
  }

  // Cursor: a disc hovering above the chosen column on our turn
  if (myTurn) drawDisc(BOARD_X + cursor * CELL + CELL / 2, BOARD_Y - 5, myMark);

  // Highlight the winning four
  if (roundResult && roundResult != 3) {
    for (int k = 0; k < 4; k++) {
      int cx = BOARD_X + winCells[k][1] * CELL + CELL / 2, cy = BOARD_Y + winCells[k][0] * CELL + CELL / 2;
      if ((now / 250) % 2) display.drawCircle(cx, cy, 4, SSD1306_WHITE);
    }
  }

  if (phase != PH_LOCAL && seriesOver && roundResult && now >= nextAt - ROUND_PAUSE_MS + 600) {
    display.fillRect(10, 6, 108, 52, SSD1306_BLACK);
    display.drawRect(10, 6, 108, 52, SSD1306_WHITE);
    drawCenteredText(mePts > opPts ? "YOU WON!" : (mePts < opPts ? "YOU LOST" : "A DRAW"), 12, 2);
    drawCenteredText("UP: LEAVE", 34, 1);
    drawCenteredText(wantRematch ? "WAITING FOR THEM..." : (peerRematch ? "THEY WANT A REMATCH" : "RIGHT: REMATCH"), 44, 1);
  }
}
