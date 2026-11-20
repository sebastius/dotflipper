#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Metaballs: a few blobs that melt together and come apart, only their outline is shown.

#define BLOBS 3

struct Blob {
  float x, y, vx, vy, r;
};

static Blob blobs[BLOBS];

static void metaballssetup() {
  for (uint8_t i = 0; i < BLOBS; i++) {
    Blob &b = blobs[i];
    b.r = random(40, 66) / 10.0f;
    b.x = random(COLS);
    b.y = random(ROWS);
    b.vx = random(20, 51) / 100.0f * (random(2) ? 1 : -1);
    b.vy = random(20, 51) / 100.0f * (random(2) ? 1 : -1);
  }
}

static void metaballsstep() {
  for (uint8_t i = 0; i < BLOBS; i++) {
    Blob &b = blobs[i];
    b.x += b.vx;
    b.y += b.vy;
    if (b.x < 0 || b.x > COLS - 1) b.vx = -b.vx;
    if (b.y < 0 || b.y > ROWS - 1) b.vy = -b.vy;
  }

  static bool inside[COLS][ROWS];
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      float field = 0;
      for (uint8_t i = 0; i < BLOBS; i++) {
        float dx = x - blobs[i].x, dy = y - blobs[i].y;
        field += blobs[i].r * blobs[i].r / (dx * dx + dy * dy + 0.01f);
      }
      inside[x][y] = field >= 1;
    }
  }

  // Outline: inside dots with a neighbour outside
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      next_state[x][y] = inside[x][y] &&
        (x == 0 || y == 0 || x == COLS - 1 || y == ROWS - 1 ||
         !inside[x - 1][y] || !inside[x + 1][y] || !inside[x][y - 1] || !inside[x][y + 1]);
    }
  }
}

const Animation metaballsAnimation = { "metaballs", "Metaballs", metaballssetup, metaballsstep, 40 };
