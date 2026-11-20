#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Crystal growth (diffusion limited aggregation): a particle wanders around at random
// and sticks as soon as it touches the crystal. Starts over when the crystal reaches the edge.

#define CRYSTAL_STEPS 400  // Random walk steps per frame at most, one particle sticks per frame
#define CRYSTAL_PAUSE 100  // Frames to show the finished crystal

static int16_t wx, wy;
static float radius;      // Distance of the furthest crystal dot from the center
static uint16_t waiting;

static float distance(int16_t x, int16_t y) {
  float dx = x - COLS / 2, dy = y - ROWS / 2;
  return sqrtf(dx * dx + dy * dy);
}

static void spawn() {
  float angle = random(6283) / 1000.0f;
  float r = min(radius + 4, (float)(COLS / 2 - 1));
  wx = constrain((int16_t)(COLS / 2 + cosf(angle) * r), 0, COLS - 1);
  wy = constrain((int16_t)(ROWS / 2 + sinf(angle) * r), 0, ROWS - 1);
}

static bool touches(int16_t x, int16_t y) {
  for (int8_t dx = -1; dx <= 1; dx++) {
    for (int8_t dy = -1; dy <= 1; dy++) {
      int16_t nx = x + dx, ny = y + dy;
      if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS && next_state[nx][ny]) {
        return true;
      }
    }
  }
  return false;
}

static void crystalsetup() {
  next_state[COLS / 2][ROWS / 2] = true;
  radius = 1;
  waiting = 0;
  spawn();
}

static void crystalstep() {
  if (waiting) {
    if (--waiting == 0) {
      memset(next_state, 0, sizeof(next_state));
      crystalsetup();
    }
    return;
  }

  for (uint16_t i = 0; i < CRYSTAL_STEPS; i++) {
    wx += random(-1, 2);
    wy += random(-1, 2);
    if (wx < 0 || wx >= COLS || wy < 0 || wy >= ROWS || distance(wx, wy) > radius + 8) {
      spawn();  // Wandered off
      continue;
    }
    if (touches(wx, wy)) {
      next_state[wx][wy] = true;
      radius = max(radius, distance(wx, wy));
      if (radius >= COLS / 2 - 1.5f) {
        waiting = CRYSTAL_PAUSE;
      }
      spawn();
      return;
    }
  }
}

const Animation crystalAnimation = { "crystal", "Kristalgroei", crystalsetup, crystalstep, 30 };
