#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Moon lander: land softly on the flat pad between the two posts. Up fires the main engine, left and
// right the side engines; they all use fuel, the bar at the top. Every landing gives a new landscape
// with less fuel and a narrower pad. Score: landings, plus a point for every 10% fuel left.

#define LAND_BIG   (ROWS >= 64)
#define LAND_W     (LAND_BIG ? 5 : 3)
#define LAND_H     (LAND_BIG ? 4 : 3)
#define GRAVITY    (LAND_BIG ? 0.010f : 0.007f)
#define MAIN       (GRAVITY * 2.6f)
#define SIDE       (GRAVITY * 1.2f)
#define SAFE_VY    0.35f      // Fastest landing that holds, in dots per frame
#define SAFE_VX    0.25f
#define FUEL_FULL  400        // Frames of main engine

static const uint8_t bigship[4] = { 0b01110, 0b11111, 0b01010, 0b10001 };
static const uint8_t smallship[3] = { 0b010, 0b111, 0b101 };

static uint8_t ground[COLS];  // Top of the ground per column
static int16_t padx, padw;
static float x, y, vx, vy;
static uint16_t fuel, fuelstart;
static uint16_t score, level;
static uint8_t landed, frame;
static bool mainon, lefton, righton;

static void newlandscape() {
  padw = max(LAND_W + 1, (LAND_BIG ? 12 : 8) - (int)level);
  padx = random(2, COLS - padw - 2);
  int16_t h = random(ROWS * 6 / 10, ROWS - 3);
  for (int16_t c = 0; c < COLS; c++) {
    if (c >= padx && c < padx + padw) {
      if (c == padx) h = random(ROWS * 6 / 10, ROWS - 3);
    } else {
      h += random(-2, 3);
      h = constrain(h, ROWS / 2, ROWS - 2);
    }
    ground[c] = h;
  }
  fuelstart = max(120, FUEL_FULL - 30 * (int)level);
  fuel = fuelstart;
  x = random(2, COLS - LAND_W - 2);
  y = 3;
  vx = random(-20, 21) / 100.0f;
  vy = 0;
  landed = 0;
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (int16_t c = 0; c < COLS; c++) {
    drawline(c, ground[c], c, ROWS - 1, true);
  }
  setdot(padx - 1, ground[padx] - 1, true);  // Posts on both sides of the pad
  setdot(padx + padw, ground[padx] - 1, true);

  int16_t bar = (int32_t)fuel * (COLS / 2) / FUEL_FULL;
  if (bar > 0) drawline(0, 0, bar - 1, 0, true);

  if (landed == 0 || (landed / 4) % 2 == 0) {
    int16_t lx = roundf(x), ly = roundf(y);
    for (uint8_t r = 0; r < LAND_H; r++) {
      uint8_t bits = LAND_BIG ? bigship[r] : smallship[r];
      for (uint8_t c = 0; c < LAND_W; c++) {
        if (bits & (1 << c)) setdot(lx + c, ly + r, true);
      }
    }
    if (frame % 2) {  // Flames
      if (mainon) setdot(lx + LAND_W / 2, ly + LAND_H, true);
      if (lefton) setdot(lx + LAND_W, ly + 1, true);
      if (righton) setdot(lx - 1, ly + 1, true);
    }
  }
}

static void landersetup() {
  score = 0;
  level = 0;
  newlandscape();
  draw();
}

static void landerstep() {
  frame++;
  while (gamepress(0)) {}
  uint8_t held = gameheld(0);

  if (landed > 0) {  // Landed: the ship blinks, then the next landscape
    if (--landed == 0) newlandscape();
    draw();
    return;
  }

  mainon = fuel > 0 && (held & (BUTTON_UP | BUTTON_A));
  righton = fuel > 0 && (held & BUTTON_LEFT);  // The engine on the right pushes left
  lefton = fuel > 0 && (held & BUTTON_RIGHT);
  vy += GRAVITY;
  if (mainon) {
    vy -= MAIN;
    fuel--;
  }
  if (righton) vx -= SIDE;
  if (lefton) vx += SIDE;
  if ((righton || lefton) && fuel > 0 && frame % 2) fuel--;

  x += vx;
  y += vy;
  if (x < 0) { x = 0; vx = 0; }
  if (x > COLS - LAND_W) { x = COLS - LAND_W; vx = 0; }
  if (y < 1) { y = 1; vy = max(vy, 0.0f); }

  // Touching the ground?
  int16_t lx = roundf(x), feet = roundf(y) + LAND_H - 1;
  bool touch = false;
  for (int16_t c = lx; c < lx + LAND_W; c++) {
    touch |= feet >= ground[c] - 1;
  }
  if (touch) {
    bool onpad = lx >= padx && lx + LAND_W <= padx + padw;
    if (onpad && vy <= SAFE_VY && fabsf(vx) <= SAFE_VX) {
      y = ground[padx] - LAND_H;
      score += 1 + fuel * 10 / fuelstart;
      level++;
      landed = 40;
      mainon = lefton = righton = false;
    } else {
      draw();
      gameover(score);
      return;
    }
  }
  draw();
}

const Game landerGame = { "lander", "Maanlander", 1, landersetup, landerstep, 50, nullptr, true };
