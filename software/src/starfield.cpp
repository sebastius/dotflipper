#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Starfield: stars fly towards you from the center, close stars get a short streak.

#define STARS      40
#define STAR_SPEED 0.02f

struct Star {
  float x, y, z;  // x and y -1..1, z 1 (far away) to 0 (here)
};

static Star stars[STARS];

static void newstar(Star &s, float z) {
  s.x = random(-1000, 1001) / 1000.0f;
  s.y = random(-1000, 1001) / 1000.0f;
  s.z = z;
}

static bool project(const Star &s, float z, int16_t &px, int16_t &py) {
  px = roundf((COLS - 1) / 2.0f + s.x / z * (COLS / 2));
  py = roundf((ROWS - 1) / 2.0f + s.y / z * (ROWS / 2));
  return px >= 0 && px < COLS && py >= 0 && py < ROWS;
}

static void starfieldsetup() {
  for (uint8_t i = 0; i < STARS; i++) {
    newstar(stars[i], random(50, 1001) / 1000.0f);  // Spread out, so they don't all arrive at once
  }
}

static void starfieldstep() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 0; i < STARS; i++) {
    Star &s = stars[i];
    s.z -= STAR_SPEED;
    int16_t px, py;
    if (s.z <= 0.05f || !project(s, s.z, px, py)) {
      newstar(s, 1.0f);
      continue;
    }
    setdot(px, py, true);
    if (s.z < 0.3f && project(s, s.z + STAR_SPEED, px, py)) {
      setdot(px, py, true);
    }
  }
}

const Animation starfieldAnimation = { "starfield", "Sterrenveld", starfieldsetup, starfieldstep, 40 };
