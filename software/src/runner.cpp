#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Runner: a little runner on a track, posts come from the right, A or up jumps. It gets faster and
// faster. The legs move as it runs, the pebbles on the ground show the speed. Score: posts jumped,
// top right.

#define RUN_BIG     (ROWS >= 64)
#define RUN_W       (RUN_BIG ? 5 : 3)
#define RUN_H       (RUN_BIG ? 7 : 4)
#define RUN_X       (RUN_BIG ? 6 : 3)
#define RUN_GROUND  (ROWS - 3)              // Row of the track
#define RUN_JUMP    (RUN_BIG ? 1.25f : 0.85f)
#define RUN_FALL    (RUN_BIG ? 0.075f : 0.06f)
#define RUN_SPEED   (RUN_BIG ? 0.7f : 0.45f)
#define RUN_FASTER  0.0004f                 // Speed added per frame
#define POSTS       4
#define PEBBLES     (COLS / 6)

static const uint8_t bigrunner[2][7] = {
  { 0b01100, 0b01100, 0b11110, 0b01101, 0b00100, 0b01010, 0b10001 },
  { 0b01100, 0b01100, 0b01111, 0b10100, 0b00100, 0b00110, 0b00100 },
};
static const uint8_t smallrunner[2][4] = {
  { 0b010, 0b111, 0b010, 0b101 },
  { 0b010, 0b111, 0b010, 0b010 },
};

struct Post {
  float x;
  uint8_t w, h;
  bool passed;
};

static Post posts[POSTS];
static float pebbles[PEBBLES];
static float y, vy, speed;
static uint16_t score, frame;

static float airtime() {
  return 2 * RUN_JUMP / RUN_FALL;
}

static void newpost(Post &p, float after) {
  float gap = speed * airtime() * 1.2f + random(0, COLS * 2 / 3);
  p = { after + gap, (uint8_t)(RUN_BIG ? random(2, 4) : random(1, 3)),
        (uint8_t)(RUN_BIG ? random(3, 8) : random(2, 5)), false };
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  drawline(0, RUN_GROUND, COLS - 1, RUN_GROUND, true);
  for (float &p : pebbles) setdot((int16_t)p, RUN_GROUND + 2, true);
  for (Post &p : posts) {
    int16_t px = roundf(p.x);
    for (int16_t c = px; c < px + p.w; c++) drawline(c, RUN_GROUND - p.h, c, RUN_GROUND - 1, true);
  }
  bool ground = vy == 0;
  uint8_t legs = ground ? (frame / 4) % 2 : 0;
  int16_t top = roundf(y) - RUN_H;
  for (uint8_t r = 0; r < RUN_H; r++) {
    uint8_t bits = RUN_BIG ? bigrunner[legs][r] : smallrunner[legs][r];
    for (uint8_t c = 0; c < RUN_W; c++) {
      if (bits & (1 << c)) setdot(RUN_X + c, top + r, true);
    }
  }
  gamedigits(score, COLS - 1, 0, 1, true);
}

static void runnersetup() {
  y = RUN_GROUND;
  vy = 0;
  speed = RUN_SPEED;
  score = 0;
  frame = 0;
  float x = COLS;
  for (Post &p : posts) {
    newpost(p, x);
    x = p.x;
  }
  for (float &p : pebbles) p = random(COLS);
  draw();
}

static void runnerstep() {
  frame++;
  for (uint8_t b; (b = gamepress(0));) {
    if ((b == BUTTON_A || b == BUTTON_UP || b == BUTTON_B) && vy == 0 && y >= RUN_GROUND) vy = -RUN_JUMP;
  }
  if (vy != 0 || y < RUN_GROUND) {
    vy += RUN_FALL;
    y += vy;
    if (y >= RUN_GROUND) {
      y = RUN_GROUND;
      vy = 0;
    }
  }
  speed += RUN_FASTER;

  float last = 0;
  for (Post &p : posts) last = max(last, p.x);
  int16_t feet = roundf(y), top = feet - RUN_H;
  bool hit = false;
  for (Post &p : posts) {
    p.x -= speed;
    if (p.x < -p.w) {
      newpost(p, last);
      last = p.x;
      continue;
    }
    int16_t px = roundf(p.x);
    // The runner's middle columns count, the outer dots of arms and legs may brush a post
    if (RUN_X + RUN_W - 1 > px && RUN_X + 1 < px + p.w && feet > RUN_GROUND - p.h && top < RUN_GROUND) hit = true;
    if (!p.passed && px + p.w < RUN_X) {
      p.passed = true;
      score++;
    }
  }
  for (float &p : pebbles) {
    p -= speed;
    if (p < 0) p += COLS;
  }
  draw();
  if (hit) gameover(score);
}

const Game runnerGame = { "runner", "Hindernisloop", 1, runnersetup, runnerstep, 30, nullptr, true };
