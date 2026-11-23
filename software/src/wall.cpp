#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Wall breaker: keep the ball up with the bat and knock out the bricks. Three balls, shown top left.
// A cleared wall comes back, the ball a bit faster. Score: bricks knocked out.

#define BRICK_W     (COLS / 8)             // A brick and the gap next to it
#define BRICK_H     (ROWS >= 64 ? 3 : 2)
#define BRICK_ROWS  (ROWS >= 64 ? 6 : 5)
#define BRICK_TOP   (ROWS >= 64 ? 8 : 4)
#define BRICK_COLS  (COLS / BRICK_W)
#define BALL        (ROWS >= 64 ? 2 : 1)
#define BAT_W       (COLS / 5)
#define BAT_Y       (ROWS - 2)            // Row of the bat
#define BALLS       3
#define WAIT        30                     // Frames with the ball on the bat

static bool bricks[BRICK_COLS][BRICK_ROWS];
static uint16_t left, score, walls;
static float batx, bx, by, vx, vy, speed;
static uint8_t balls, wait;

static void newwall() {
  memset(bricks, true, sizeof(bricks));
  left = BRICK_COLS * BRICK_ROWS;
}

static void onbat() {
  wait = WAIT;
  vx = vy = 0;
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t c = 0; c < BRICK_COLS; c++) {
    for (uint8_t r = 0; r < BRICK_ROWS; r++) {
      if (bricks[c][r]) {  // The last column and row of a cell are the gap
        for (int16_t x = 0; x < BRICK_W - 1; x++) {
          for (int16_t y = 0; y < BRICK_H - 1; y++) {
            setdot(c * BRICK_W + x, BRICK_TOP + r * BRICK_H + y, true);
          }
        }
      }
    }
  }
  for (uint8_t i = 1; i < balls; i++) {  // Balls in reserve
    setdot(2 * i - 2, 0, true);
  }
  for (int16_t x = 0; x < BAT_W; x++) {
    setdot((int16_t)batx + x, BAT_Y, true);
  }
  for (int16_t i = 0; i < BALL; i++) {
    for (int16_t j = 0; j < BALL; j++) {
      setdot((int16_t)bx + i, (int16_t)by + j, true);
    }
  }
}

static void wallsetup() {
  newwall();
  score = walls = 0;
  balls = BALLS;
  speed = ROWS >= 64 ? 0.7f : 0.5f;
  batx = (COLS - BAT_W) / 2;
  onbat();
  bx = batx + BAT_W / 2;
  by = BAT_Y - BALL;
  draw();
}

// The brick under a dot, -1 if none. The gaps count as brick, so the ball can't slip through them
static int16_t brickat(int16_t x, int16_t y) {
  if (y < BRICK_TOP || y >= BRICK_TOP + BRICK_ROWS * BRICK_H || x < 0 || x >= BRICK_COLS * BRICK_W) {
    return -1;
  }
  uint8_t c = x / BRICK_W, r = (y - BRICK_TOP) / BRICK_H;
  return bricks[c][r] ? c * BRICK_ROWS + r : -1;
}

// Knocks out the bricks under the ball at (x, y), true if there was one
static bool knock(float x, float y) {
  bool hit = false;
  for (int16_t i = 0; i < BALL; i++) {
    for (int16_t j = 0; j < BALL; j++) {
      int16_t b = brickat((int16_t)x + i, (int16_t)y + j);
      if (b >= 0) {
        bricks[b / BRICK_ROWS][b % BRICK_ROWS] = false;
        left--;
        score++;
        hit = true;
      }
    }
  }
  return hit;
}

static void launch() {
  float angle = random(-50, 51) / 100.0f;
  vx = sinf(angle) * speed;
  vy = -cosf(angle) * speed;
}

static void wallstep() {
  // The bat
  float batspeed = ROWS >= 64 ? 2.0f : 1.0f;
  uint8_t pressed = 0;
  for (uint8_t b; (b = gamepress(0));) {
    pressed |= b;
  }
  uint8_t held = gameheld(0);
  if (held & BUTTON_LEFT) batx -= batspeed;
  if (held & BUTTON_RIGHT) batx += batspeed;
  batx = constrain(batx, 0.0f, (float)(COLS - BAT_W));

  if (wait > 0) {  // On the bat: launch after a while or with a button
    bx = batx + BAT_W / 2 - BALL / 2;
    by = BAT_Y - BALL;
    if (--wait == 0 || (pressed & (BUTTON_A | BUTTON_B | BUTTON_UP))) {
      wait = 0;
      launch();
    }
    draw();
    return;
  }

  // The ball, in steps of at most one dot, x and y apart so it knows which side it hit
  uint8_t steps = ceilf(max(fabsf(vx), fabsf(vy)));
  for (uint8_t s = 0; s < steps; s++) {
    float nx = bx + vx / steps;
    if (nx < 0 || nx > COLS - BALL) {
      vx = -vx;
    } else if (knock(nx, by)) {
      vx = -vx;
    } else {
      bx = nx;
    }
    float ny = by + vy / steps;
    if (ny < 0) {
      vy = -vy;
    } else if (knock(bx, ny)) {
      vy = -vy;
    } else if (vy > 0 && ny + BALL > BAT_Y && by + BALL <= BAT_Y + 0.01f &&
               bx + BALL > batx && bx < batx + BAT_W) {
      // On the bat: the further from the middle, the more sideways
      float offset = (bx + BALL / 2.0f - (batx + BAT_W / 2.0f)) / (BAT_W / 2.0f + BALL / 2.0f);
      float angle = constrain(offset, -1.0f, 1.0f) * 1.0f;
      vx = sinf(angle) * speed;
      vy = -cosf(angle) * speed;
    } else {
      by = ny;
    }
  }

  if (left == 0) {
    walls++;
    newwall();
    speed = min(speed * 1.15f, 1.5f);
    onbat();
  } else if (by > ROWS) {
    if (--balls == 0) {
      gameover(score);
      return;
    }
    onbat();
  }
  draw();
}

const Game wallGame = { "wall", "Muurbreker", 1, wallsetup, wallstep, 30, nullptr, true };
