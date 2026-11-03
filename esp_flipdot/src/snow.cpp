#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Snow: flakes drift down slowly and stay where they land. When the snow is
// SNOW_LIMIT rows from the top it melts away and it starts snowing again.

#define FLAKES     20
#define SNOW_LIMIT 12
#define SNOW_MELT  6    // Dots that melt per frame

struct Flake {
  int16_t x, y;
  uint8_t period;  // Falls one row every period frames
};

static Flake flakes[FLAKES];
static bool snow[COLS][ROWS];
static bool melting;
static uint32_t frame;

static void newflake(Flake &f) {
  f.x = random(COLS);
  f.y = -random(1, ROWS);
  f.period = random(3, 7);
}

static void snowsetup() {
  memset(snow, 0, sizeof(snow));
  melting = false;
  frame = 0;
  for (uint8_t i = 0; i < FLAKES; i++) {
    newflake(flakes[i]);
  }
}

static void snowstep() {
  frame++;

  if (melting) {
    for (uint8_t i = 0; i < SNOW_MELT; i++) {
      snow[random(COLS)][random(ROWS)] = false;
    }
    bool any = false;
    for (uint8_t x = 0; x < COLS && !any; x++) {
      for (uint8_t y = 0; y < ROWS && !any; y++) {
        any = snow[x][y];
      }
    }
    melting = any;
  }

  memcpy(next_state, snow, sizeof(snow));

  for (uint8_t i = 0; i < FLAKES; i++) {
    Flake &f = flakes[i];
    if (frame % f.period == 0) {
      f.y++;
      if (random(3) == 0) {
        int16_t x = constrain(f.x + random(-1, 2), 0, COLS - 1);
        if (f.y < 0 || !snow[x][f.y]) {
          f.x = x;  // Drift, unless there is snow
        }
      }
    }
    if (f.y >= 0 && (f.y >= ROWS - 1 || snow[f.x][f.y + 1])) {
      if (!melting) {
        snow[f.x][f.y] = true;
        if (f.y < SNOW_LIMIT) {
          melting = true;
        }
      }
      newflake(f);
      continue;
    }
    setdot(f.x, f.y, true);
  }
}

const Animation snowAnimation = { "snow", "Sneeuw", snowsetup, snowstep, 60 };
