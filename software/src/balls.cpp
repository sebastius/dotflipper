#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Bouncing balls with gravity. A ball that has lost its bounce gets a new kick.

#define BALLS   3
#define GRAVITY 0.08f

struct Ball {
  float x, y, vx, vy;
  uint8_t r;
};

static Ball balls[BALLS];

static void ballssetup() {
  for (uint8_t i = 0; i < BALLS; i++) {
    Ball &b = balls[i];
    b.r = random(1, 4);
    b.x = random(b.r, COLS - b.r);
    b.y = random(b.r, ROWS / 2);
    b.vx = random(5, 15) / 10.0f * (random(2) ? 1 : -1);
    b.vy = 0;
  }
}

static void ballsstep() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 0; i < BALLS; i++) {
    Ball &b = balls[i];
    b.vy += GRAVITY;
    b.x += b.vx;
    b.y += b.vy;

    if (b.x < b.r) {
      b.x = b.r;
      b.vx = -b.vx;
    } else if (b.x > COLS - 1 - b.r) {
      b.x = COLS - 1 - b.r;
      b.vx = -b.vx;
    }
    if (b.y < b.r) {
      b.y = b.r;
      b.vy = -b.vy;
    } else if (b.y > ROWS - 1 - b.r) {
      b.y = ROWS - 1 - b.r;
      b.vy = -b.vy * 0.92f;
      if (fabsf(b.vy) < 0.5f) {
        b.vy = -random(15, 25) / 10.0f;
      }
    }

    int16_t cx = roundf(b.x), cy = roundf(b.y);
    for (int16_t dx = -b.r; dx <= b.r; dx++) {
      for (int16_t dy = -b.r; dy <= b.r; dy++) {
        if (dx * dx + dy * dy <= b.r * b.r + b.r) {
          setdot(cx + dx, cy + dy, true);
        }
      }
    }
  }
}

const Animation ballsAnimation = { "balls", "Stuiterende ballen", ballssetup, ballsstep, 30 };
