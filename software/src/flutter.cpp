#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Flutter: a bird that falls, every press of A or up gives it a flap. Fly through the openings in the
// pipes that come from the right; touching a pipe or the ground ends it. Score: pipes passed.
// Moving a solid pipe one dot only flips its front and back column, so this stays light on the flips.

#define FL_BIG    (ROWS >= 64)
#define FL_BIRD   (FL_BIG ? 2 : 1)
#define FL_X      (COLS / 4)
#define FL_PIPE   (FL_BIG ? 3 : 2)       // Width of a pipe
#define FL_GAP    (FL_BIG ? 18 : 11)     // Height of the opening
#define FL_APART  (FL_BIG ? 26 : 16)     // From pipe to pipe
#define FL_PIPES  (COLS / FL_APART + 2)
#define FL_SPEED  (FL_BIG ? 0.75f : 0.5f)
#define FL_FALL   (FL_BIG ? 0.09f : 0.06f)
#define FL_FLAP   (FL_BIG ? -0.95f : -0.65f)

struct Pipe {
  float x;
  int16_t gap;  // Top of the opening
  bool passed;
};

static Pipe pipes[FL_PIPES];
static float y, vy;
static uint16_t score;

static void newpipe(Pipe &p, float x, int16_t near) {
  int16_t lo = max(2, near - ROWS / 3), hi = min(ROWS - FL_GAP - 2, near + ROWS / 3);
  p = { x, (int16_t)random(lo, hi + 1), false };
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (Pipe &p : pipes) {
    int16_t px = roundf(p.x);
    for (int16_t c = px; c < px + FL_PIPE; c++) {
      if (p.gap > 0) drawline(c, 0, c, p.gap - 1, true);
      drawline(c, p.gap + FL_GAP, c, ROWS - 1, true);
    }
  }
  int16_t by = roundf(y);
  for (int16_t i = 0; i < FL_BIRD; i++)
    for (int16_t j = 0; j < FL_BIRD; j++) setdot(FL_X + i, by + j, true);
}

static void fluttersetup() {
  y = ROWS / 2 - FL_BIRD / 2;
  vy = 0;
  score = 0;
  int16_t gap = (ROWS - FL_GAP) / 2;
  for (uint8_t i = 0; i < FL_PIPES; i++) {
    newpipe(pipes[i], COLS + 4 + i * FL_APART, gap);
    gap = pipes[i].gap;
  }
  draw();
}

static void flutterstep() {
  for (uint8_t b; (b = gamepress(0));) {
    if (b == BUTTON_A || b == BUTTON_UP || b == BUTTON_B) vy = FL_FLAP;
  }
  vy = min(vy + FL_FALL, 1.2f);
  y += vy;
  if (y < 0) {
    y = 0;
    vy = 0;
  }

  float last = 0;
  int16_t lastgap = 0;
  for (Pipe &p : pipes) {
    if (p.x > last) {
      last = p.x;
      lastgap = p.gap;
    }
  }
  bool hit = y > ROWS - FL_BIRD;
  int16_t by = roundf(y);
  for (Pipe &p : pipes) {
    p.x -= FL_SPEED;
    if (p.x < -FL_PIPE) {
      newpipe(p, last + FL_APART, lastgap);
      last = p.x;
      lastgap = p.gap;
      continue;
    }
    int16_t px = roundf(p.x);
    if (FL_X + FL_BIRD > px && FL_X < px + FL_PIPE && (by < p.gap || by + FL_BIRD > p.gap + FL_GAP)) {
      hit = true;
    }
    if (!p.passed && px + FL_PIPE <= FL_X) {
      p.passed = true;
      score++;
    }
  }
  draw();
  if (hit) gameover(score);
}

const Game flutterGame = { "flutter", "Fladder", 1, fluttersetup, flutterstep, 40, nullptr, true };
