#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"
#include "clockface.h"

// Life-like cellular automata, wrapping around the edges. Every rule says for how many
// live neighbours a dead cell is born and a live cell survives (bit n = n neighbours).
// Starts over every GOL_GENERATIONS, or as soon as nothing changes anymore.

struct LifeRule {
  uint16_t birth;
  uint16_t survive;
  uint8_t density;  // percentage of cells alive at start
};

#define N(n) (1 << (n))

static const LifeRule conway   = { N(3),                      N(2) | N(3),                             GOL_DENSITY };
static const LifeRule highlife = { N(3) | N(6),               N(2) | N(3),                             GOL_DENSITY };
static const LifeRule daynight = { N(3) | N(6) | N(7) | N(8), N(3) | N(4) | N(6) | N(7) | N(8),        50 };
static const LifeRule seeds    = { N(2),                      0,                                       5 };

static const LifeRule *rule;
static uint16_t generation;

static void lifenext() {
  for (int r = 0; r < ROWS; r++) { // for each row
    for (int c = 0; c < COLS; c++) { // and each column

      // count how many live neighbors this cell has
      int liveNeighbors = 0;
      for (int i = -1; i < 2; i++) {
        int y = (r + i + ROWS) % ROWS;
        for (int j = -1; j < 2; j++) {
          if (i != 0 || j != 0) {
            int x = (c + j + COLS) % COLS;
            if (current_state[x][y]) {
              liveNeighbors++;
            }
          }
        }
      }

      // apply the rules
      uint16_t mask = current_state[c][r] ? rule->survive : rule->birth;
      next_state[c][r] = (mask >> liveNeighbors) & 1;
    }
  }
}

static void liferandomize() {
  randomSeed(millis());
  generation = 0;

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      next_state[c][r] = random(100) < rule->density;
    }
  }
}

static void lifesetup(const LifeRule *r) {
  rule = r;
  liferandomize();
  fullupdate();  // lifenext works from current_state
}

static void lifestep() {
  if (++generation >= GOL_GENERATIONS) {
    liferandomize();
    return;
  }
  lifenext();
  if (memcmp(current_state, next_state, sizeof(next_state)) == 0) {
    liferandomize();  // Nothing moves anymore
  }
}

// Clock that comes to life: shows the time for CLOCKLIFE_HOLD frames, then the digits
// follow Conway's rules. As soon as nothing changes anymore the time is back.

#define CLOCKLIFE_HOLD 40

static uint16_t hold;

static void clocklifestart() {
  struct tm now;
  if (getLocalTime(&now, 0)) {
    clockdraw(now, CLOCK_BIG, false);
  } else {
    clockdrawunsynced();
  }
  fullupdate();  // lifenext works from current_state
  rule = &conway;
  generation = 0;
  hold = CLOCKLIFE_HOLD;
}

static void clocklifestep() {
  if (hold) {
    hold--;
    return;
  }
  if (++generation >= GOL_GENERATIONS) {
    clocklifestart();
    return;
  }
  lifenext();
  if (memcmp(current_state, next_state, sizeof(next_state)) == 0) {
    clocklifestart();
  }
}

static void golsetup()      { lifesetup(&conway); }
static void highlifesetup() { lifesetup(&highlife); }
static void daynightsetup() { lifesetup(&daynight); }
static void seedssetup()    { lifesetup(&seeds); }

const Animation golAnimation      = { "gol",      "Game of Life",  golsetup,      lifestep, GOL_FRAMETIME };
const Animation highlifeAnimation = { "highlife", "HighLife",      highlifesetup, lifestep, GOL_FRAMETIME };
const Animation daynightAnimation = { "daynight", "Day & Night",   daynightsetup, lifestep, GOL_FRAMETIME };
const Animation seedsAnimation    = { "seeds",    "Seeds",         seedssetup,    lifestep, GOL_FRAMETIME };
const Animation clocklifeAnimation = { "clocklife", "Klok in Life", clocklifestart, clocklifestep, GOL_FRAMETIME };
