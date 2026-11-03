#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Lightning: now and then a branching bolt strikes and flickers, dark in between.

#define BRANCHES 3

static bool bolt[COLS][ROWS];
static uint16_t wait;
static uint8_t phase;

static void branch(int16_t x, int16_t y, int8_t direction, uint8_t length) {
  for (uint8_t i = 0; i < length && y < ROWS; i++) {
    x += direction * random(0, 2);
    y++;
    if (x < 0 || x >= COLS) return;
    bolt[x][y] = true;
  }
}

static void newbolt() {
  memset(bolt, 0, sizeof(bolt));
  int16_t x = random(6, COLS - 6);
  uint8_t branches = 0;
  for (int16_t y = 0; y < ROWS; y++) {
    bolt[x][y] = true;
    if (branches < BRANCHES && y < ROWS - 8 && random(100) < 12) {
      branch(x, y, random(2) ? 1 : -1, random(4, 11));
      branches++;
    }
    x = constrain(x + random(-1, 2), 0, COLS - 1);
  }
}

static void lightningsetup() {
  wait = random(10, 40);
  phase = 0;
}

static void lightningstep() {
  switch (phase) {
    case 0:  // Dark
      if (--wait == 0) {
        newbolt();
        phase = 1;
      }
      return;
    case 1:
    case 2:
    case 4:
      memcpy(next_state, bolt, sizeof(bolt));
      break;
    case 3:
    case 5:
      memset(next_state, 0, sizeof(next_state));
      break;
  }
  if (++phase > 5) {
    phase = 0;
    wait = random(15, 70);
  }
}

const Animation lightningAnimation = { "lightning", "Bliksem", lightningsetup, lightningstep, 60 };
