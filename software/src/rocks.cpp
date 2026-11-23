#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Rocks: a small ship in a field of drifting rocks, everything wraps around the edges. Left and right
// turn, up gives thrust, A fires, B jumps to a random spot. A rock that is hit breaks in two smaller ones.
// Three lives, top left. A cleared field comes back with one rock more. Score: rocks hit.

#define ROCKS_BIG    (ROWS >= 64)
#define ROCKS_MAX    24
#define SHOTS        4
#define SHOT_SPEED   1.5f
#define SHOT_LIFE    (int)(min(COLS, ROWS) * 0.7f / SHOT_SPEED)
#define SHIP_LIVES   3
#define SHIP_SAFE    60      // Frames the ship can't be hit after it comes back

struct Rock {
  float x, y, vx, vy;
  uint8_t size;  // 0: gone, 1 small, 2 middle, 3 big
};
struct Shot {
  float x, y, vx, vy;
  uint8_t life;
};

static Rock rocks[ROCKS_MAX];
static Shot shots[SHOTS];
static float sx, sy, svx, svy, angle;
static uint8_t lives, safe, wave, frame;
static bool thrust;
static uint16_t score;

static float radius(uint8_t size) {
  static const float big[] = { 0, 1.5f, 3, 6 }, small[] = { 0, 1, 2, 4 };
  return ROCKS_BIG ? big[size] : small[size];
}

static float wrap(float v, float size) {
  while (v < 0) v += size;
  while (v >= size) v -= size;
  return v;
}

static void plot(float x, float y) {
  setdot((int16_t)wrap(roundf(x), COLS), (int16_t)wrap(roundf(y), ROWS), true);
}

static void line(float x0, float y0, float x1, float y1) {
  uint8_t n = max(fabsf(x1 - x0), fabsf(y1 - y0)) + 1;
  for (uint8_t i = 0; i <= n; i++) plot(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
}

// Shortest distance on a screen that wraps around
static float distance(float x0, float y0, float x1, float y1) {
  float dx = fabsf(x1 - x0), dy = fabsf(y1 - y0);
  dx = min(dx, COLS - dx);
  dy = min(dy, ROWS - dy);
  return sqrtf(dx * dx + dy * dy);
}

static void addrock(float x, float y, uint8_t size) {
  for (Rock &r : rocks) {
    if (r.size == 0) {
      float a = random(0, 628) / 100.0f, v = random(15, 40) / 100.0f * (ROCKS_BIG ? 1.0f : 0.7f);
      r = { x, y, cosf(a) * v, sinf(a) * v, size };
      return;
    }
  }
}

static void newfield() {
  for (Rock &r : rocks) r.size = 0;
  uint8_t count = (ROCKS_BIG ? 4 : 3) + wave;
  for (uint8_t i = 0; i < count && i < ROCKS_MAX / 4; i++) {
    float x, y;
    do {  // Not on top of the ship
      x = random(COLS);
      y = random(ROWS);
    } while (distance(x, y, sx, sy) < min(COLS, ROWS) / 3);
    addrock(x, y, 3);
  }
}

static void resetship() {
  sx = COLS / 2;
  sy = ROWS / 2;
  svx = svy = 0;
  angle = -M_PI / 2;
  safe = SHIP_SAFE;
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 1; i < lives; i++) setdot(2 * i - 2, 0, true);
  for (Rock &r : rocks) {
    if (r.size == 0) continue;
    int16_t rr = roundf(radius(r.size));
    for (int16_t ox = -COLS; ox <= COLS; ox += COLS) {  // Also the copy across the edge
      for (int16_t oy = -ROWS; oy <= ROWS; oy += ROWS) {
        int16_t cx = roundf(r.x) + ox, cy = roundf(r.y) + oy;
        if (cx + rr >= 0 && cx - rr < COLS && cy + rr >= 0 && cy - rr < ROWS) gamecircle(cx, cy, rr, true);
      }
    }
  }
  for (Shot &s : shots) {
    if (s.life) plot(s.x, s.y);
  }
  if (safe == 0 || (safe / 4) % 2 == 0) {  // Blinks while it can't be hit
    float nose = ROCKS_BIG ? 3.5f : 2.5f, back = ROCKS_BIG ? 2.5f : 1.8f;
    float nx = sx + cosf(angle) * nose, ny = sy + sinf(angle) * nose;
    float lx = sx + cosf(angle + 2.5f) * back, ly = sy + sinf(angle + 2.5f) * back;
    float rx = sx + cosf(angle - 2.5f) * back, ry = sy + sinf(angle - 2.5f) * back;
    line(nx, ny, lx, ly);
    line(nx, ny, rx, ry);
    if (ROCKS_BIG) line(lx, ly, rx, ry);
    if (thrust && frame % 2) plot(sx - cosf(angle) * (back + 1.5f), sy - sinf(angle) * (back + 1.5f));
  }
}

static void rockssetup() {
  lives = SHIP_LIVES;
  wave = 0;
  score = 0;
  for (Shot &s : shots) s.life = 0;
  resetship();
  newfield();
  draw();
}

static void rocksstep() {
  frame++;
  bool fire = false, jump = false;
  for (uint8_t b; (b = gamepress(0));) {
    fire |= b == BUTTON_A;
    jump |= b == BUTTON_B;
  }
  uint8_t held = gameheld(0);

  if (held & BUTTON_LEFT) angle -= 0.15f;
  if (held & BUTTON_RIGHT) angle += 0.15f;
  thrust = held & BUTTON_UP;
  if (thrust) {
    svx += cosf(angle) * 0.04f;
    svy += sinf(angle) * 0.04f;
  }
  svx *= 0.99f;
  svy *= 0.99f;
  float v = sqrtf(svx * svx + svy * svy);
  if (v > 1.2f) {
    svx *= 1.2f / v;
    svy *= 1.2f / v;
  }
  sx = wrap(sx + svx, COLS);
  sy = wrap(sy + svy, ROWS);
  if (jump) {
    sx = random(COLS);
    sy = random(ROWS);
    svx = svy = 0;
  }
  if (safe > 0) safe--;

  if (fire) {
    for (Shot &s : shots) {
      if (s.life == 0) {
        s = { sx + cosf(angle) * 2, sy + sinf(angle) * 2, cosf(angle) * SHOT_SPEED + svx,
              sinf(angle) * SHOT_SPEED + svy, (uint8_t)SHOT_LIFE };
        break;
      }
    }
  }

  for (Rock &r : rocks) {
    if (r.size == 0) continue;
    r.x = wrap(r.x + r.vx, COLS);
    r.y = wrap(r.y + r.vy, ROWS);
  }

  for (Shot &s : shots) {
    if (s.life == 0) continue;
    s.life--;
    s.x = wrap(s.x + s.vx, COLS);
    s.y = wrap(s.y + s.vy, ROWS);
    for (Rock &r : rocks) {
      if (r.size && distance(s.x, s.y, r.x, r.y) < radius(r.size) + 0.8f) {
        s.life = 0;
        score++;
        uint8_t size = r.size - 1;
        float x = r.x, y = r.y;
        r.size = 0;
        if (size > 0) {
          addrock(x, y, size);
          addrock(x, y, size);
        }
        break;
      }
    }
  }

  if (safe == 0) {
    for (Rock &r : rocks) {
      if (r.size && distance(sx, sy, r.x, r.y) < radius(r.size) + (ROCKS_BIG ? 2.0f : 1.2f)) {
        if (--lives == 0) {
          draw();
          gameover(score);
          return;
        }
        resetship();
        break;
      }
    }
  }

  bool any = false;
  for (Rock &r : rocks) any |= r.size > 0;
  if (!any) {
    wave++;
    newfield();
    safe = SHIP_SAFE;
  }
  draw();
}

const Game rocksGame = { "rocks", "Rotsen", 1, rockssetup, rocksstep, 35, nullptr, true };
