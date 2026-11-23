#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Life sandbox: draw cells like on the drawing board (cursor and A, or your finger on the phone), then
// B starts and stops the Game of Life. The edges wrap around. It stops by itself when nothing changes.

static GameCursor cursor;
static bool running;
static uint8_t frame;

static void generation() {
  static bool next[COLS][ROWS];
  bool changed = false;
  for (int16_t x = 0; x < COLS; x++) {
    for (int16_t y = 0; y < ROWS; y++) {
      uint8_t n = 0;
      for (int8_t dx = -1; dx <= 1; dx++)
        for (int8_t dy = -1; dy <= 1; dy++)
          if ((dx || dy) && gamepaper[(x + dx + COLS) % COLS][(y + dy + ROWS) % ROWS]) n++;
      next[x][y] = n == 3 || (n == 2 && gamepaper[x][y]);
      changed |= next[x][y] != gamepaper[x][y];
    }
  }
  memcpy(gamepaper, next, sizeof(gamepaper));
  if (!changed) running = false;
}

static void lifeboxsetup() {
  cursor = { COLS / 2, ROWS / 2, 0 };
  running = false;
}

static void lifeboxstep() {
  frame++;
  for (uint8_t b; (b = gamepress(0));) {
    if (b == BUTTON_B) running = !running;
    else if (b == BUTTON_A) gamepaper[cursor.x][cursor.y] = !gamepaper[cursor.x][cursor.y];
    else cursorpress(cursor, b);
  }
  cursorheld(cursor, gameheld(0));
  if (running && frame % 4 == 0) generation();

  memcpy(next_state, gamepaper, sizeof(gamepaper));
  if (!running && (frame / 4) % 2 == 0) next_state[cursor.x][cursor.y] = !gamepaper[cursor.x][cursor.y];
}

static void lifeboxtouch(int16_t x, int16_t y, bool on) {
  gamepaper[x][y] = on;
}

const Game lifeGame = { "life", "Life-zandbak", 1, lifeboxsetup, lifeboxstep, 40, lifeboxtouch, false };
