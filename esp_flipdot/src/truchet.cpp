#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Truchet tiles: every 8x8 tile has two quarter circles in opposite corners, so the
// lines of neighbouring tiles always connect. Every frame one tile turns a quarter.

#define TILE 8

static void drawtile(uint8_t tx, uint8_t ty, bool turned) {
  // Corners the arcs are centered on
  float ax = turned ? TILE : 0, ay = 0;
  float bx = turned ? 0 : TILE, by = TILE;
  for (uint8_t x = 0; x < TILE; x++) {
    for (uint8_t y = 0; y < TILE; y++) {
      float cx = x + 0.5f, cy = y + 0.5f;
      float da = sqrtf((cx - ax) * (cx - ax) + (cy - ay) * (cy - ay));
      float db = sqrtf((cx - bx) * (cx - bx) + (cy - by) * (cy - by));
      next_state[tx * TILE + x][ty * TILE + y] = (da >= 3 && da < 5) || (db >= 3 && db < 5);
    }
  }
}

static bool tiles[COLS / TILE][ROWS / TILE];

static void truchetsetup() {
  for (uint8_t tx = 0; tx < COLS / TILE; tx++) {
    for (uint8_t ty = 0; ty < ROWS / TILE; ty++) {
      tiles[tx][ty] = random(2);
      drawtile(tx, ty, tiles[tx][ty]);
    }
  }
}

static void truchetstep() {
  uint8_t tx = random(COLS / TILE), ty = random(ROWS / TILE);
  tiles[tx][ty] = !tiles[tx][ty];
  drawtile(tx, ty, tiles[tx][ty]);
}

const Animation truchetAnimation = { "truchet", "Truchet-tegels", truchetsetup, truchetstep, 300 };
