#include "snake.h"
#include "display.h"
#include "input.h"
#include "fx.h"

SnakeGame snakeGame;

static const char* const WALL_VALUES[] = {"SOLID", "WRAP"};
static const GameOption OPT_WALLS = {"WALLS", 2, WALL_VALUES, 0, false};

const GameOption* SnakeGame::option(int i) { return i == 0 ? &OPT_DIFFICULTY : &OPT_WALLS; }

// Start speed (ms per step) and the fastest it gets, by level
static const int START_SPEED[DIFFICULTY_LEVELS] = {380, 300, 240, 190, 150};
static const int FAST_SPEED[DIFFICULTY_LEVELS]  = {200, 100,  90,  70,  55};

void SnakeGame::init() {
  snakeLength = 3;
  snake[0] = {GRID_WIDTH/2, GRID_HEIGHT/2};
  snake[1] = {GRID_WIDTH/2-1, GRID_HEIGHT/2};
  snake[2] = {GRID_WIDTH/2-2, GRID_HEIGHT/2};
  
  direction = 0; // right
  queued = 0;
  score = 0;
  gameSpeed = START_SPEED[level];
  minSpeed = FAST_SPEED[level];
  gameOver = false;
  lastMoveTime = 0;
  
  generateFood();
}

void SnakeGame::update() {
  handleInput();
  
  if (millis() - lastMoveTime > gameSpeed) {
    if (queued) {
      direction = queue[0];
      queue[0] = queue[1];
      queued--;
    }
    
    moveSnake();
    if (checkCollisions()) {
      gameOver = true;
    }
    lastMoveTime = millis();
  }
}

void SnakeGame::draw() {
  clearDisplay();
  
  // Draw snake
  for (int i = 0; i < snakeLength; i++) {
    int x = snake[i].x * GRID_SIZE;
    int y = snake[i].y * GRID_SIZE;
    
    if (i == 0) {
      display.fillRect(x, y, GRID_SIZE, GRID_SIZE, SSD1306_WHITE);
    } else {
      display.drawRect(x, y, GRID_SIZE, GRID_SIZE, SSD1306_WHITE);
    }
  }
  
  // Draw food
  int foodX = food.x * GRID_SIZE;
  int foodY = food.y * GRID_SIZE;
  display.fillCircle(foodX + GRID_SIZE/2, foodY + GRID_SIZE/2, GRID_SIZE/2-1, SSD1306_WHITE);
  
  // Draw score - avoid food area
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (food.y <= 1 && food.x <= 10) {
    display.setCursor(SCREEN_WIDTH - 60, 0);
  } else {
    display.setCursor(0, 0);
  }
  
  display.print(F("Score: "));
  display.print(score);
  
}

// Turns are queued (two deep), so a quick "up, left" within one step is not
// lost. A turn that reverses the snake or repeats the last one is ignored.
void SnakeGame::handleInput() {
  int want = -1;
  if (buttons.upPressed) want = 3;
  else if (buttons.downPressed) want = 1;
  else if (buttons.leftPressed) want = 2;
  else if (buttons.rightPressed) want = 0;
  if (want < 0 || queued >= 2) return;

  int last = queued ? queue[queued - 1] : direction;
  if (want == last || want == (last + 2) % 4) return;
  queue[queued++] = want;
}

void SnakeGame::generateFood() {
  // Pick uniformly among the free cells; cannot spin forever on a full board
  int freeCells = GRID_WIDTH * GRID_HEIGHT - snakeLength;
  if (freeCells <= 0) return;
  int pick = random(0, freeCells);

  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      bool occupied = false;
      for (int i = 0; i < snakeLength; i++) {
        if (snake[i].x == x && snake[i].y == y) { occupied = true; break; }
      }
      if (occupied) continue;
      if (pick-- == 0) {
        food.x = x;
        food.y = y;
        return;
      }
    }
  }
}

void SnakeGame::moveSnake() {
  Point newHead = snake[0];
  
  switch (direction) {
    case 0: newHead.x++; break; // right
    case 1: newHead.y++; break; // down
    case 2: newHead.x--; break; // left
    case 3: newHead.y--; break; // up
  }
  
  for (int i = snakeLength - 1; i > 0; i--) {
    snake[i] = snake[i-1];
  }
  
  snake[0] = newHead;
}

bool SnakeGame::checkCollisions() {
  Point head = snake[0];
  
  // Wall: deadly, or the way round when wrapping
  if (head.x < 0 || head.x >= GRID_WIDTH || 
      head.y < 0 || head.y >= GRID_HEIGHT) {
    if (!wrap) return true;
    snake[0].x = (head.x + GRID_WIDTH) % GRID_WIDTH;
    snake[0].y = (head.y + GRID_HEIGHT) % GRID_HEIGHT;
    head = snake[0];
  }
  
  // Self collision
  for (int i = 1; i < snakeLength; i++) {
    if (snake[i].x == head.x && snake[i].y == head.y) {
      return true;
    }
  }
  
  // Food collision
  if (head.x == food.x && head.y == food.y) {
    score++;
    // Stop growing at the buffer size instead of writing past it
    if (snakeLength < MAX_SNAKE_LENGTH) snakeLength++;
    
    fxPop(food.x * GRID_SIZE, food.y * GRID_SIZE, 1);
    if (gameSpeed > minSpeed) {
      gameSpeed = max(minSpeed, gameSpeed - 10);
    }
    
    generateFood();
  }
  
  return false;
}