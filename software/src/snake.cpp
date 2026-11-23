#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Snake: eat the blinking food, don't hit the edge or yourself. Faster with every bite.
// On 64x64 every cell is 2x2 dots, the same field as on 32x32 but easier to see.

#define SNAKE_CELL   (ROWS >= 64 ? 2 : 1)
#define SNAKE_W      (COLS / SNAKE_CELL)
#define SNAKE_H      (ROWS / SNAKE_CELL)
#define SNAKE_START  4
#define SNAKE_GROW   2     // Cells per bite
#define SNAKE_FAST   70    // Fastest frame time in ms
#define SNAKE_FASTER 4     // ms faster per bite

static uint16_t body[SNAKE_W * SNAKE_H];  // Cells as x * SNAKE_H + y, a ring with the head at headindex
static uint16_t headindex, length;
static uint8_t grow;
static bool taken[SNAKE_W][SNAKE_H];
static int8_t dx, dy;
static int16_t foodx, foody;
static uint16_t frame, eaten;

static void cell(int16_t x, int16_t y, bool on) {
  for (uint8_t i = 0; i < SNAKE_CELL; i++) {
    for (uint8_t j = 0; j < SNAKE_CELL; j++) {
      setdot(x * SNAKE_CELL + i, y * SNAKE_CELL + j, on);
    }
  }
}

static void placefood() {
  uint16_t free = SNAKE_W * SNAKE_H - length;
  if (free == 0) {
    foodx = -1;
    return;
  }
  uint16_t pick = random(free);
  for (int16_t x = 0; x < SNAKE_W; x++) {
    for (int16_t y = 0; y < SNAKE_H; y++) {
      if (!taken[x][y] && pick-- == 0) {
        foodx = x;
        foody = y;
        return;
      }
    }
  }
}

static void snakesetup() {
  memset(taken, 0, sizeof(taken));
  length = SNAKE_START;
  headindex = SNAKE_START - 1;
  int16_t y = SNAKE_H / 2;
  for (uint16_t i = 0; i < SNAKE_START; i++) {
    int16_t x = SNAKE_W / 4 + i;
    body[i] = x * SNAKE_H + y;
    taken[x][y] = true;
    cell(x, y, true);
  }
  dx = 1;
  dy = 0;
  grow = 0;
  frame = 0;
  eaten = 0;
  placefood();
  cell(foodx, foody, true);
}

static void snakestep() {
  // One turn per step, the rest of the presses wait for the next steps
  for (uint8_t button; (button = gamepress(0));) {
    int8_t ndx = button == BUTTON_LEFT ? -1 : button == BUTTON_RIGHT ? 1 : 0;
    int8_t ndy = button == BUTTON_UP ? -1 : button == BUTTON_DOWN ? 1 : 0;
    if ((ndx || ndy) && ndx != -dx && ndy != -dy && (ndx != dx || ndy != dy)) {
      dx = ndx;
      dy = ndy;
      break;
    }
  }

  uint16_t head = body[headindex];
  int16_t x = head / SNAKE_H + dx, y = head % SNAKE_H + dy;

  // The tail moves away in this step, unless the snake grows
  uint16_t tailindex = (headindex + SNAKE_W * SNAKE_H - length + 1) % (SNAKE_W * SNAKE_H);
  uint16_t tail = body[tailindex];
  if (grow == 0) {
    taken[tail / SNAKE_H][tail % SNAKE_H] = false;
  }

  if (x < 0 || x >= SNAKE_W || y < 0 || y >= SNAKE_H || taken[x][y]) {
    if (grow == 0) {
      taken[tail / SNAKE_H][tail % SNAKE_H] = true;
    }
    gameover(eaten);
    return;
  }

  if (grow > 0) {
    grow--;
    length++;
  } else {
    cell(tail / SNAKE_H, tail % SNAKE_H, false);
  }
  headindex = (headindex + 1) % (SNAKE_W * SNAKE_H);
  body[headindex] = x * SNAKE_H + y;
  taken[x][y] = true;
  cell(x, y, true);

  if (x == foodx && y == foody) {
    eaten++;
    grow += SNAKE_GROW;
    gameframetime = max(SNAKE_FAST, (int)gameframetime - SNAKE_FASTER);
    placefood();
  }

  // The food blinks, so it doesn't look like a piece of snake
  frame++;
  if (foodx >= 0) {
    cell(foodx, foody, (frame / 2) % 2 == 0);
  }
}

const Game snakeGame = { "snake", "Snake", 1, snakesetup, snakestep, 160, nullptr, true };
