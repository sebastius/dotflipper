#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Rotating wireframe cube with perspective.

static const float vertices[8][3] = {
  { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
  { -1, -1,  1 }, { 1, -1,  1 }, { 1, 1,  1 }, { -1, 1,  1 },
};

static const uint8_t edges[12][2] = {
  { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },  // back
  { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },  // front
  { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },  // sides
};

static float ax, ay, az;

static void cubesetup() {
  ax = ay = az = 0;
}

static void cubestep() {
  ax += 0.04f;
  ay += 0.03f;
  az += 0.02f;

  int16_t px[8], py[8];
  for (uint8_t i = 0; i < 8; i++) {
    float x = vertices[i][0], y = vertices[i][1], z = vertices[i][2], t;

    t = y * cosf(ax) - z * sinf(ax); z = y * sinf(ax) + z * cosf(ax); y = t;  // around x
    t = x * cosf(ay) + z * sinf(ay); z = -x * sinf(ay) + z * cosf(ay); x = t; // around y
    t = x * cosf(az) - y * sinf(az); y = x * sinf(az) + y * cosf(az); x = t;  // around z

    // Perspective, the camera is 4 units away. Corners stay within 14 dots of the center on 32x32,
    // on a larger screen the cube grows along.
    float f = 18.0f * min(COLS, ROWS) / 32 / (z + 4.0f);
    px[i] = roundf((COLS - 1) / 2.0f + x * f);
    py[i] = roundf((ROWS - 1) / 2.0f + y * f);
  }

  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 0; i < 12; i++) {
    drawline(px[edges[i][0]], py[edges[i][0]], px[edges[i][1]], py[edges[i][1]], true);
  }
}

const Animation cubeAnimation = { "cube", "Kubus", cubesetup, cubestep, 40 };
