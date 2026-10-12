#include "tictactoe.h"
#include "display.h"
#include "input.h"
#include "ui.h"
#include "fx.h"

TicTacToeGame ticTacToeGame;

static const uint8_t LINES[8][3] = {
  {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}
};

static const int BOARD_X = 44, BOARD_Y = 11, CELL = 13;   // 3 cells of 13 px: 39 x 39, leaving a line below for messages
static const unsigned long ROUND_PAUSE_MS = 1600;

void TicTacToeGame::init() {
  exitRequested = false;
  if (mode == 1) {
    phase = PH_LOBBY;
    lobby.begin(GAME_TICTACTOE, "TIC-TAC-TOE");
    seriesOver = false;
    ptsX = ptsO = 0;
  } else {
    phase = PH_LOCAL;
    myMark = 1;
    startSeries();
  }
}

void TicTacToeGame::restart() {
  exitRequested = false;
  phase = PH_LOCAL;
  myMark = 1;
  startSeries();
}

void TicTacToeGame::startSeries() {
  round = 0;
  ptsX = ptsO = 0;
  seriesOver = false;
  wantRematch = peerRematch = false;
  pendingCell = 0xFF;
  startRound();
}

// X always moves first in a round; who is X alternates (the human in even rounds
// against the computer, the host in even rounds on two consoles), so the
// starter alternates. Here X is simply whoever is to move first.
void TicTacToeGame::startRound() {
  memset(board, 0, sizeof(board));
  turn = 1;
  winLine = 0xFF;
  roundResult = 0;
  cursor = 4;
  nextAt = millis() + 500;
}

// Marks are fixed per player (host or human = X); the starter alternates by
// letting O move first in odd rounds.
bool TicTacToeGame::place(uint8_t cell, uint8_t mark) {
  if (cell >= 9 || board[cell] || roundResult) return false;
  board[cell] = mark;
  finishMove();
  return true;
}

uint8_t TicTacToeGame::winnerOf(const uint8_t* b, uint8_t* line) {
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t m = b[LINES[i][0]];
    if (m && m == b[LINES[i][1]] && m == b[LINES[i][2]]) {
      if (line) *line = i;
      return m;
    }
  }
  bool full = true;
  for (uint8_t i = 0; i < 9; i++) if (!b[i]) full = false;
  return full ? 3 : 0;
}

void TicTacToeGame::finishMove() {
  uint8_t r = winnerOf(board, &winLine);
  if (r == 0) {
    turn = turn == 1 ? 2 : 1;
    nextAt = millis() + 450;   // a short pause before the other side moves
    return;
  }
  roundResult = r;
  if (r == 1) ptsX += 3;
  else if (r == 2) ptsO += 3;
  else { ptsX += 1; ptsO += 1; }
  nextAt = millis() + ROUND_PAUSE_MS;
  if (round + 1 >= TTT_ROUNDS) seriesOver = true;
}

// ---------------------------------------------------------------------------
// The computer
// ---------------------------------------------------------------------------
// +10 - depth if `me` wins, -10 + depth if the other wins, 0 for a draw
int TicTacToeGame::minimax(uint8_t* b, uint8_t mark, uint8_t me, int depth) {
  uint8_t w = winnerOf(b, nullptr);
  if (w == me) return 10 - depth;
  if (w == 3) return 0;
  if (w) return depth - 10;

  int best = mark == me ? -100 : 100;
  for (uint8_t i = 0; i < 9; i++) {
    if (b[i]) continue;
    b[i] = mark;
    int v = minimax(b, mark == 1 ? 2 : 1, me, depth + 1);
    b[i] = 0;
    if (mark == me) { if (v > best) best = v; }
    else            { if (v < best) best = v; }
  }
  return best;
}

uint8_t TicTacToeGame::chooseAiMove() {
  const uint8_t ai = 2, human = 1;
  uint8_t empty[9], n = 0;
  for (uint8_t i = 0; i < 9; i++) if (!board[i]) empty[n++] = i;

  // A move that wins for `mark` right now, or 0xFF
  auto finisher = [&](uint8_t mark) -> uint8_t {
    for (uint8_t k = 0; k < n; k++) {
      board[empty[k]] = mark;
      bool win = winnerOf(board, nullptr) == mark;
      board[empty[k]] = 0;
      if (win) return empty[k];
    }
    return 0xFF;
  };

  uint8_t move;
  switch (level) {
    case 0:   // EASY: random, now and then spots a win
      if (random(100) < 30 && (move = finisher(ai)) != 0xFF) return move;
      return empty[random(n)];

    case 1:   // NORMAL: wins when it can, blocks most of the time
      if ((move = finisher(ai)) != 0xFF) return move;
      if (random(100) < 70 && (move = finisher(human)) != 0xFF) return move;
      return empty[random(n)];

    case 2:   // HARD: wins, blocks, then prefers centre and corners
      if ((move = finisher(ai)) != 0xFF) return move;
      if ((move = finisher(human)) != 0xFF) return move;
      if (!board[4]) return 4;
      {
        uint8_t corners[4], c = 0;
        for (uint8_t k : {0, 2, 6, 8}) if (!board[k]) corners[c++] = k;
        if (c) return corners[random(c)];
      }
      return empty[random(n)];

    default: {   // EXPERT / INSANE: minimax, picks randomly among the best moves
      if (level == 3 && random(100) < 12) return empty[random(n)];   // EXPERT blunders now and then
      int bestScore = -1000;
      uint8_t bests[9], nb = 0;
      for (uint8_t k = 0; k < n; k++) {
        board[empty[k]] = ai;
        int v = minimax(board, human, ai, 1);
        board[empty[k]] = 0;
        if (v > bestScore) { bestScore = v; nb = 0; }
        if (v == bestScore) bests[nb++] = empty[k];
      }
      return bests[random(nb)];
    }
  }
}

// ---------------------------------------------------------------------------
// Input: RIGHT / LEFT step through the empty cells, DOWN jumps a row, UP places
// ---------------------------------------------------------------------------
uint8_t TicTacToeGame::moveCursor(int dir) {
  for (int i = 1; i <= 9; i++) {
    uint8_t c = (cursor + dir * i + 18) % 9;
    if (!board[c]) return c;
  }
  return cursor;
}

void TicTacToeGame::moveCursorDown() {
  for (int i = 1; i <= 3; i++) {
    uint8_t c = (cursor + 3 * i) % 9;
    if (!board[c]) { cursor = c; return; }
  }
  cursor = moveCursor(1);
}

void TicTacToeGame::handleCursorInput() {
  if (board[cursor]) cursor = moveCursor(1);
  if (buttons.rightPressed) cursor = moveCursor(1);
  else if (buttons.leftPressed) cursor = moveCursor(-1);
  else if (buttons.downPressed) moveCursorDown();
}

void TicTacToeGame::updateLocal() {
  unsigned long now = millis();

  if (roundResult) {
    if (now >= nextAt && !seriesOver) {
      round++;
      startRound();
      // odd rounds: the computer starts
      if (round % 2 == 1) turn = 2;
    }
    return;
  }
  if (now < nextAt) return;

  // In odd rounds O (the computer) moves first, so `turn` says who is to move
  if (turn == 1) {
    handleCursorInput();
    if (buttons.upPressed && !board[cursor]) place(cursor, 1);
  } else {
    place(chooseAiMove(), 2);
  }
}

// ---------------------------------------------------------------------------
// Two consoles. The host decides everything and sends the whole state each
// frame; the client keeps asking for its move until the board shows it.
//   host -> client: flags, board (3 bytes, 2 bits per cell), round, host points,
//                   client points, winning line
//   client -> host: flags, number of marks on the board, wanted cell
// flags: host bit0 O to move, bit1 round over, bit2 series over, bit3 wants
//        rematch, bits 4-5 round result; client bit0 wants rematch
// ---------------------------------------------------------------------------
bool TicTacToeGame::linkAlive() {
  LinkState s = gameLink.state();
  if (s == LINK_CONNECTED) return true;
  lobby.fail(s == LINK_PEER_LEFT ? "OPPONENT LEFT" : "CONNECTION LOST", "The match has ended", "");
  phase = PH_LOBBY;
  return false;
}

static uint8_t marksOnBoard(const uint8_t* b) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 9; i++) if (b[i]) n++;
  return n;
}

void TicTacToeGame::updateHost() {
  gameLink.poll();
  if (!linkAlive()) return;
  unsigned long now = millis();

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 3) {
    peerRematch = in[0] & 1;
    gotState = true;
    // The joiner is O. It may move only on its turn, and only for the position
    // it saw (the mark count guards against a repeated request)
    if (!roundResult && turn == 2 && in[2] < 9 && in[1] == marksOnBoard(board)) place(in[2], 2);
  }

  if (roundResult) {
    if (seriesOver) {
      if (buttons.rightPressed) wantRematch = true;
      if (buttons.upPressed) exitRequested = true;
      if (wantRematch && peerRematch) {
        startSeries();
        turn = 1;
      }
    } else if (now >= nextAt) {
      round++;
      startRound();
      if (round % 2 == 1) turn = 2;   // the joiner starts odd rounds
    }
  } else if (now >= nextAt && turn == 1) {
    handleCursorInput();
    if (buttons.upPressed && !board[cursor]) place(cursor, 1);
  }

  uint8_t out[8];
  uint32_t packed = 0;
  for (uint8_t i = 0; i < 9; i++) packed |= (uint32_t)board[i] << (2 * i);
  out[0] = (turn == 2 ? 1 : 0) | (roundResult ? 2 : 0) | (seriesOver ? 4 : 0) | (wantRematch ? 8 : 0) | (roundResult << 4);
  out[1] = packed;
  out[2] = packed >> 8;
  out[3] = packed >> 16;
  out[4] = round;
  out[5] = ptsX;
  out[6] = ptsO;
  out[7] = winLine;
  gameLink.send(out, sizeof(out));
}

void TicTacToeGame::updateClient() {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 8) {
    gotState = true;
    uint32_t packed = in[1] | (uint32_t)in[2] << 8 | (uint32_t)in[3] << 16;
    for (uint8_t i = 0; i < 9; i++) board[i] = (packed >> (2 * i)) & 3;
    turn = (in[0] & 1) ? 2 : 1;
    roundResult = (in[0] >> 4) & 3;
    seriesOver = in[0] & 4;
    peerRematch = in[0] & 8;
    round = in[4];
    ptsX = in[5];
    ptsO = in[6];
    winLine = in[7];
    if (!seriesOver) wantRematch = false;
  }

  uint8_t want = 0xFF;
  if (gotState && !roundResult && turn == 2) {
    handleCursorInput();
    if (buttons.upPressed && !board[cursor]) want = cursor;
  } else if (seriesOver) {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
  }

  // keep asking until the board shows the move
  if (want != 0xFF) { pendingCell = want; pendingCount = marksOnBoard(board); }
  if (pendingCell != 0xFF && marksOnBoard(board) != pendingCount) pendingCell = 0xFF;

  uint8_t out[3] = {(uint8_t)(wantRematch ? 1 : 0), pendingCount, pendingCell};
  gameLink.send(out, sizeof(out));
}

// ---------------------------------------------------------------------------
void TicTacToeGame::update() {
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
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static void drawMark(int cx, int cy, uint8_t mark, int r) {
  if (mark == 1) {
    display.drawLine(cx - r, cy - r, cx + r, cy + r, SSD1306_WHITE);
    display.drawLine(cx - r, cy + r, cx + r, cy - r, SSD1306_WHITE);
    display.drawLine(cx - r + 1, cy - r, cx + r, cy + r - 1, SSD1306_WHITE);
    display.drawLine(cx - r, cy + r - 1, cx + r - 1, cy - r, SSD1306_WHITE);
  } else if (mark == 2) {
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    display.drawCircle(cx, cy, r - 1, SSD1306_WHITE);
  }
}

void TicTacToeGame::draw() {
  if (phase == PH_LOBBY) { lobby.draw(); return; }
  if ((phase == PH_CLIENT) && !gotState) {
    drawMessageScreen("CONNECTED", "", "Waiting for the host", "to start...", "");
    return;
  }
  drawBoard();
}

void TicTacToeGame::drawBoard() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header: round and scores. "me" is on the left.
  uint8_t mePts = myMark == 1 ? ptsX : ptsO, opPts = myMark == 1 ? ptsO : ptsX;
  display.setCursor(0, 0);
  display.print(F("YOU "));
  display.print(mePts);
  char text[12];
  snprintf(text, sizeof(text), "RND %d/%d", round + 1, TTT_ROUNDS);
  drawCenteredText(text, 0, 1);
  snprintf(text, sizeof(text), "%s %d", phase == PH_LOCAL ? "AI" : "OPP", opPts);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(text), 0);
  display.print(text);

  // Side labels: which mark is whose
  drawMark(16, 26, myMark, 7);
  display.setCursor(8, 38);
  display.print(F("YOU"));
  drawMark(SCREEN_WIDTH - 17, 26, myMark == 1 ? 2 : 1, 7);
  display.setCursor(SCREEN_WIDTH - 26, 38);
  display.print(phase == PH_LOCAL ? "CPU" : "OPP");

  // Grid
  for (int i = 1; i < 3; i++) {
    display.drawFastVLine(BOARD_X + i * CELL, BOARD_Y, 3 * CELL, SSD1306_WHITE);
    display.drawFastHLine(BOARD_X, BOARD_Y + i * CELL, 3 * CELL, SSD1306_WHITE);
  }
  for (int i = 0; i < 9; i++) {
    drawMark(BOARD_X + (i % 3) * CELL + CELL / 2, BOARD_Y + (i / 3) * CELL + CELL / 2, board[i], 3);
  }

  // Whose turn: the cursor on our own turn, a marker on the other's
  bool myTurn = !roundResult && turn == myMark && millis() >= nextAt;
  if (myTurn) {
    int x = BOARD_X + (cursor % 3) * CELL, y = BOARD_Y + (cursor / 3) * CELL;
    display.drawRect(x + 1, y + 1, CELL - 1, CELL - 1, SSD1306_WHITE);
  }

  if (roundResult && winLine < 8) {
    const uint8_t* l = LINES[winLine];
    int x0 = BOARD_X + (l[0] % 3) * CELL + CELL / 2, y0 = BOARD_Y + (l[0] / 3) * CELL + CELL / 2;
    int x1 = BOARD_X + (l[2] % 3) * CELL + CELL / 2, y1 = BOARD_Y + (l[2] / 3) * CELL + CELL / 2;
    display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
    display.drawLine(x0 + 1, y0, x1 + 1, y1, SSD1306_WHITE);
  }

  if (roundResult && !seriesOver) {
    const char* msg = roundResult == 3 ? "DRAW" : (roundResult == myMark ? "YOU WIN" : "YOU LOSE");
    drawCenteredText(msg, 55, 1);
  } else if (!roundResult && !myTurn && turn != myMark) {
    drawCenteredText(phase == PH_LOCAL ? "THINKING..." : "THEIR TURN", 55, 1);
  }

  if (phase != PH_LOCAL && seriesOver) {
    display.fillRect(10, 6, 108, 52, SSD1306_BLACK);
    display.drawRect(10, 6, 108, 52, SSD1306_WHITE);
    drawCenteredText(mePts > opPts ? "YOU WON!" : (mePts < opPts ? "YOU LOST" : "A DRAW"), 12, 2);
    drawCenteredText("UP: LEAVE", 34, 1);
    drawCenteredText(wantRematch ? "WAITING FOR THEM..." : (peerRematch ? "THEY WANT A REMATCH" : "RIGHT: REMATCH"), 44, 1);
  }
}
