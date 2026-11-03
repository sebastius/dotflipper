#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Elementary 1D cellular automaton: every new bottom row follows from the row above it,
// the screen scrolls up. A random rule is picked every time, wrapping around the edges.

// Only rules where an empty neighbourhood stays empty, others flash whole rows
static const uint8_t rules[] = { 30, 90, 110, 150, 126, 22 };
static uint8_t rule;

static void automatonsetup() {
  rule = rules[random(sizeof(rules))];
  Serial.printf("Rule %u\n", rule);

  if (rule == 110) {
    // Rule 110 needs a random start to get going
    for (uint8_t x = 0; x < COLS; x++) {
      next_state[x][ROWS - 1] = random(2);
    }
  } else {
    next_state[COLS / 2][ROWS - 1] = true;
  }
}

static void automatonstep() {
  // Scroll everything one row up
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS - 1; y++) {
      next_state[x][y] = next_state[x][y + 1];
    }
  }

  // The old bottom row is now at ROWS - 2
  for (uint8_t x = 0; x < COLS; x++) {
    uint8_t pattern = (next_state[(x + COLS - 1) % COLS][ROWS - 2] << 2)
                    | (next_state[x][ROWS - 2] << 1)
                    |  next_state[(x + 1) % COLS][ROWS - 2];
    next_state[x][ROWS - 1] = (rule >> pattern) & 1;
  }
}

const Animation automatonAnimation = { "automaton", "1D-automaat", automatonsetup, automatonstep, 150 };
