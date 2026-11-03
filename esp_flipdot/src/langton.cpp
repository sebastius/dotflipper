#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Langton's Ant: on an off dot it turns right, on an on dot it turns left,
// flips the dot and steps forward, wrapping around the edges. Starts over after LANGTON_RESET steps.

#define LANGTON_STEPS 2       // Steps per frame
#define LANGTON_RESET 12000

static const int8_t directions[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };  // up, right, down, left

static int16_t antx, anty;
static uint8_t direction;
static uint16_t steps;

static void langtonsetup() {
  antx = COLS / 2;
  anty = ROWS / 2;
  direction = 0;
  steps = 0;
}

static void langtonstep() {
  for (uint8_t i = 0; i < LANGTON_STEPS; i++) {
    bool on = next_state[antx][anty];
    direction = (direction + (on ? 3 : 1)) % 4;
    next_state[antx][anty] = !on;
    antx = (antx + directions[direction][0] + COLS) % COLS;
    anty = (anty + directions[direction][1] + ROWS) % ROWS;
  }
  steps += LANGTON_STEPS;
  if (steps >= LANGTON_RESET) {
    memset(next_state, 0, sizeof(next_state));
    langtonsetup();
  }
}

const Animation langtonAnimation = { "langton", "Langtons mier", langtonsetup, langtonstep, 20 };
