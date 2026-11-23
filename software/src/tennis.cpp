#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Table tennis: player 1 on the left, player 2 or the computer on the right. First to TENNIS_POINTS wins.
// The ball gets faster with every return; where it hits the bat sets the angle.
// Score: the points of player 1.

#define TENNIS_POINTS 7
#define TENNIS_BALL   (ROWS >= 64 ? 2 : 1)       // Size of the ball in dots
#define TENNIS_BAT    (ROWS / 5)                 // Length of a bat
#define TENNIS_SPEED  0.6f                       // Dots per frame at the serve
#define TENNIS_MAX    1.4f
#define TENNIS_SERVE  25                         // Frames before the serve

static float batpos[2];   // Top of the bats
static float bx, by, vx, vy;
static uint8_t points[2];
static uint8_t wait;
static float aim;         // Where the computer wants the ball on its bat, it doesn't always get it right

static void serve(int8_t dir) {
  bx = COLS / 2 - TENNIS_BALL / 2.0f;
  by = random(ROWS / 4, ROWS * 3 / 4);
  vx = TENNIS_SPEED * dir;
  vy = random(-40, 41) / 100.0f;
  wait = TENNIS_SERVE;
  aim = random(0, TENNIS_BAT);
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (int16_t y = 0; y < ROWS; y += 2) {  // The net
    setdot(COLS / 2, y, true);
  }
  uint8_t scale = ROWS >= 64 ? 2 : 1;
  gamedigits(points[0], COLS / 2 - 3, 1, scale, true);
  gamedigits(points[1], COLS / 2 + 3, 1, scale, false);
  for (uint8_t s = 0; s < 2; s++) {
    int16_t x = s == 0 ? 0 : COLS - TENNIS_BALL;
    for (int16_t y = 0; y < TENNIS_BAT; y++) {
      for (int16_t w = 0; w < TENNIS_BALL; w++) {
        setdot(x + w, (int16_t)batpos[s] + y, true);
      }
    }
  }
  if (wait == 0 || (wait / 4) % 2 == 0) {  // The ball blinks before the serve
    for (int16_t i = 0; i < TENNIS_BALL; i++) {
      for (int16_t j = 0; j < TENNIS_BALL; j++) {
        setdot((int16_t)bx + i, (int16_t)by + j, true);
      }
    }
  }
}

static void tennissetup() {
  points[0] = points[1] = 0;
  batpos[0] = batpos[1] = (ROWS - TENNIS_BAT) / 2;
  serve(random(2) ? 1 : -1);
  draw();
}

static void movebat(uint8_t s) {
  float speed = ROWS >= 64 ? 1.5f : 1.0f;
  if (gamehuman(s)) {
    while (gamepress(s)) {}  // Only holding counts
    uint8_t held = gameheld(s);
    if (held & BUTTON_UP) batpos[s] -= speed;
    if (held & BUTTON_DOWN) batpos[s] += speed;
  } else {
    // Follow the ball when it comes this way, else drift to the middle; a bit slower than a person
    float target = (vx > 0) == (s == 1) ? by - aim : (ROWS - TENNIS_BAT) / 2.0f;
    float step = speed * 0.7f;
    if (batpos[s] < target - step) batpos[s] += step;
    else if (batpos[s] > target + step) batpos[s] -= step;
  }
  batpos[s] = constrain(batpos[s], 0.0f, (float)(ROWS - TENNIS_BAT));
}

// The ball reaches the side of bat s: back, or a point for the other side
static bool hits(uint8_t s) {
  float offset = by + TENNIS_BALL / 2.0f - (batpos[s] + TENNIS_BAT / 2.0f);  // From the middle of the bat
  if (fabsf(offset) > TENNIS_BAT / 2.0f + TENNIS_BALL / 2.0f) {
    return false;
  }
  float speed = min(TENNIS_MAX, sqrtf(vx * vx + vy * vy) * 1.06f);
  float angle = offset / (TENNIS_BAT / 2.0f + TENNIS_BALL / 2.0f) * 1.0f;  // Up to about 57 degrees
  vx = cosf(angle) * speed * (s == 0 ? 1 : -1);
  vy = sinf(angle) * speed;
  aim = random(0, TENNIS_BAT);
  return true;
}

static void tennisstep() {
  movebat(0);
  movebat(1);

  if (wait > 0) {
    wait--;
  } else {
    float oldx = bx;
    bx += vx;
    by += vy;
    if (by < 0) {
      by = -by;
      vy = -vy;
    }
    if (by > ROWS - TENNIS_BALL) {
      by = 2 * (ROWS - TENNIS_BALL) - by;
      vy = -vy;
    }
    // A bat only hits when the ball passes its face, not when it moves in behind a ball that got past
    if (vx < 0 && oldx >= TENNIS_BALL && bx < TENNIS_BALL && hits(0)) {
      bx = 2 * TENNIS_BALL - bx;
    } else if (vx > 0 && oldx <= COLS - 2 * TENNIS_BALL && bx > COLS - 2 * TENNIS_BALL && hits(1)) {
      bx = 2 * (COLS - 2 * TENNIS_BALL) - bx;
    } else if (bx < 0) {
      points[1]++;
      serve(-1);
    } else if (bx > COLS - TENNIS_BALL) {
      points[0]++;
      serve(1);
    }
  }

  draw();
  if (points[0] >= TENNIS_POINTS || points[1] >= TENNIS_POINTS) {
    gameover(points[0]);
  }
}

const Game tennisGame = { "tennis", "Tafeltennis", 2, tennissetup, tennisstep, 35, nullptr, true };
