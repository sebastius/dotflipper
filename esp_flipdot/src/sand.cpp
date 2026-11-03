#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Falling sand: a wandering source drops grains that slide off each other into piles.
// When the pile reaches the source the floor opens and everything falls out.

static int16_t source;
static int8_t sourcedirection;
static bool draining;
static uint32_t frame;

static void sandsetup() {
  source = random(COLS);
  sourcedirection = random(2) ? 1 : -1;
  draining = false;
  frame = 0;
}

static bool empty(int16_t x, int16_t y) {
  return x >= 0 && x < COLS && !next_state[x][y];
}

static void sandstep() {
  frame++;

  if (draining) {
    // Everything falls one row, out at the bottom
    bool any = false;
    for (uint8_t x = 0; x < COLS; x++) {
      for (int16_t y = ROWS - 1; y > 0; y--) {
        next_state[x][y] = next_state[x][y - 1];
        any |= next_state[x][y];
      }
      next_state[x][0] = false;
    }
    if (!any) {
      sandsetup();
    }
    return;
  }

  // From the bottom up, so every grain moves at most once per frame.
  // Alternate the direction per frame so piles don't lean to one side.
  bool lefttoright = frame & 1;
  for (int16_t y = ROWS - 2; y >= 0; y--) {
    for (uint8_t i = 0; i < COLS; i++) {
      int16_t x = lefttoright ? i : COLS - 1 - i;
      if (!next_state[x][y]) {
        continue;
      }
      int8_t side = random(2) ? 1 : -1;
      int16_t to = -1;
      if (empty(x, y + 1)) {
        to = x;
      } else if (empty(x + side, y + 1)) {
        to = x + side;
      } else if (empty(x - side, y + 1)) {
        to = x - side;
      }
      if (to >= 0) {
        next_state[x][y] = false;
        next_state[to][y + 1] = true;
      }
    }
  }

  // The source wanders along the top
  if (random(30) == 0) {
    sourcedirection = -sourcedirection;
  }
  if (frame % 3 == 0) {
    source += sourcedirection;
    if (source < 0 || source >= COLS) {
      sourcedirection = -sourcedirection;
      source += 2 * sourcedirection;
    }
  }

  if (next_state[source][0]) {
    draining = true;  // Full
  } else {
    next_state[source][0] = true;
  }
}

const Animation sandAnimation = { "sand", "Vallend zand", sandsetup, sandstep, 30 };
