#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Table tennis for four: a bat on every side, players 1 to 4 on left, right, top and bottom; the computer
// plays the sides nobody presses buttons for. A missed ball costs a life, after three a side becomes a
// wall. While the ball waits for the serve, the lives show as dots from the middle to each side.
// Up and left move a bat one way, down and right the other. Score: the place of player 1.

#define T4_LIVES  3
#define T4_BALL   (ROWS >= 64 ? 2 : 1)
#define T4_BAT    (min(COLS, ROWS) / 5)
#define T4_SPEED  0.6f
#define T4_MAX    1.4f
#define T4_SERVE  30

static float bat[4];       // Start of each bat along its side
static uint8_t lives[4], place[4];
static float bx, by, vx, vy;
static uint8_t wait;
static float aim[4];

static int16_t sidelength(uint8_t s) {
  return s < 2 ? ROWS : COLS;
}

static void serve() {
  bx = COLS / 2 - T4_BALL / 2.0f;
  by = ROWS / 2 - T4_BALL / 2.0f;
  float angle;
  do {  // Not too close to an axis (endless back and forth) or a diagonal (straight into a corner)
    angle = random(0, 628) / 100.0f;
  } while (fabsf(sinf(2 * angle)) < 0.3f || fabsf(sinf(2 * angle)) > 0.95f);
  vx = cosf(angle) * T4_SPEED;
  vy = sinf(angle) * T4_SPEED;
  wait = T4_SERVE;
  for (uint8_t s = 0; s < 4; s++) aim[s] = random(0, T4_BAT);
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t s = 0; s < 4; s++) {
    if (lives[s] == 0) {  // Out: a wall
      if (s == 0) drawline(0, 0, 0, ROWS - 1, true);
      if (s == 1) drawline(COLS - 1, 0, COLS - 1, ROWS - 1, true);
      if (s == 2) drawline(0, 0, COLS - 1, 0, true);
      if (s == 3) drawline(0, ROWS - 1, COLS - 1, ROWS - 1, true);
      continue;
    }
    for (int16_t i = 0; i < T4_BAT; i++) {
      for (int16_t w = 0; w < T4_BALL; w++) {
        int16_t along = (int16_t)bat[s] + i;
        if (s == 0) setdot(w, along, true);
        if (s == 1) setdot(COLS - 1 - w, along, true);
        if (s == 2) setdot(along, w, true);
        if (s == 3) setdot(along, ROWS - 1 - w, true);
      }
    }
    if (wait > 0) {
      for (uint8_t l = 0; l < lives[s]; l++) {
        int16_t d = 4 + 3 * l;
        int16_t cx = COLS / 2, cy = ROWS / 2;
        setdot(cx + (s == 0 ? -d : s == 1 ? d : 0), cy + (s == 2 ? -d : s == 3 ? d : 0), true);
      }
    }
  }
  if (wait == 0 || (wait / 4) % 2 == 0) {
    for (int16_t i = 0; i < T4_BALL; i++) {
      for (int16_t j = 0; j < T4_BALL; j++) {
        setdot((int16_t)bx + i, (int16_t)by + j, true);
      }
    }
  }
}

static void tennis4setup() {
  for (uint8_t s = 0; s < 4; s++) {
    lives[s] = T4_LIVES;
    place[s] = 0;
    bat[s] = (sidelength(s) - T4_BAT) / 2.0f;
  }
  serve();
  draw();
}

static void movebat(uint8_t s) {
  if (lives[s] == 0) return;
  float speed = ROWS >= 64 ? 1.5f : 1.0f;
  if (gamehuman(s)) {
    while (gamepress(s)) {}
    uint8_t held = gameheld(s);
    if (held & (BUTTON_UP | BUTTON_LEFT)) bat[s] -= speed;
    if (held & (BUTTON_DOWN | BUTTON_RIGHT)) bat[s] += speed;
  } else {
    bool coming = (s == 0 && vx < 0) || (s == 1 && vx > 0) || (s == 2 && vy < 0) || (s == 3 && vy > 0);
    float ball = s < 2 ? by : bx;
    float target = coming ? ball - aim[s] : (sidelength(s) - T4_BAT) / 2.0f;
    float step = speed * 0.7f;
    if (bat[s] < target - step) bat[s] += step;
    else if (bat[s] > target + step) bat[s] -= step;
  }
  bat[s] = constrain(bat[s], 0.0f, (float)(sidelength(s) - T4_BAT));
}

// Does the bat of side s meet the ball? Then it goes back, at an angle from where it hit
static bool hits(uint8_t s) {
  float ball = (s < 2 ? by : bx) + T4_BALL / 2.0f;
  float offset = ball - (bat[s] + T4_BAT / 2.0f);
  float half = T4_BAT / 2.0f + T4_BALL / 2.0f;
  if (fabsf(offset) > half) return false;
  float speed = min(T4_MAX, sqrtf(vx * vx + vy * vy) * 1.05f);
  float angle = offset / half;  // Up to about 57 degrees from straight back
  float back = cosf(angle) * speed, side = sinf(angle) * speed;
  if (s == 0) { vx = back; vy = side; }
  if (s == 1) { vx = -back; vy = side; }
  if (s == 2) { vy = back; vx = side; }
  if (s == 3) { vy = -back; vx = side; }
  aim[s] = random(0, T4_BAT);
  return true;
}

static uint8_t alive() {
  uint8_t n = 0;
  for (uint8_t s = 0; s < 4; s++) n += lives[s] > 0;
  return n;
}

static void miss(uint8_t s) {
  if (--lives[s] == 0) {
    place[s] = alive() + 1;
  }
  serve();
}

static void tennis4step() {
  for (uint8_t s = 0; s < 4; s++) movebat(s);

  if (wait > 0) {
    wait--;
  } else {
    float ox = bx, oy = by;
    bx += vx;
    by += vy;
    const float lo = T4_BALL, hix = COLS - 2 * T4_BALL, hiy = ROWS - 2 * T4_BALL;
    // A side that is out is a wall: the ball bounces off the edge
    if (vx < 0 && bx < 0 && lives[0] == 0) { bx = -bx; vx = -vx; }
    if (vx > 0 && bx > COLS - T4_BALL && lives[1] == 0) { bx = 2 * (COLS - T4_BALL) - bx; vx = -vx; }
    if (vy < 0 && by < 0 && lives[2] == 0) { by = -by; vy = -vy; }
    if (vy > 0 && by > ROWS - T4_BALL && lives[3] == 0) { by = 2 * (ROWS - T4_BALL) - by; vy = -vy; }

    if (lives[0] && vx < 0 && ox >= lo && bx < lo && hits(0)) bx = 2 * lo - bx;
    else if (lives[1] && vx > 0 && ox <= hix && bx > hix && hits(1)) bx = 2 * hix - bx;
    else if (lives[2] && vy < 0 && oy >= lo && by < lo && hits(2)) by = 2 * lo - by;
    else if (lives[3] && vy > 0 && oy <= hiy && by > hiy && hits(3)) by = 2 * hiy - by;
    else if (bx < -T4_BALL) miss(0);
    else if (bx > COLS) miss(1);
    else if (by < -T4_BALL) miss(2);
    else if (by > ROWS) miss(3);
  }

  draw();
  uint8_t left = alive(), humans = 0;
  for (uint8_t s = 0; s < 4; s++) humans += lives[s] > 0 && gamehuman(s);
  if (left <= 1 || humans == 0) {
    gameover(lives[0] > 0 ? left : place[0], false);
  }
}

const Game tennis4Game = { "tennis4", "Tafeltennis voor 4", 4, tennis4setup, tennis4step, 35, nullptr, true };
