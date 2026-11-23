#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Crossing: get the frog to the top. First a road with traffic in both directions, then a strip of grass,
// then a river with floating logs: in the water you drown, on a log you drift along. Every crossing makes
// the traffic faster. The frog is drawn inverted, so it shows on a log as well as on the road.
// Three lives, bottom right. Score: crossings.

#define CR_CELL   (ROWS >= 64 ? 4 : 2)
#define CR_W      (COLS / CR_CELL)
#define CR_H      (ROWS / CR_CELL)       // 16 lanes on both screens
#define CR_GOAL   0
#define CR_RIVER  1                      // First river lane
#define CR_GRASS  (CR_H / 2 - 1)
#define CR_ROAD   (CR_H / 2)             // First road lane
#define CR_START  (CR_H - 1)
#define CR_LIVES  3

struct Lane {
  float offset;   // in dots
  float speed;    // dots per frame, + to the right
  bool cells[CR_W];
};

static Lane lanes[CR_H];
static float fx;              // Left of the frog, in dots
static int8_t fy;             // Lane of the frog
static uint8_t lives, dying, frame;
static uint16_t score;

static bool river(int8_t lane) { return lane >= CR_RIVER && lane < CR_GRASS; }
static bool road(int8_t lane) { return lane >= CR_ROAD && lane < CR_START; }

static void filllane(Lane &l, uint8_t minlen, uint8_t maxlen, uint8_t mingap, uint8_t maxgap) {
  memset(l.cells, 0, sizeof(l.cells));
  uint8_t c = random(CR_W);
  uint8_t filled = 0;
  while (filled < CR_W) {
    uint8_t len = random(minlen, maxlen + 1), gap = random(mingap, maxgap + 1);
    if (filled + len + mingap > CR_W) break;
    for (uint8_t i = 0; i < len; i++) l.cells[(c + i) % CR_W] = true;
    c = (c + len + gap) % CR_W;
    filled += len + gap;
  }
}

static void newlanes() {
  float faster = 1 + score * 0.12f;
  for (int8_t i = 0; i < CR_H; i++) {
    Lane &l = lanes[i];
    l.offset = 0;
    float base = CR_CELL * random(4, 12) / 100.0f * faster;
    l.speed = (i % 2 ? base : -base);
    if (road(i)) {
      bool truck = random(3) == 0;
      filllane(l, truck ? 3 : 1, truck ? 4 : 2, 3, 6);
    } else if (river(i)) {
      filllane(l, 3, 5, 2, 3);
    } else {
      l.speed = 0;
      memset(l.cells, 0, sizeof(l.cells));
    }
  }
}

// Is there a car or log under dot column x of a lane?
static bool solid(const Lane &l, int16_t x) {
  int16_t c = (int16_t)floorf((x - l.offset) / CR_CELL);
  c = ((c % CR_W) + CR_W) % CR_W;
  return l.cells[c];
}

static void resetfrog() {
  fx = (CR_W / 2) * CR_CELL;
  fy = CR_START;
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (int8_t i = 0; i < CR_H; i++) {
    int16_t top = i * CR_CELL;
    if (road(i) || river(i)) {  // The bottom row of a lane stays empty, so the lanes don't merge
      for (int16_t x = 0; x < COLS; x++) {
        if (solid(lanes[i], x)) drawline(x, top, x, top + CR_CELL - 2, true);
      }
    }
  }
  for (int16_t x = 0; x < COLS; x += 2) setdot(x, CR_CELL - 1, true);  // Edge of the goal
  for (uint8_t i = 1; i < lives; i++) setdot(COLS - 2 * i + 1, ROWS - 1, true);

  if (dying == 0 || (dying / 3) % 2 == 0) {
    int16_t x0 = roundf(fx), y0 = fy * CR_CELL;
    for (int16_t x = x0; x < x0 + CR_CELL - 1; x++) {
      for (int16_t y = y0; y < y0 + CR_CELL - 1; y++) {
        if (x >= 0 && x < COLS) next_state[x][y] = !next_state[x][y];
      }
    }
  }
}

static void crossingsetup() {
  lives = CR_LIVES;
  score = 0;
  dying = 0;
  newlanes();
  resetfrog();
  draw();
}

static void crossingstep() {
  frame++;
  for (Lane &l : lanes) {
    l.offset += l.speed;
    if (l.offset >= CR_W * CR_CELL) l.offset -= CR_W * CR_CELL;
    if (l.offset < 0) l.offset += CR_W * CR_CELL;
  }

  if (dying > 0) {
    while (gamepress(0)) {}
    if (--dying == 0) {
      if (lives == 0) {
        gameover(score);
        return;
      }
      resetfrog();
    }
    draw();
    return;
  }

  for (uint8_t b; (b = gamepress(0));) {
    if (b == BUTTON_UP) fy--;
    if (b == BUTTON_DOWN && fy < CR_START) fy++;
    if (b == BUTTON_LEFT) fx -= CR_CELL;
    if (b == BUTTON_RIGHT) fx += CR_CELL;
    fx = constrain(fx, 0.0f, (float)(COLS - CR_CELL));
    if (fy == CR_GOAL) break;
  }

  if (fy == CR_GOAL) {
    score++;
    newlanes();
    resetfrog();
    draw();
    return;
  }

  bool dead = false;
  int16_t x0 = roundf(fx);
  if (river(fy)) {
    fx += lanes[fy].speed;  // Drifting along
    x0 = roundf(fx);
    dead = fx < -CR_CELL / 2 || fx > COLS - CR_CELL / 2 || !solid(lanes[fy], x0 + CR_CELL / 2);
  } else if (road(fy)) {
    for (int16_t x = x0; x < x0 + CR_CELL - 1; x++) dead |= solid(lanes[fy], x);
  }
  if (dead) {
    lives--;
    dying = 30;
  }
  draw();
}

const Game crossingGame = { "crossing", "Oversteken", 1, crossingsetup, crossingstep, 40, nullptr, true };
