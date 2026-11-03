#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Hilbert curve: a space filling line that folds through the whole screen.
// It is drawn segment by segment, then erased the same way, and again.

#define HILBERT_N     16   // Points per side, 2 dots apart
#define HILBERT_PAUSE 60   // Frames between drawing and erasing

static uint16_t segment;
static bool erasing;
static uint16_t waiting;

// Position of point d along the curve (the usual d2xy)
static void hilbertpoint(uint16_t d, int16_t &x, int16_t &y) {
  x = y = 0;
  for (uint16_t s = 1; s < HILBERT_N; s *= 2) {
    uint16_t rx = 1 & (d / 2);
    uint16_t ry = 1 & (d ^ rx);
    if (ry == 0) {
      if (rx == 1) {
        x = s - 1 - x;
        y = s - 1 - y;
      }
      int16_t t = x;
      x = y;
      y = t;
    }
    x += s * rx;
    y += s * ry;
    d /= 4;
  }
}

static void hilbertsetup() {
  segment = 0;
  erasing = false;
  waiting = 0;
}

static void hilbertstep() {
  if (waiting) {
    waiting--;
    return;
  }
  int16_t x0, y0, x1, y1;
  hilbertpoint(segment, x0, y0);
  hilbertpoint(segment + 1, x1, y1);
  drawline(2 * x0, 2 * y0, 2 * x1, 2 * y1, !erasing);
  if (++segment >= HILBERT_N * HILBERT_N - 1) {
    segment = 0;
    erasing = !erasing;
    waiting = HILBERT_PAUSE;
  }
}

const Animation hilbertAnimation = { "hilbert", "Hilbert-curve", hilbertsetup, hilbertstep, 30 };
