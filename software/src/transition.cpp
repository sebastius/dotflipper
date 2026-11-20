#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "transition.h"

#define DISSOLVE_DOTS 6     // Dots per frame when dissolving
#define REMOVE_DOTS   8     // Old dots per frame that disappear before falling in
#define FALL_SPAWN    2     // New falling dots per frame
#define FALL_GRAVITY  0.12f
#define FALL_MAX      1.5f  // Rows per frame
#define SAND_DELAY    12    // Grains start falling within this many frames

struct Particle {
  uint8_t x;
  uint8_t target;  // Row where a falling dot lands
  float y, vy;
  uint8_t delay;
};

static Transition type;
static uint8_t phase;
static boolean target[COLS][ROWS];  // The new picture
static boolean base[COLS][ROWS];    // What stays put, without the moving particles
static uint16_t dots[COLS * ROWS];  // Dots still to change as x * ROWS + y
static uint16_t dotcount, dotdone;
static Particle particles[COLS * ROWS];
static uint16_t particlecount;

static void shuffle(uint16_t start, uint16_t count) {
  for (uint16_t i = count; i > 1; i--) {
    uint16_t j = start + random(i);
    uint16_t t = dots[start + i - 1];
    dots[start + i - 1] = dots[j];
    dots[j] = t;
  }
}

// Collects the dots that are 'now' on screen and 'then' in the new picture, in random order
static void collect(bool now, bool then) {
  dotcount = dotdone = 0;
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      if (current_state[x][y] == now && target[x][y] == then) {
        dots[dotcount++] = x * ROWS + y;
      }
    }
  }
  shuffle(0, dotcount);
}

// Collects every dot that changes, in random order
static void collectchanged() {
  dotcount = dotdone = 0;
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      if (current_state[x][y] != target[x][y]) {
        dots[dotcount++] = x * ROWS + y;
      }
    }
  }
  shuffle(0, dotcount);
}

// Puts the base plus the moving particles in next_state
static void compose() {
  memcpy(next_state, base, sizeof(base));
  for (uint16_t i = 0; i < particlecount; i++) {
    setdot(particles[i].x, roundf(particles[i].y), true);
  }
}

// Applies up to count dots from the list, true when the list is done
static bool apply(uint8_t count, bool on) {
  for (uint8_t i = 0; i < count && dotdone < dotcount; i++, dotdone++) {
    uint16_t dot = dots[dotdone];
    base[dot / ROWS][dot % ROWS] = on;
  }
  return dotdone >= dotcount;
}

void transitionstart(Transition t) {
  type = t;
  phase = 0;
  particlecount = 0;
  memcpy(target, next_state, sizeof(target));
  memcpy(base, current_state, sizeof(base));

  switch (type) {
    case TRANSITION_NONE:
      memcpy(base, target, sizeof(base));
      break;
    case TRANSITION_DISSOLVE:
      collectchanged();
      break;
    case TRANSITION_FALL:
      collect(true, false);
      break;
    case TRANSITION_SAND:
      collect(true, false);
      for (uint16_t i = 0; i < dotcount; i++) {
        uint8_t x = dots[i] / ROWS, y = dots[i] % ROWS;
        base[x][y] = false;
        particles[particlecount++] = { x, 0, (float)y, 0, (uint8_t)random(SAND_DELAY) };
      }
      collect(false, true);
      break;
  }
  compose();
}

bool transitionstep() {
  switch (type) {
    case TRANSITION_NONE:
      return false;

    case TRANSITION_DISSOLVE:
      for (uint8_t i = 0; i < DISSOLVE_DOTS && dotdone < dotcount; i++, dotdone++) {
        uint16_t dot = dots[dotdone];
        base[dot / ROWS][dot % ROWS] = target[dot / ROWS][dot % ROWS];
      }
      compose();
      return dotdone < dotcount;

    case TRANSITION_FALL:
      if (phase == 0) {
        if (apply(REMOVE_DOTS, false)) {
          // Falling dots, bottom rows first
          phase = 1;
          dotcount = dotdone = 0;
          for (int16_t y = ROWS - 1; y >= 0; y--) {
            uint16_t start = dotcount;
            for (uint8_t x = 0; x < COLS; x++) {
              if (target[x][y] && !base[x][y]) {
                dots[dotcount++] = x * ROWS + y;
              }
            }
            shuffle(start, dotcount - start);
          }
        }
        compose();
        return true;
      }
      for (uint8_t i = 0; i < FALL_SPAWN && dotdone < dotcount; i++, dotdone++) {
        uint16_t dot = dots[dotdone];
        particles[particlecount++] = { (uint8_t)(dot / ROWS), (uint8_t)(dot % ROWS), -1, 0.3f, 0 };
      }
      for (uint16_t i = 0; i < particlecount; ) {
        Particle &p = particles[i];
        p.vy = min(p.vy + FALL_GRAVITY, FALL_MAX);
        p.y += p.vy;
        if (p.y >= p.target) {
          base[p.x][p.target] = true;
          particles[i] = particles[--particlecount];  // Landed
        } else {
          i++;
        }
      }
      compose();
      return dotdone < dotcount || particlecount > 0;

    case TRANSITION_SAND:
      if (phase == 0) {
        for (uint16_t i = 0; i < particlecount; ) {
          Particle &p = particles[i];
          if (p.delay) {
            p.delay--;
          } else {
            p.vy = min(p.vy + FALL_GRAVITY, FALL_MAX);
            p.y += p.vy;
            p.x = constrain(p.x + random(-1, 2) * (random(4) == 0), 0, COLS - 1);  // A bit of crumbling
          }
          if (p.y >= ROWS) {
            particles[i] = particles[--particlecount];  // Fell out
          } else {
            i++;
          }
        }
        if (particlecount == 0) {
          phase = 1;
        }
        compose();
        return true;
      }
      apply(DISSOLVE_DOTS, true);
      compose();
      return dotdone < dotcount;
  }
  return false;
}
