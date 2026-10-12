#include "flappy.h"
#include "display.h"
#include "input.h"
#include "fx.h"

FlappyGame flappyGame;

// Gap between the pipes, start speed and top speed (px per frame), by level
static const int GAP[DIFFICULTY_LEVELS]          = {28, 23, 20, 17, 15};
static const float START_SPEED[DIFFICULTY_LEVELS] = {1.4, 1.8, 2.0, 2.3, 2.6};
static const float TOP_SPEED[DIFFICULTY_LEVELS]   = {2.2, 2.8, 3.2, 3.5, 3.8};

void FlappyGame::init() {
  // Initialize bird
  bird.x = 20;
  bird.y = SCREEN_HEIGHT / 2;
  bird.velocity = 0;
  
  // Initialize pipes
  for (int i = 0; i < MAX_PIPES; i++) {
    pipes[i].active = false;
    pipes[i].passed = false;
  }
  
  score = 0;
  gameOver = false;
  gravity = 0.25;
  jumpStrength = -2.5;
  pipeGap = GAP[level];
  gameSpeed = START_SPEED[level];
  maxSpeed = TOP_SPEED[level];
  
  // Spawn first pipe
  spawnPipe();
}

void FlappyGame::update() {
  if (gameOver) return;
  
  handleInput();
  updateBird();
  updatePipes();
  
  if (checkCollisions()) {
    gameOver = true;
  }
  
  // Spawn a new pipe once the last one is far enough along
  float rightmost = -1000;
  for (int i = 0; i < MAX_PIPES; i++) if (pipes[i].active && pipes[i].x > rightmost) rightmost = pipes[i].x;
  if (rightmost < SCREEN_WIDTH - PIPE_SPACING) spawnPipe();
}

void FlappyGame::draw() {
  clearDisplay();
  
  drawGround();
  drawPipes();
  drawBird();
  
  // Draw score
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_WIDTH - 30, 5);
  display.print(score);
  
}

void FlappyGame::handleInput() {
  if (buttons.upPressed || buttons.rightPressed) {
    bird.velocity = jumpStrength;
  }
}

void FlappyGame::spawnPipe() {
  // Find an inactive pipe slot
  for (int i = 0; i < MAX_PIPES; i++) {
    if (!pipes[i].active) {
      pipes[i].active = true;
      pipes[i].x = SCREEN_WIDTH;
      pipes[i].gapY = random(15, SCREEN_HEIGHT - pipeGap - 10);
      pipes[i].passed = false;
      break;
    }
  }
}

void FlappyGame::updateBird() {
  // Apply gravity
  bird.velocity += gravity;
  bird.y += bird.velocity;
  
  // Prevent bird from going off screen top
  if (bird.y < 0) {
    bird.y = 0;
    bird.velocity = 0;
  }
}

void FlappyGame::updatePipes() {
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].active) {
      pipes[i].x -= gameSpeed;
      
      // Check if bird passed pipe
      if (!pipes[i].passed && pipes[i].x + PIPE_WIDTH < bird.x) {
        pipes[i].passed = true;
        score++;
        fxPop((int)bird.x + 6, (int)bird.y - 4, 1);
        
        // Increase game speed slightly
        if (gameSpeed < maxSpeed) {
          gameSpeed += 0.1;
        }
      }
      
      // Remove pipe when off screen
      if (pipes[i].x < -PIPE_WIDTH) {
        pipes[i].active = false;
      }
    }
  }
}

bool FlappyGame::checkCollisions() {
  // Ground collision
  if (bird.y >= SCREEN_HEIGHT - 5) {
    return true;
  }
  
  // Pipe collision
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].active) {
      // Check if bird is within pipe x range
      if (bird.x + BIRD_SIZE > pipes[i].x && bird.x < pipes[i].x + PIPE_WIDTH) {
        // Check if bird hits top or bottom pipe
        if (bird.y < pipes[i].gapY || bird.y + BIRD_SIZE > pipes[i].gapY + pipeGap) {
          return true;
        }
      }
    }
  }
  
  return false;
}

void FlappyGame::drawBird() {
  // Draw bird as a small filled circle
  display.fillCircle(bird.x + BIRD_SIZE/2, bird.y + BIRD_SIZE/2, BIRD_SIZE/2, SSD1306_WHITE);
  
  // Draw simple wing/eye
  display.drawPixel(bird.x + 1, bird.y + 1, SSD1306_BLACK);
}

void FlappyGame::drawPipes() {
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].active) {
      // Top pipe
      display.fillRect(pipes[i].x, 0, PIPE_WIDTH, pipes[i].gapY, SSD1306_WHITE);
      
      // Bottom pipe
      display.fillRect(pipes[i].x, pipes[i].gapY + pipeGap, PIPE_WIDTH, 
                       SCREEN_HEIGHT - pipes[i].gapY - pipeGap - 5, SSD1306_WHITE);
      
      // Pipe caps (make them look more like pipes)
      display.fillRect(pipes[i].x - 1, pipes[i].gapY - 3, PIPE_WIDTH + 2, 3, SSD1306_WHITE);
      display.fillRect(pipes[i].x - 1, pipes[i].gapY + pipeGap, PIPE_WIDTH + 2, 3, SSD1306_WHITE);
    }
  }
}

void FlappyGame::drawGround() {
  // Draw simple ground line
  display.drawLine(0, SCREEN_HEIGHT - 5, SCREEN_WIDTH, SCREEN_HEIGHT - 5, SSD1306_WHITE);
  
  // Draw some ground texture
  for (int x = 0; x < SCREEN_WIDTH; x += 4) {
    display.drawPixel(x, SCREEN_HEIGHT - 4, SSD1306_WHITE);
    display.drawPixel(x + 2, SCREEN_HEIGHT - 3, SSD1306_WHITE);
  }
}