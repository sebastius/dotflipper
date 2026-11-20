#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Fireworks: a rocket goes up and bursts into sparks that fall down and fade out.

#define SPARKS          64
#define ROCKET_GRAVITY  0.03f
#define SPARK_GRAVITY   0.025f

struct Spark {
  float x, y, vx, vy;
  uint8_t life;  // Frames left, 0 is unused
};

static Spark rocket;
static Spark sparks[SPARKS];

static void explode() {
  uint8_t count = random(18, 31);
  for (uint8_t i = 0; i < SPARKS && count; i++) {
    if (sparks[i].life) continue;
    float angle = random(6283) / 1000.0f, speed = random(30, 101) / 100.0f;
    sparks[i] = { rocket.x, rocket.y, cosf(angle) * speed, sinf(angle) * speed, (uint8_t)random(18, 33) };
    count--;
  }
  rocket.life = 0;
}

static void fireworkssetup() {
  rocket.life = 0;
  for (uint8_t i = 0; i < SPARKS; i++) {
    sparks[i].life = 0;
  }
}

static void fireworksstep() {
  memset(next_state, 0, sizeof(next_state));

  if (!rocket.life && random(25) == 0) {
    rocket = { (float)random(6, COLS - 6), ROWS - 1, random(-20, 21) / 100.0f, -random(110, 146) / 100.0f, 1 };
  }
  if (rocket.life) {
    rocket.vy += ROCKET_GRAVITY;
    rocket.x += rocket.vx;
    rocket.y += rocket.vy;
    setdot(roundf(rocket.x), roundf(rocket.y), true);
    setdot(roundf(rocket.x - rocket.vx), roundf(rocket.y - rocket.vy), true);  // Tail
    if (rocket.vy >= -0.15f) {
      explode();
    }
  }

  for (uint8_t i = 0; i < SPARKS; i++) {
    Spark &s = sparks[i];
    if (!s.life) continue;
    s.vy += SPARK_GRAVITY;
    s.vx *= 0.98f;
    s.vy *= 0.98f;
    s.x += s.vx;
    s.y += s.vy;
    s.life--;
    if (s.life > 6 || random(2)) {  // Flicker out at the end
      setdot(roundf(s.x), roundf(s.y), true);
    }
  }
}

const Animation fireworksAnimation = { "fireworks", "Vuurwerk", fireworkssetup, fireworksstep, 40 };
