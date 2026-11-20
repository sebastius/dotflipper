#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Flock (boids): every bird steers towards its neighbours, flies along with them
// and keeps a little distance. The screen wraps around.

#define BOIDS          16
#define BOID_SIGHT     8.0f
#define BOID_SPACE     2.5f
#define BOID_MIN_SPEED 0.3f
#define BOID_MAX_SPEED 0.8f

struct Boid {
  float x, y, vx, vy;
};

static Boid boids[BOIDS];

// Shortest distance on a wrapping screen
static float wrapped(float d, float size) {
  if (d > size / 2) return d - size;
  if (d < -size / 2) return d + size;
  return d;
}

static void boidssetup() {
  for (uint8_t i = 0; i < BOIDS; i++) {
    boids[i].x = random(COLS);
    boids[i].y = random(ROWS);
    boids[i].vx = random(-50, 51) / 100.0f;
    boids[i].vy = random(-50, 51) / 100.0f;
  }
}

static void boidsstep() {
  float nvx[BOIDS], nvy[BOIDS];
  for (uint8_t i = 0; i < BOIDS; i++) {
    Boid &b = boids[i];
    float cx = 0, cy = 0, ax = 0, ay = 0, sx = 0, sy = 0;
    uint8_t count = 0;
    for (uint8_t j = 0; j < BOIDS; j++) {
      if (i == j) continue;
      float dx = wrapped(boids[j].x - b.x, COLS), dy = wrapped(boids[j].y - b.y, ROWS);
      float d = sqrtf(dx * dx + dy * dy);
      if (d < BOID_SIGHT) {
        cx += dx;
        cy += dy;
        ax += boids[j].vx;
        ay += boids[j].vy;
        count++;
        if (d < BOID_SPACE) {
          sx -= dx;
          sy -= dy;
        }
      }
    }
    nvx[i] = b.vx + random(-5, 6) / 200.0f;
    nvy[i] = b.vy + random(-5, 6) / 200.0f;
    if (count) {
      nvx[i] += cx / count * 0.01f + (ax / count - b.vx) * 0.05f + sx * 0.05f;
      nvy[i] += cy / count * 0.01f + (ay / count - b.vy) * 0.05f + sy * 0.05f;
    }
    float speed = sqrtf(nvx[i] * nvx[i] + nvy[i] * nvy[i]);
    if (speed > BOID_MAX_SPEED || speed < BOID_MIN_SPEED) {
      float target = speed > BOID_MAX_SPEED ? BOID_MAX_SPEED : BOID_MIN_SPEED;
      if (speed < 0.01f) {
        nvx[i] = target;
        nvy[i] = 0;
      } else {
        nvx[i] *= target / speed;
        nvy[i] *= target / speed;
      }
    }
  }

  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 0; i < BOIDS; i++) {
    Boid &b = boids[i];
    b.vx = nvx[i];
    b.vy = nvy[i];
    b.x = fmodf(b.x + b.vx + COLS, COLS);
    b.y = fmodf(b.y + b.vy + ROWS, ROWS);
    setdot((int16_t)b.x, (int16_t)b.y, true);
  }
}

const Animation boidsAnimation = { "boids", "Zwerm", boidssetup, boidsstep, 40 };
