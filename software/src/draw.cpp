#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Drawing board: a blinking cursor, A switches the dot under it; hold A while moving to keep drawing
// (or erasing, if the first dot went off). B wipes the board, top to bottom. On the phone you can also
// draw with your finger on the screen.

static GameCursor cursor;
static bool pen, pendown;
static int16_t wipe;   // Row the wipe has reached, -1 if not wiping
static uint8_t frame;

static void drawsetup() {
  cursor = { COLS / 2, ROWS / 2, 0 };
  pendown = false;
  wipe = -1;
}

static void drawstep() {
  frame++;
  int16_t oldx = cursor.x, oldy = cursor.y;
  for (uint8_t b; (b = gamepress(0));) {
    if (b == BUTTON_A) {
      pen = !gamepaper[cursor.x][cursor.y];
      gamepaper[cursor.x][cursor.y] = pen;
      pendown = true;
    } else if (b == BUTTON_B) {
      wipe = 0;
    } else {
      cursorpress(cursor, b);
    }
  }
  uint8_t held = gameheld(0);
  cursorheld(cursor, held);
  pendown = pendown && (held & BUTTON_A);
  if (pendown) {  // Every dot from where the cursor was to where it is now
    for (int16_t x = oldx, y = oldy;; ) {
      gamepaper[x][y] = pen;
      if (x == cursor.x && y == cursor.y) break;
      x += (cursor.x > x) - (cursor.x < x);
      y += (cursor.y > y) - (cursor.y < y);
    }
  }

  if (wipe >= 0) {  // Two rows per frame
    for (uint8_t i = 0; i < 2 && wipe < ROWS; i++, wipe++)
      for (int16_t x = 0; x < COLS; x++) gamepaper[x][wipe] = false;
    if (wipe >= ROWS) wipe = -1;
  }

  memcpy(next_state, gamepaper, sizeof(gamepaper));
  if ((frame / 4) % 2 == 0) next_state[cursor.x][cursor.y] = !gamepaper[cursor.x][cursor.y];
}

static void drawtouch(int16_t x, int16_t y, bool on) {
  gamepaper[x][y] = on;
}

const Game drawGame = { "draw", "Tekenbord", 1, drawsetup, drawstep, 40, drawtouch, false };
