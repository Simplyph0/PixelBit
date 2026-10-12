#include "pong.h"
#include "display.h"
#include "input.h"
#include "ui.h"
#include "fx.h"

PongGame pongGame;

// How good the AI is, easiest to hardest. A stronger AI is faster, starts
// tracking the ball earlier, aims with a smaller error and, from EXPERT up,
// looks ahead to where the ball will land (including wall bounces) instead of
// just following its current height; INSANE also aims its returns away from the
// player. NORMAL is the original behaviour. Foresight is very powerful, so even
// a few percent is a big step: against a simulated player that reacts two frames
// late, these levels win about 86 / 64 / 44 / 19 / 0 % of matches for the player.
struct AiLevel {
  float speed;       // paddle speed while tracking, px per 40 ms frame
  float reactX;      // starts tracking once the ball passed this fraction of the width
  int   error;       // aim error after each player hit, +/- px
  float foresight;   // 0 = follows the ball, 1 = goes straight to where it will land (wall bounces included)
  int   tactic;      // when > 0 the AI hits the ball off-centre, sending it away from the player
};
static const AiLevel AI_LEVELS[DIFFICULTY_LEVELS] = {
  {1.3, 0.50, 9, 0.0,  0},   // EASY
  {1.9, 0.25, 5, 0.0,  0},   // NORMAL
  {2.0, 0.25, 5, 0.0,  0},   // HARD
  {2.1, 0.10, 4, 0.02, 0},   // EXPERT
  {2.3, 0.00, 3, 0.05, 1},   // INSANE
};

static const char* const POWERUP_VALUES[] = {"OFF", "ON"};
static const GameOption OPT_POWERUPS = {"POWER-UPS", 2, POWERUP_VALUES, 0, false};

const GameOption* PongGame::option(int i) {
  switch (i) {
    case 0: return &OPT_MODE_PRACTICE;
    case 1: return &OPT_DIFFICULTY;
    default: return &OPT_POWERUPS;
  }
}

// Difficulty only matters against the computer; power-ups need an opponent
bool PongGame::optionShown(int i) {
  if (i == 1) return mode == PONG_AI;
  if (i == 2) return mode != PONG_PRACTICE;
  return true;
}

void PongGame::setOption(int i, int value) {
  if (i == 0) mode = value;
  else if (i == 1) level = constrain(value, 0, DIFFICULTY_LEVELS - 1);
  else powerups = value;
}

void PongGame::init() {
  exitRequested = false;
  playerScore = aiScore = rally = 0;
  gameOver = gameWon = false;
  if (mode == PONG_VERSUS) {
    phase = PH_LOBBY;
    lobby.begin(GAME_PONG, "PONG");
  } else {
    startLocalMatch();
  }
}

void PongGame::resetMatch() {
  playerH = aiH = PONG_PADDLE_H;
  playerY = aiY = (SCREEN_HEIGHT - PONG_PADDLE_H) / 2;
  playerVy = aiVy = 0;
  playerScore = 0;
  aiScore = 0;
  rally = 0;
  aiError = 0;
  gameOver = false;
  gameWon = false;
  matchOver = false;
  wantRematch = peerRematch = false;
  puType = 0;
  puNextAt = millis() + 6000;
  playerBigUntil = aiBigUntil = playerSmallUntil = aiSmallUntil = 0;
  lastUpdate = millis();
  serve();
  serveUntil = millis() + 2000;   // a little longer before the first ball
}

void PongGame::startLocalMatch() {
  phase = PH_LOCAL;
  resetMatch();
  serveUntil = millis() + 800;
}

// "Play again" on the results screen after a game against the computer
void PongGame::restart() {
  exitRequested = false;
  startLocalMatch();
}

void PongGame::stop() {
  lobby.end();
}

int PongGame::getScore() {
  if (mode == PONG_PRACTICE) return rally;
  return playerScore * 10 + (gameWon ? (PONG_WIN_SCORE - aiScore) * 10 : 0);
}

void PongGame::serve() {
  ballX = SCREEN_WIDTH / 2;
  ballY = SCREEN_HEIGHT / 2;
  velX = (random(2) ? 2.2 : -2.2);
  if (mode == PONG_PRACTICE) velX = -2.2;   // towards you first
  velY = random(-12, 13) / 10.0;
  serveUntil = millis() + 800;
}

void PongGame::update() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 40.0;   // 1.0 = one 40 ms frame
  lastUpdate = now;
  if (dt > 2.0) dt = 2.0;

  switch (phase) {
    case PH_LOBBY: {
      LobbyResult r = lobby.update();
      if (r == LOBBY_BACK) {
        exitRequested = true;
      } else if (r == LOBBY_HOST || r == LOBBY_CLIENT) {
        phase = (r == LOBBY_HOST) ? PH_HOST : PH_CLIENT;
        resetMatch();
        gotState = false;
      }
      break;
    }
    case PH_LOCAL:  updateLocal(now, dt); break;
    case PH_HOST:   updateHost(now, dt); break;
    case PH_CLIENT: updateClient(now, dt); break;
  }
}

void PongGame::movePlayer(float dt) {
  float before = playerY;
  if (buttons.up)   playerY -= 3.0 * dt;
  if (buttons.down) playerY += 3.0 * dt;
  playerY = constrain(playerY, 0, SCREEN_HEIGHT - playerH);
  playerVy = dt > 0 ? (playerY - before) / dt : 0;
}

void PongGame::updateLocal(unsigned long now, float dt) {
  if (gameOver) return;

  movePlayer(dt);
  if (mode == PONG_AI) {
    float before = aiY;
    updateAI(dt);
    aiVy = dt > 0 ? (aiY - before) / dt : 0;
  }

  if (now >= serveUntil) moveBall(dt);
  if (mode != PONG_PRACTICE) updatePowerups(now);

  if (mode == PONG_PRACTICE) {
    if (ballX + PONG_BALL_SIZE < 0) { gameOver = true; gameWon = false; }
  } else if (playerScore >= PONG_WIN_SCORE || aiScore >= PONG_WIN_SCORE) {
    gameOver = true;
    gameWon = playerScore > aiScore;
  }
}

// Deliberately imperfect: only reacts once the ball is past the first
// quarter, is slower than steep shots, and aims with a random error.
void PongGame::updateAI(float dt) {
  const AiLevel& ai = AI_LEVELS[level];
  float target;
  float speed;
  if (velX > 0 && ballX > SCREEN_WIDTH * ai.reactX) {
    float y = ballY + (ai.foresight > 0 ? (predictBallY() - ballY) * ai.foresight : 0);
    target = y + PONG_BALL_SIZE / 2.0 - aiH / 2.0 + aiError;
    speed = ai.speed * dt;
  } else {
    target = (SCREEN_HEIGHT - aiH) / 2.0;
    speed = 1.0 * dt;
  }
  float diff = target - aiY;
  if (diff > speed) diff = speed;
  if (diff < -speed) diff = -speed;
  aiY = constrain(aiY + diff, 0, SCREEN_HEIGHT - aiH);
}

// Where on the paddle the AI wants to meet the next ball, as an offset in px.
// Weak levels just scatter their aim. Strong ones also aim for the edge of the
// paddle on the side that sends the ball away from the player's paddle.
float PongGame::pickAiAim() {
  const AiLevel& ai = AI_LEVELS[level];
  float aim = ai.error ? random(-ai.error, ai.error + 1) : 0;
  if (ai.tactic) {
    bool playerLow = playerY + playerH / 2.0 > SCREEN_HEIGHT / 2.0;
    aim += playerLow ? ai.tactic : -ai.tactic;   // paddle centre below the ball = ball leaves upwards
  }
  return aim;
}

// Height the ball will have when it reaches the AI paddle, folding the path
// back at the top and bottom walls.
float PongGame::predictBallY() {
  if (velX <= 0) return ballY;
  float travel = (PONG_AI_X - PONG_BALL_SIZE - ballX) / velX;
  if (travel < 0) travel = 0;
  const float range = SCREEN_HEIGHT - PONG_BALL_SIZE;
  float y = fmodf(ballY + velY * travel, 2 * range);
  if (y < 0) y += 2 * range;
  return y > range ? 2 * range - y : y;
}

// A paddle returns the ball. The hit position sets the angle, and a moving
// paddle adds spin (extra vertical speed in its direction of travel).
void PongGame::paddleHit(bool left) {
  float padY = left ? playerY : aiY;
  float padH = left ? playerH : aiH;
  float vy = left ? playerVy : aiVy;

  float speed = min(fabs(velX) * 1.07, 4.5);
  velX = left ? speed : -speed;
  velY = ((ballY + PONG_BALL_SIZE / 2.0) - (padY + padH / 2.0)) / (padH / 2.0) * 2.5;
  velY = constrain(velY + vy * 0.35, -3.4, 3.4);
}

void PongGame::moveBall(float dt) {
  // Sub-steps so a fast ball cannot tunnel through a thin paddle
  float dx = velX * dt, dy = velY * dt;
  float longest = max(fabs(dx), fabs(dy));
  int steps = max(1, (int)ceil(longest / 1.5));
  dx /= steps;
  dy /= steps;

  for (int i = 0; i < steps; i++) {
    ballX += dx;
    ballY += dy;

    // Top / bottom walls
    if (ballY <= 0) { ballY = 0; velY = fabs(velY); dy = fabs(dy); }
    if (ballY >= SCREEN_HEIGHT - PONG_BALL_SIZE) {
      ballY = SCREEN_HEIGHT - PONG_BALL_SIZE;
      velY = -fabs(velY);
      dy = -fabs(dy);
    }

    // Our paddle
    if (velX < 0 &&
        ballX <= PONG_PLAYER_X + PONG_PADDLE_W && ballX + PONG_BALL_SIZE >= PONG_PLAYER_X &&
        ballY + PONG_BALL_SIZE >= playerY && ballY <= playerY + playerH) {
      paddleHit(true);
      ballX = PONG_PLAYER_X + PONG_PADDLE_W;
      aiError = pickAiAim();
      if (mode == PONG_PRACTICE) {
        rally++;
        fxPop(PONG_PLAYER_X + 14, (int)ballY, 1);
      }
      dx = velX * dt / steps;
      dy = velY * dt / steps;
    }

    if (mode == PONG_PRACTICE) {
      // The wall returns the ball
      if (velX > 0 && ballX + PONG_BALL_SIZE >= SCREEN_WIDTH - 1) {
        velX = -velX;
        ballX = SCREEN_WIDTH - 1 - PONG_BALL_SIZE;
        dx = velX * dt / steps;
      }
    } else if (velX > 0 &&
        ballX + PONG_BALL_SIZE >= PONG_AI_X && ballX <= PONG_AI_X + PONG_PADDLE_W &&
        ballY + PONG_BALL_SIZE >= aiY && ballY <= aiY + aiH) {
      // The opponent's paddle
      paddleHit(false);
      ballX = PONG_AI_X - PONG_BALL_SIZE;
      dx = velX * dt / steps;
      dy = velY * dt / steps;
    }

    // Point scored
    if (ballX + PONG_BALL_SIZE < 0) {
      if (mode != PONG_PRACTICE) { aiScore++; serve(); }
      return;
    }
    if (ballX > SCREEN_WIDTH) { playerScore++; serve(); return; }
  }
}

// ---------------------------------------------------------------------------
// Power-ups: now and then a pick-up appears mid-field. Whoever hit the ball last
// collects it when the ball touches it: BIG makes their paddle taller for a
// while, SMALL makes the other paddle shorter.
// ---------------------------------------------------------------------------
static const unsigned long POWERUP_MS = 8000;
static const int PU_SIZE = 9;

void PongGame::applyPaddleSizes(unsigned long now) {
  float newPlayer = PONG_PADDLE_H, newAi = PONG_PADDLE_H;
  if (now < playerBigUntil) newPlayer += 8;
  if (now < playerSmallUntil) newPlayer -= 6;
  if (now < aiBigUntil) newAi += 8;
  if (now < aiSmallUntil) newAi -= 6;
  playerH = newPlayer;
  aiH = newAi;
  playerY = constrain(playerY, 0, SCREEN_HEIGHT - playerH);
  aiY = constrain(aiY, 0, SCREEN_HEIGHT - aiH);
}

void PongGame::updatePowerups(unsigned long now) {
  if (!powerups) return;
  applyPaddleSizes(now);

  if (puType == 0) {
    if (now >= puNextAt && now >= serveUntil) {
      puType = 1 + random(2);
      puX = random(46, SCREEN_WIDTH - 46 - PU_SIZE);
      puY = random(10, SCREEN_HEIGHT - 10 - PU_SIZE);
    }
    return;
  }

  bool hit = ballX + PONG_BALL_SIZE >= puX && ballX <= puX + PU_SIZE &&
             ballY + PONG_BALL_SIZE >= puY && ballY <= puY + PU_SIZE;
  if (!hit) return;

  bool collectorIsLeft = velX > 0;   // the ball is travelling away from whoever hit it last
  if (puType == 1) {
    (collectorIsLeft ? playerBigUntil : aiBigUntil) = now + POWERUP_MS;
  } else {
    (collectorIsLeft ? aiSmallUntil : playerSmallUntil) = now + POWERUP_MS;
  }
  fxPopText((int)puX + 4, (int)puY, puType == 1 ? "BIG" : "SMALL");
  puType = 0;
  puNextAt = now + 7000;
}

// ---------------------------------------------------------------------------
// Two-console match. The host runs the ball, the power-ups and the scoring and
// sends the full state every frame (so a lost packet does not matter); the
// client moves its own paddle locally and sends its position. Each player sees
// their own paddle on the left, so the client mirrors what it receives.
//   host -> client: flags, host paddle y, ball x, ball y (1/8 px), host score,
//                   client score, host paddle h, client paddle h, pick-up type, x, y
//   client -> host: flags, client paddle y
// flags: host bit0 serving, bit1 match over, bit2 wants rematch, bit3 power-ups on;
//        client bit0 wants rematch
// ---------------------------------------------------------------------------
bool PongGame::linkAlive() {
  LinkState s = gameLink.state();
  if (s == LINK_CONNECTED) return true;
  lobby.fail(s == LINK_PEER_LEFT ? "OPPONENT LEFT" : "CONNECTION LOST", "The match has ended", "");
  phase = PH_LOBBY;
  return false;
}

void PongGame::updateHost(unsigned long now, float dt) {
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 2) {
    peerRematch = in[0] & 1;
    float y = constrain((int)in[1], 0, SCREEN_HEIGHT - (int)aiH);
    aiVy = (y - aiY) / 1.0;
    aiY = y;
    gotState = true;
  }

  movePlayer(dt);

  if (!matchOver) {
    serving = now < serveUntil;
    if (!serving) moveBall(dt);
    updatePowerups(now);
    if (playerScore >= PONG_WIN_SCORE || aiScore >= PONG_WIN_SCORE) matchOver = true;
  } else {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
    if (wantRematch && peerRematch) resetMatch();
  }

  uint8_t out[14];
  int bx = (int)(ballX * 8), by = (int)(ballY * 8);
  out[0] = (serving ? 1 : 0) | (matchOver ? 2 : 0) | (wantRematch ? 4 : 0) | (powerups ? 8 : 0);
  out[1] = (uint8_t)playerY;
  out[2] = bx;
  out[3] = bx >> 8;
  out[4] = by;
  out[5] = by >> 8;
  out[6] = playerScore;
  out[7] = aiScore;
  out[8] = (uint8_t)playerH;
  out[9] = (uint8_t)aiH;
  out[10] = puType;
  out[11] = (uint8_t)puX;
  out[12] = (uint8_t)puY;
  out[13] = 0;
  gameLink.send(out, sizeof(out));
}

void PongGame::updateClient(unsigned long now, float dt) {
  (void)now;
  gameLink.poll();
  if (!linkAlive()) return;

  uint8_t in[LINK_MAX_PAYLOAD];
  uint8_t n;
  if (gameLink.receive(in, n) && n >= 13) {
    serving = in[0] & 1;
    matchOver = in[0] & 2;
    peerRematch = in[0] & 4;
    powerups = (in[0] & 8) ? 1 : 0;
    aiY = in[1];
    int bx = (int16_t)(in[2] | in[3] << 8);
    int by = (int16_t)(in[4] | in[5] << 8);
    ballX = SCREEN_WIDTH - bx / 8.0 - PONG_BALL_SIZE;   // mirrored
    ballY = by / 8.0;
    aiScore = in[6];       // the host's score is the opponent's
    playerScore = in[7];
    aiH = in[8];           // host paddle
    playerH = in[9];       // our paddle
    puType = in[10];
    puX = SCREEN_WIDTH - in[11] - PU_SIZE;   // mirrored
    puY = in[12];
    if (!matchOver) wantRematch = false;   // the host started a new match
    gotState = true;
  }

  movePlayer(dt);

  if (matchOver) {
    if (buttons.rightPressed) wantRematch = true;
    if (buttons.upPressed) exitRequested = true;
  }

  uint8_t out[2] = {(uint8_t)(wantRematch ? 1 : 0), (uint8_t)playerY};
  gameLink.send(out, sizeof(out));
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void PongGame::draw() {
  if (phase == PH_LOBBY) lobby.draw();
  else drawMatch();
}

void PongGame::drawMatch() {
  if (phase == PH_CLIENT && !gotState) {
    drawMessageScreen("CONNECTED", "", "Waiting for the host", "to start...", "");
    return;
  }

  clearDisplay();

  // Net
  for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
    display.drawFastVLine(SCREEN_WIDTH / 2, y, 2, SSD1306_WHITE);
  }

  // Scores (practice: returns)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  if (mode == PONG_PRACTICE) {
    display.setCursor(SCREEN_WIDTH / 2 - 14, 0);
    display.print(rally);
  } else {
    display.setCursor(SCREEN_WIDTH / 2 - 14, 0);
    display.print(playerScore);
    display.setCursor(SCREEN_WIDTH / 2 + 8, 0);
    display.print(aiScore);
  }

  display.fillRect(PONG_PLAYER_X, (int)playerY, PONG_PADDLE_W, (int)playerH, SSD1306_WHITE);
  if (mode == PONG_PRACTICE) display.drawFastVLine(SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT, SSD1306_WHITE);
  else display.fillRect(PONG_AI_X, (int)aiY, PONG_PADDLE_W, (int)aiH, SSD1306_WHITE);
  display.fillRect((int)ballX, (int)ballY, PONG_BALL_SIZE, PONG_BALL_SIZE, SSD1306_WHITE);

  // Pick-up: a box with B (big) or S (small)
  if (puType) {
    display.drawRect((int)puX, (int)puY, PU_SIZE, PU_SIZE, SSD1306_WHITE);
    display.setCursor((int)puX + 2, (int)puY + 1);
    display.print(puType == 1 ? 'B' : 'S');
  }

  if (phase != PH_LOCAL) {
    if (serving && !matchOver && playerScore + aiScore == 0) {
      drawCenteredText("GET READY", 12, 1);
    }

    if (matchOver) {
      display.fillRect(10, 6, 108, 52, SSD1306_BLACK);
      display.drawRect(10, 6, 108, 52, SSD1306_WHITE);
      drawCenteredText(playerScore > aiScore ? "YOU WON!" : "YOU LOST", 12, 2);
      drawCenteredText("UP: LEAVE", 34, 1);
      drawCenteredText(wantRematch ? "WAITING FOR THEM..." : (peerRematch ? "THEY WANT A REMATCH" : "RIGHT: REMATCH"), 44, 1);
    }
  }
}
