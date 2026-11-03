#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Spirograph (hypotrochoid) or Lissajous figure that draws itself, then a new one.

#define SPIRO_STEP  0.02f
#define SPIRO_STEPS 4     // Steps per frame
#define SPIRO_PAUSE 80    // Frames to show the finished figure

// Small wheel radius as a fraction of the big wheel, the figure closes after 2 pi * numerator
static const uint8_t wheels[][2] = { { 3, 7 }, { 5, 8 }, { 2, 5 }, { 3, 8 }, { 4, 9 }, { 5, 7 } };

static bool lissajous;
static float r, d;         // Spirograph: small wheel and pen distance, the big wheel is 1
static float a, b, phase;  // Lissajous
static float t, tend;
static int16_t lastx, lasty;
static uint16_t waiting;

static void point(float t, int16_t &x, int16_t &y) {
  const float size = COLS / 2 - 1;
  float fx, fy;
  if (lissajous) {
    fx = sinf(a * t + phase);
    fy = sinf(b * t);
  } else {
    float k = (1 - r) / r;
    fx = ((1 - r) * cosf(t) + d * cosf(k * t)) / (1 - r + d);
    fy = ((1 - r) * sinf(t) - d * sinf(k * t)) / (1 - r + d);
  }
  x = roundf((COLS - 1) / 2.0f + fx * size);
  y = roundf((ROWS - 1) / 2.0f + fy * size);
}

static void newfigure() {
  memset(next_state, 0, sizeof(next_state));
  lissajous = random(3) == 0;
  if (lissajous) {
    a = random(1, 6);
    do {
      b = random(1, 6);
    } while (b == a);
    phase = random(628) / 100.0f;
    tend = TWO_PI;
  } else {
    const uint8_t *w = wheels[random(sizeof(wheels) / sizeof(wheels[0]))];
    r = (float)w[0] / w[1];
    d = r * random(50, 120) / 100.0f;
    tend = TWO_PI * w[0];
  }
  t = 0;
  waiting = 0;
  point(t, lastx, lasty);
}

static void spirographsetup() {
  newfigure();
}

static void spirographstep() {
  if (waiting) {
    if (--waiting == 0) {
      newfigure();
    }
    return;
  }
  for (uint8_t i = 0; i < SPIRO_STEPS; i++) {
    t += SPIRO_STEP;
    int16_t x, y;
    point(t, x, y);
    drawline(lastx, lasty, x, y, true);
    lastx = x;
    lasty = y;
    if (t >= tend) {
      waiting = SPIRO_PAUSE;
      return;
    }
  }
}

const Animation spirographAnimation = { "spirograph", "Spirograaf", spirographsetup, spirographstep, 30 };
