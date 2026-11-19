#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Chaos game: dots at random spots that together form a fern (Barnsley) or a
// Sierpinski triangle. The two take turns.

// On a larger screen the figures grow along, with as many more points as there are more dots
#define CHAOS_SCALE            (min(COLS, ROWS) / 32)
#define CHAOS_POINTS_PER_FRAME (4 * CHAOS_SCALE * CHAOS_SCALE)
#define CHAOS_POINTS           (3000 * CHAOS_SCALE * CHAOS_SCALE)  // Then the other figure

static bool fern = false;
static uint32_t points;
static float px, py;

static void chaossetup() {
  fern = !fern;
  points = 0;
  px = py = 0;
}

static void chaosstep() {
  for (uint8_t i = 0; i < CHAOS_POINTS_PER_FRAME; i++) {
    if (fern) {
      float x = px, y = py;
      uint8_t r = random(100);
      if (r < 1) {
        px = 0;
        py = 0.16f * y;
      } else if (r < 86) {
        px = 0.85f * x + 0.04f * y;
        py = -0.04f * x + 0.85f * y + 1.6f;
      } else if (r < 93) {
        px = 0.20f * x - 0.26f * y;
        py = 0.23f * x + 0.22f * y + 1.6f;
      } else {
        px = -0.15f * x + 0.28f * y;
        py = 0.26f * x + 0.24f * y + 0.44f;
      }
    } else {
      static const float corners[3][2] = { { (COLS - 1) / 2.0f, 0 }, { 0, ROWS - 1 }, { COLS - 1, ROWS - 1 } };
      uint8_t c = random(3);
      px = (px + corners[c][0]) / 2;
      py = (py + corners[c][1]) / 2;
    }

    if (++points < 20) {
      continue;  // Let the point settle onto the figure first
    }
    if (fern) {
      // The fern is about -2.2..2.7 wide and 0..10 high
      setdot(roundf(COLS / 2 + px * 3.0f * CHAOS_SCALE), roundf(ROWS - 1 - py * 3.1f * CHAOS_SCALE), true);
    } else {
      setdot(roundf(px), roundf(py), true);
    }
  }

  if (points >= CHAOS_POINTS) {
    memset(next_state, 0, sizeof(next_state));
    chaossetup();
  }
}

const Animation chaosAnimation = { "chaos", "Varen en Sierpinski", chaossetup, chaosstep, 30 };
