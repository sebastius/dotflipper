#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Rain: every drop is one dot with an optional short tail, falling at its own speed.

struct Drop {
  uint8_t x;
  int16_t y;       // Row of the drop itself, negative is above the screen
  uint8_t period;  // Falls one row every period frames, so 1 is fastest
  uint8_t tail;    // Number of dots above the drop
};

static Drop drops[RAIN_DROPS];
static uint32_t frame;

static void newdrop(Drop &d) {
  d.x = random(COLS);
  d.y = -random(1, ROWS);  // Start somewhere above the screen, so drops don't arrive all at once
  d.period = random(1, RAIN_SLOWEST + 1);
  d.tail = random(RAIN_MAX_TAIL + 1);
}

static void rainsetup() {
  frame = 0;
  for (uint8_t i = 0; i < RAIN_DROPS; i++) {
    newdrop(drops[i]);
  }
}

static void rainstep() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 0; i < RAIN_DROPS; i++) {
    Drop &d = drops[i];
    if (frame % d.period == 0) {
      d.y++;
    }
    for (int16_t y = d.y - d.tail; y <= d.y; y++) {
      setdot(d.x, y, true);
    }
    if (d.y - d.tail >= ROWS) {
      newdrop(d);
    }
  }
  frame++;
}

const Animation rainAnimation = { "rain", "Regen", rainsetup, rainstep, RAIN_FRAMETIME };
