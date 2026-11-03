#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Pendulum wave: a row of pendulums with slightly different lengths. They start in a line,
// drift into waves and spirals and line up again after PENDULUM_CYCLE seconds.

#define PENDULUMS      16
#define PENDULUM_BASE  10      // Swings of the slowest pendulum per cycle, every next one swings once more
#define PENDULUM_CYCLE 40.0f   // in seconds at 100% speed
#define PENDULUM_FRAME 0.04f   // seconds per frame

static float t;

static void pendulumsetup() {
  t = 0;
}

static void pendulumstep() {
  t += PENDULUM_FRAME;
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 0; i < PENDULUMS; i++) {
    float omega = TWO_PI * (PENDULUM_BASE + i) / PENDULUM_CYCLE;
    int16_t y = roundf((ROWS - 2) / 2.0f - (ROWS / 2 - 2) * cosf(omega * t));
    int16_t x = i * COLS / PENDULUMS;
    // 2x2 bobs
    setdot(x, y, true);
    setdot(x + 1, y, true);
    setdot(x, y + 1, true);
    setdot(x + 1, y + 1, true);
  }
}

const Animation pendulumAnimation = { "pendulum", "Slingergolf", pendulumsetup, pendulumstep, 40 };
