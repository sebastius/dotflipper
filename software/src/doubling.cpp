#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "font.h"
#include "game.h"

// Doubling: slide all tiles on a 4x4 board to one side, two equal tiles that meet become one with double
// the value. After every move a 2 (sometimes a 4) appears. The tiles are filled squares with the number
// cut out; on 32x32 there is room for the power of two only (1 for 2, 11 for 2048).
// Score: the sum of all tiles made by merging, the usual score for this kind of game.

#define DB_TILE   (ROWS >= 64 ? 15 : 7)
#define DB_LEFT   ((COLS - 4 * DB_TILE - 3) / 2)
#define DB_TOP    ((ROWS - 4 * DB_TILE - 3) / 2)

static uint8_t board[4][4];  // Power of two per tile, 0 is empty; [x][y]
static uint32_t score;

static void addtile() {
  uint8_t free = 0;
  for (uint8_t x = 0; x < 4; x++)
    for (uint8_t y = 0; y < 4; y++) free += board[x][y] == 0;
  if (free == 0) return;
  uint8_t pick = random(free);
  for (uint8_t x = 0; x < 4; x++)
    for (uint8_t y = 0; y < 4; y++)
      if (board[x][y] == 0 && pick-- == 0) board[x][y] = random(10) ? 1 : 2;
}

// Digits cut out of a filled tile: the dots of the digits go off
static void cutdigits(int32_t number, int16_t x, int16_t y) {
  char text[12];
  snprintf(text, sizeof(text), "%ld", (long)number);
  for (const char *c = text; *c; c++, x += 4) {
    const uint8_t *glyph = fontDigits[*c - '0'];
    for (uint8_t row = 0; row < 5; row++)
      for (uint8_t col = 0; col < 3; col++)
        if (glyph[row] & (4 >> col)) setdot(x + col, y + row, false);
  }
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t tx = 0; tx < 4; tx++) {
    for (uint8_t ty = 0; ty < 4; ty++) {
      int16_t x0 = DB_LEFT + tx * (DB_TILE + 1), y0 = DB_TOP + ty * (DB_TILE + 1);
      uint8_t power = board[tx][ty];
      if (power == 0) {
        setdot(x0 + DB_TILE / 2, y0 + DB_TILE / 2, true);
        continue;
      }
      for (int16_t x = 0; x < DB_TILE; x++)
        for (int16_t y = 0; y < DB_TILE; y++) setdot(x0 + x, y0 + y, true);
      int32_t number = ROWS >= 64 && power <= 13 ? (1L << power) : power;
      int16_t w = gamedigitswidth(number, 1);
      cutdigits(number, x0 + (DB_TILE - w) / 2, y0 + (DB_TILE - 5) / 2);
    }
  }
}

static void doublingsetup() {
  memset(board, 0, sizeof(board));
  score = 0;
  addtile();
  addtile();
  draw();
}

// Slides one line of four tiles towards index 0, true if something moved
static bool slide(uint8_t *line[4]) {
  bool moved = false;
  uint8_t out = 0;
  bool merged = false;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t v = *line[i];
    if (v == 0) continue;
    if (out > 0 && !merged && *line[out - 1] == v) {
      (*line[out - 1])++;
      score += 1UL << (v + 1);
      merged = true;
      *line[i] = 0;
      moved = true;
      continue;
    }
    merged = false;
    if (out != i) {
      *line[out] = v;
      *line[i] = 0;
      moved = true;
    }
    out++;
  }
  return moved;
}

static bool move(uint8_t button) {
  bool moved = false;
  for (uint8_t k = 0; k < 4; k++) {
    uint8_t *line[4];
    for (uint8_t i = 0; i < 4; i++) {
      switch (button) {
        case BUTTON_LEFT: line[i] = &board[i][k]; break;
        case BUTTON_RIGHT: line[i] = &board[3 - i][k]; break;
        case BUTTON_UP: line[i] = &board[k][i]; break;
        default: line[i] = &board[k][3 - i]; break;
      }
    }
    moved |= slide(line);
  }
  return moved;
}

static bool canmove() {
  for (uint8_t x = 0; x < 4; x++) {
    for (uint8_t y = 0; y < 4; y++) {
      if (board[x][y] == 0) return true;
      if (x < 3 && board[x][y] == board[x + 1][y]) return true;
      if (y < 3 && board[x][y] == board[x][y + 1]) return true;
    }
  }
  return false;
}

static void doublingstep() {
  uint8_t b = gamepress(0);
  if (!(b & BUTTONS_DIRECTION)) return;
  if (move(b)) {
    addtile();
    draw();
    if (!canmove()) gameover(score);
  }
}

const Game doublingGame = { "doubling", "Verdubbelen", 1, doublingsetup, doublingstep, 40, nullptr, true };
