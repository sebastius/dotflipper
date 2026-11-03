#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Zooming into the Mandelbrot set (seahorse valley). The set itself is on,
// outside it the number of iterations gives bands of on and off.
// Restarts before float precision runs out.

static const float centerx = -0.743643887f;
static const float centery =  0.131825904f;
static float scale;  // Width of the screen in the complex plane

static void mandelbrotsetup() {
  scale = 3.0f;
}

static void mandelbrotstep() {
  float dotsize = scale / COLS;
  uint16_t maxiter = 40 + logf(3.0f / scale) * 12;

  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      float cr = centerx + (x - COLS / 2 + 0.5f) * dotsize;
      float ci = centery + (y - ROWS / 2 + 0.5f) * dotsize;
      float zr = 0, zi = 0;
      uint16_t i = 0;
      while (i < maxiter && zr * zr + zi * zi < 4.0f) {
        float t = zr * zr - zi * zi + cr;
        zi = 2 * zr * zi + ci;
        zr = t;
        i++;
      }
      next_state[x][y] = (i == maxiter) || ((i >> 1) & 1);
    }
  }

  scale *= 0.93f;
  if (scale < 1e-4f) {
    scale = 3.0f;
  }
}

const Animation mandelbrotAnimation = { "mandelbrot", "Mandelbrot", mandelbrotsetup, mandelbrotstep, 100 };
