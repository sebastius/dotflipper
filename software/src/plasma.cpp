#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Plasma: a sum of moving sine waves, shown in fake greyscale with ordered (Bayer) dithering.

static const uint8_t bayer[4][4] = {
  {  0,  8,  2, 10 },
  { 12,  4, 14,  6 },
  {  3, 11,  1,  9 },
  { 15,  7, 13,  5 },
};

static float t;

static void plasmasetup() {
  t = 0;
}

static void plasmastep() {
  t += 0.15f;
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      float dx = x - COLS / 2.0f;
      float dy = y - ROWS / 2.0f;
      float v = sinf(x * 0.25f + t)
              + sinf(y * 0.30f - t * 0.7f)
              + sinf((x + y) * 0.18f + t * 0.5f)
              + sinf(sqrtf(dx * dx + dy * dy) * 0.30f - t);
      // v is -4..4, scale to a level of 0..16
      uint8_t level = (v + 4.0f) * 2.0f;
      next_state[x][y] = level > bayer[x & 3][y & 3];
    }
  }
}

const Animation plasmaAnimation = { "plasma", "Plasma", plasmasetup, plasmastep, 50 };
