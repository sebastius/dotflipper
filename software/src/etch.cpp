#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Line drawing: the pen always draws, the arrows move it (two at once for a diagonal). The end of the
// line blinks. A or B shakes the board: the line falls apart and the board is empty again.

static int16_t px, py;
static uint8_t frame, shaking, slow;

static void etchsetup() {
  px = COLS / 2;
  py = ROWS / 2;
  gamepaper[px][py] = true;
  shaking = 0;
}

static void movepen(uint8_t buttons) {
  if (buttons & BUTTON_LEFT) px--;
  if (buttons & BUTTON_RIGHT) px++;
  if (buttons & BUTTON_UP) py--;
  if (buttons & BUTTON_DOWN) py++;
  px = constrain(px, 0, COLS - 1);
  py = constrain(py, 0, ROWS - 1);
  gamepaper[px][py] = true;
}

static void etchstep() {
  frame++;
  bool moved = false;
  for (uint8_t b; (b = gamepress(0));) {
    if (b == BUTTON_A || b == BUTTON_B) {
      shaking = 12;
    } else if (shaking == 0) {
      movepen(b);
      moved = true;
      slow = 6;  // A held button repeats after a moment
    }
  }
  uint8_t held = gameheld(0) & BUTTONS_DIRECTION;
  if (!held) {
    slow = 6;
  } else if (!moved && shaking == 0) {
    if (slow > 0) slow--;
    else if (frame % 2 == 0) movepen(held);
  }

  if (shaking > 0) {  // Every frame a quarter of the line falls off
    for (int16_t x = 0; x < COLS; x++)
      for (int16_t y = 0; y < ROWS; y++)
        if (gamepaper[x][y] && (random(4) == 0 || shaking == 1)) gamepaper[x][y] = false;
    if (--shaking == 0) gamepaper[px][py] = true;
  }

  memcpy(next_state, gamepaper, sizeof(gamepaper));
  if (shaking == 0 && (frame / 5) % 2 == 0) next_state[px][py] = false;
}

const Game etchGame = { "etch", "Lijntekenen", 1, etchsetup, etchstep, 30, nullptr, false };
