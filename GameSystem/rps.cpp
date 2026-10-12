#include "rps.h"
#include "display.h"
#include "input.h"
#include "ui.h"
#include "fx.h"

RpsGame rpsGame;

static const char* const HAND_NAMES[3] = {"ROCK", "PAPER", "SCISSORS"};
static const unsigned long REVEAL_MS = 2600;   // shake, shake, shake, show, hold

// 0 draw, 1 `a` beats `b`, 2 `b` beats `a`  (rock 0, paper 1, scissors 2)
static uint8_t beats(uint8_t a, uint8_t b) {
  if (a == b) return 0;
  return ((a + 1) % 3 == b) ? 2 : 1;
}

static uint8_t counterTo(uint8_t move) { return (move + 1) % 3; }   // paper beats rock, ...

void RpsGame::init() {
  exitRequested = false;
  if (mode == 1) {
    phase = PH_LOBBY;
    lobby.begin(GAME_RPS, "RPS");
    myWins = opWins = 0;
    seriesOver = false;
    stage = OVER;
  } else {
    phase = PH_LOCAL;
    startSeries();
  }
}

void RpsGame::restart() {
  exitRequested = false;
  phase = PH_LOCAL;
  startSeries();
}

void RpsGame::startSeries() {
  round = 0;
  myWins = opWins = 0;
  seriesOver = false;
  wantRematch = peerRematch = false;
  history[0] = history[1] = 0xFF;
  memset(freq, 0, sizeof(freq));
  memset(order1, 0, sizeof(order1));
  memset(order2, 0, sizeof(order2));
  startChoose();
}

void RpsGame::startChoose() {
  stage = CHOOSE;
  selection = 0;
  myChoice = opChoice = 0xFF;
  peerLocked = false;
  hostChoice = clientChoice = 0xFF;
  result = 0;
}

// ---------------------------------------------------------------------------
// The computer: counts what the player does and plays what beats the likely
// next move. Higher levels look at more context and trust it more.
// ---------------------------------------------------------------------------
void RpsGame::learn(uint8_t move) {
  freq[move]++;
  if (history[0] != 0xFF) order1[history[0]][move]++;
  if (history[0] != 0xFF && history[1] != 0xFF) order2[history[1] * 3 + history[0]][move]++;
  history[1] = history[0];
  history[0] = move;
}

static uint8_t argmax3(const uint8_t* c, bool* known) {
  uint8_t best = 0;
  for (uint8_t i = 1; i < 3; i++) if (c[i] > c[best]) best = i;
  *known = c[best] > 0;
  return best;
}

uint8_t RpsGame::aiChoice() {
  uint8_t random3 = random(3);
  bool known = false;
  uint8_t predicted = 0;
  int trust = 0;   // percent

  switch (level) {
    case 0:   // EASY: pure chance
      return random3;
    case 1:   // NORMAL: sometimes beats your last move
      if (history[0] == 0xFF) return random3;
      predicted = history[0];
      trust = 40;
      break;
    case 2:   // HARD: beats your most common move
      predicted = argmax3(freq, &known);
      trust = known ? 65 : 0;
      break;
    case 3:   // EXPERT: what you tend to play after your last move
      if (history[0] == 0xFF) return random3;
      predicted = argmax3(order1[history[0]], &known);
      trust = known ? 80 : 0;
      break;
    default:  // INSANE: what you tend to play after your last two moves, falling back to one
      if (history[0] != 0xFF && history[1] != 0xFF) {
        predicted = argmax3(order2[history[1] * 3 + history[0]], &known);
      }
      if (!known && history[0] != 0xFF) predicted = argmax3(order1[history[0]], &known);
      trust = known ? 90 : 0;
      break;
  }
  return (random(100) < trust) ? counterTo(predicted) : random3;
}

// ---------------------------------------------------------------------------
// Rounds
// ---------------------------------------------------------------------------
void RpsGame::resolve(uint8_t mine, uint8_t theirs) {
  myChoice = mine;
  opChoice = theirs;
  result = beats(mine, theirs);
  if (result == 1) myWins++;
  else if (result == 2) opWins++;
  stage = REVEAL;
  revealAt = millis() + REVEAL_MS;
  if (myWins >= RPS_WINS || opWins >= RPS_WINS) seriesOver = true;
}

void RpsGame::handleChoiceInput() {
  if (buttons.rightPressed) selection = (selection + 1) % 3;
  else if (buttons.leftPressed) selection = (selection + 2) % 3;
}

void RpsGame::updateLocal() {
  unsigned long now = millis();
  if (stage == CHOOSE) {
    handleChoiceInput();
    if (buttons.upPressed) {
      uint8_t mine = selection;
      uint8_t theirs = aiChoice();   // picked before it learns from this round
      learn(mine);
      resolve(mine, theirs);
    }
  } else if (stage == REVEAL && now >= revealAt) {
    if (seriesOver) {
      stage = OVER;
      revealAt = millis() + 600;
    } else {
      round++;
      startChoose();
    }
  }
}

// ---------------------------------------------------------------------------
// Two consoles: both lock in a hand and the host decides. The host's hand is
// never sent before the reveal.
//   host -> client: flags, round, host wins, client wins, host hand, client hand
//   client -> host: flags, round, hand (0xFF = none yet)
// flags: host bit0 revealing, bit1 series over, bit2 wants rematch, bit3 the
//        client has locked in; client bit0 wants rematch, bit1 locked in
// ---------------------------------------------------------------------------
bool RpsGame::linkAlive() {
  LinkState s = gameLink.state();
  if (s == LINK_CONNECTED) return true;
  lobby.fail(s == LINK_PEER_LEFT ? "OPPONENT LEFT" : "CONNECTION LOST", "The match has ended", "");
  phase = PH_LOBBY;
  return false;
}

void RpsGame::updateHost() {
  gameLink.poll();
  if (!linkAlive()) return;
  unsigned long now = millis();

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 3) {
    peerRematch = in[0] & 1;
    gotState = true;
    if (stage == CHOOSE && in[1] == round && in[2] < 3) clientChoice = in[2];
  }
  peerLocked = clientChoice != 0xFF;

  if (stage == CHOOSE) {
    if (hostChoice == 0xFF) {
      handleChoiceInput();
      if (buttons.upPressed) hostChoice = selection;
    }
    if (hostChoice != 0xFF && clientChoice != 0xFF) resolve(hostChoice, clientChoice);
  } else if (stage == REVEAL && now >= revealAt) {
    if (seriesOver) {
      stage = OVER;
    } else {
      round++;
      startChoose();
    }
  } else if (stage == OVER) {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
    if (wantRematch && peerRematch) startSeries();
  }

  uint8_t out[6];
  bool reveal = stage != CHOOSE;
  out[0] = (reveal ? 1 : 0) | (stage == OVER ? 2 : 0) | (wantRematch ? 4 : 0) | (hostChoice != 0xFF ? 8 : 0);
  out[1] = round;
  out[2] = myWins;
  out[3] = opWins;
  out[4] = reveal ? myChoice : 0xFF;
  out[5] = reveal ? opChoice : 0xFF;
  gameLink.send(out, sizeof(out));
}

void RpsGame::updateClient() {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 6) {
    gotState = true;
    bool reveal = in[0] & 1;
    Stage newStage = (in[0] & 2) ? OVER : (reveal ? REVEAL : CHOOSE);
    peerRematch = in[0] & 4;
    peerLocked = in[0] & 8;   // the host has chosen

    if (in[1] != round || (newStage == CHOOSE && stage != CHOOSE)) {
      round = in[1];
      if (newStage == CHOOSE) { startChoose(); stage = CHOOSE; }
    }
    myWins = in[3];     // from our view: we are the client
    opWins = in[2];
    if (reveal) {
      if (stage == CHOOSE) revealAt = millis() + REVEAL_MS;   // start the reveal animation now
      opChoice = in[4];   // host hand
      myChoice = in[5];   // our hand
      result = beats(myChoice, opChoice);
    }
    stage = newStage;
    seriesOver = newStage == OVER;
    if (stage == CHOOSE) wantRematch = false;
  }

  if (stage == CHOOSE && myChoice == 0xFF) {
    handleChoiceInput();
    if (buttons.upPressed) myChoice = selection;
  } else if (stage == OVER) {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
  }

  uint8_t out[3] = {(uint8_t)(wantRematch ? 1 : 0), round, stage == CHOOSE ? myChoice : (uint8_t)0xFF};
  gameLink.send(out, sizeof(out));
}

void RpsGame::update() {
  switch (phase) {
    case PH_LOBBY: {
      LobbyResult r = lobby.update();
      if (r == LOBBY_BACK) exitRequested = true;
      else if (r == LOBBY_HOST || r == LOBBY_CLIENT) {
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
// Drawing: hands are made of a few shapes on a 16-unit grid, scaled by k
// ---------------------------------------------------------------------------
static void drawHand(uint8_t hand, int x, int y, int k) {
  switch (hand) {
    case 0:   // rock: a fist
      display.fillCircle(x + 8 * k, y + 8 * k, 6 * k, SSD1306_WHITE);
      for (int f = 1; f <= 2; f++) {
        display.drawFastVLine(x + (5 + f * 2) * k, y + 3 * k, 4 * k, SSD1306_BLACK);
      }
      break;
    case 1:   // paper: a flat hand with fingers
      display.drawRect(x + 2 * k, y + 4 * k, 12 * k, 11 * k, SSD1306_WHITE);
      for (int f = 0; f < 4; f++) {
        display.drawRect(x + (2 + f * 3) * k, y + 1 * k, 3 * k, 4 * k, SSD1306_WHITE);
      }
      break;
    default:  // scissors: two blades and two rings
      for (int t = 0; t < k; t++) {
        display.drawLine(x + 3 * k + t, y + 1 * k, x + 11 * k + t, y + 10 * k, SSD1306_WHITE);
        display.drawLine(x + 13 * k + t, y + 1 * k, x + 5 * k + t, y + 10 * k, SSD1306_WHITE);
      }
      display.drawCircle(x + 4 * k, y + 13 * k, 2 * k, SSD1306_WHITE);
      display.drawCircle(x + 12 * k, y + 13 * k, 2 * k, SSD1306_WHITE);
      break;
  }
}

void RpsGame::draw() {
  if (phase == PH_LOBBY) { lobby.draw(); return; }
  if (phase == PH_CLIENT && !gotState) {
    drawMessageScreen("CONNECTED", "", "Waiting for the host", "to start...", "");
    return;
  }
  drawScreen();
}

void RpsGame::drawScreen() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setCursor(0, 0);
  display.print(F("YOU "));
  display.print(myWins);
  char text[16];
  snprintf(text, sizeof(text), "FIRST TO %d", RPS_WINS);
  drawCenteredText(text, 0, 1);
  snprintf(text, sizeof(text), "%s %d", phase == PH_LOCAL ? "AI" : "OPP", opWins);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(text), 0);
  display.print(text);

  if (stage == CHOOSE) {
    // Three hands to pick from; the chosen one is framed
    for (int i = 0; i < 3; i++) {
      int x = 20 + i * 36;
      drawHand(i, x, 14, 2);
      if (i == selection && myChoice == 0xFF) display.drawRect(x - 3, 11, 38, 38, SSD1306_WHITE);
      else if (i == myChoice) display.drawRect(x - 3, 11, 38, 38, SSD1306_WHITE);
    }
    if (myChoice == 0xFF) {
      drawCenteredText(HAND_NAMES[selection], 52, 1);
    } else {
      drawCenteredText(phase == PH_LOCAL ? "..." : "WAITING FOR THEM...", 52, 1);
    }
    if (phase != PH_LOCAL && peerLocked && myChoice == 0xFF) {
      display.setCursor(0, 56);
      display.print(F("THEY ARE READY"));
    }
  } else {
    // Reveal: shake three times, then both hands
    unsigned long left = revealAt > millis() ? revealAt - millis() : 0;
    unsigned long elapsed = REVEAL_MS - min(left, REVEAL_MS);
    if (elapsed < 1500) {
      int bump = ((elapsed / 250) % 2) ? 6 : 0;
      drawHand(0, 12, 16 + bump, 2);
      drawHand(0, SCREEN_WIDTH - 44, 16 + bump, 2);
      drawCenteredText(elapsed < 500 ? "ROCK" : (elapsed < 1000 ? "PAPER" : "SCISSORS"), 52, 1);
    } else {
      drawHand(myChoice, 12, 14, 2);
      drawHand(opChoice, SCREEN_WIDTH - 44, 14, 2);
      drawCenteredText(result == 0 ? "DRAW" : (result == 1 ? "YOU WIN" : "YOU LOSE"), 52, 1);
    }
  }

  if (phase != PH_LOCAL && stage == OVER) {
    display.fillRect(10, 6, 108, 52, SSD1306_BLACK);
    display.drawRect(10, 6, 108, 52, SSD1306_WHITE);
    drawCenteredText(myWins > opWins ? "YOU WON!" : "YOU LOST", 12, 2);
    drawCenteredText("UP: LEAVE", 34, 1);
    drawCenteredText(wantRematch ? "WAITING FOR THEM..." : (peerRematch ? "THEY WANT A REMATCH" : "RIGHT: REMATCH"), 44, 1);
  }
}
