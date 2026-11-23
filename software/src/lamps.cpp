#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Lamps: a 5x5 board of lamps. Pressing a lamp switches it and its four neighbours; get them all off.
// Move the blinking frame with the arrows, A presses, B puts the level back to its start. Tapping a lamp
// on the phone presses it too. Every level starts with its number and needs a few presses more.
// The puzzles are made by pressing from an empty board, so they can always be solved. Score: levels.

#define LP_N     5
#define LP_CELL  (ROWS >= 64 ? 11 : 5)
#define LP_GAP   (ROWS >= 64 ? 2 : 1)
#define LP_LEFT  ((COLS - LP_N * LP_CELL - (LP_N - 1) * LP_GAP) / 2)
#define LP_TOP   ((ROWS - LP_N * LP_CELL - (LP_N - 1) * LP_GAP) / 2)

static bool lamps[LP_N][LP_N], start[LP_N][LP_N];
static int8_t cx, cy;
static uint16_t level;
static uint8_t showlevel, solved, frame;
static int8_t lasttx = -1, lastty = -1;
static unsigned long lasttouch;

static void press(int8_t x, int8_t y) {
  static const int8_t d[5][2] = { { 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
  for (auto &o : d) {
    int8_t nx = x + o[0], ny = y + o[1];
    if (nx >= 0 && nx < LP_N && ny >= 0 && ny < LP_N) lamps[nx][ny] = !lamps[nx][ny];
  }
}

static bool alloff() {
  for (uint8_t x = 0; x < LP_N; x++)
    for (uint8_t y = 0; y < LP_N; y++)
      if (lamps[x][y]) return false;
  return true;
}

static void newlevel() {
  do {
    memset(lamps, 0, sizeof(lamps));
    uint8_t presses = min(3 + (int)level, 15);
    bool used[LP_N][LP_N] = {};
    for (uint8_t i = 0; i < presses; i++) {
      int8_t x, y;
      do {
        x = random(LP_N);
        y = random(LP_N);
      } while (used[x][y]);
      used[x][y] = true;
      press(x, y);
    }
  } while (alloff());
  memcpy(start, lamps, sizeof(lamps));
  showlevel = 12;
}

static int16_t cellx(int8_t x) { return LP_LEFT + x * (LP_CELL + LP_GAP); }
static int16_t celly(int8_t y) { return LP_TOP + y * (LP_CELL + LP_GAP); }

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  if (showlevel > 0) {
    uint8_t scale = ROWS >= 64 ? 4 : 2;
    gamedigits(level + 1, (COLS - gamedigitswidth(level + 1, scale)) / 2, (ROWS - 5 * scale) / 2, scale, false);
    return;
  }
  for (int8_t x = 0; x < LP_N; x++) {
    for (int8_t y = 0; y < LP_N; y++) {
      bool on = lamps[x][y] || (solved > 0 && (solved / 3) % 2 == 0);
      if (on) {
        for (int16_t i = 0; i < LP_CELL; i++)
          for (int16_t j = 0; j < LP_CELL; j++) setdot(cellx(x) + i, celly(y) + j, true);
      } else {
        setdot(cellx(x) + LP_CELL / 2, celly(y) + LP_CELL / 2, true);  // An off lamp is a dot
      }
    }
  }
  if (solved == 0 && (frame / 3) % 2 == 0) {  // The cursor: a frame in the gap around the lamp
    int16_t x0 = cellx(cx) - 1, y0 = celly(cy) - 1, x1 = cellx(cx) + LP_CELL, y1 = celly(cy) + LP_CELL;
    drawline(x0, y0, x1, y0, true);
    drawline(x0, y1, x1, y1, true);
    drawline(x0, y0, x0, y1, true);
    drawline(x1, y0, x1, y1, true);
  }
}

static void lampssetup() {
  level = 0;
  cx = cy = LP_N / 2;
  solved = 0;
  newlevel();
  draw();
}

static void lampsstep() {
  frame++;
  if (showlevel > 0) {
    showlevel--;
    while (gamepress(0)) {}
  } else if (solved > 0) {
    while (gamepress(0)) {}
    if (--solved == 0) {
      level++;
      newlevel();
    }
  } else {
    for (uint8_t b; (b = gamepress(0));) {
      if (b == BUTTON_LEFT && cx > 0) cx--;
      if (b == BUTTON_RIGHT && cx < LP_N - 1) cx++;
      if (b == BUTTON_UP && cy > 0) cy--;
      if (b == BUTTON_DOWN && cy < LP_N - 1) cy++;
      if (b == BUTTON_A) press(cx, cy);
      if (b == BUTTON_B) memcpy(lamps, start, sizeof(lamps));
      if (alloff()) {
        solved = 18;
        break;
      }
    }
  }
  draw();
}

// A tap on the phone: press the lamp under it, once per lamp per swipe
static void lampstouch(int16_t x, int16_t y, bool) {
  if (showlevel > 0 || solved > 0) return;
  int8_t tx = (x - LP_LEFT) / (LP_CELL + LP_GAP), ty = (y - LP_TOP) / (LP_CELL + LP_GAP);
  if (x < LP_LEFT || y < LP_TOP || tx >= LP_N || ty >= LP_N) return;
  if (tx == lasttx && ty == lastty && millis() - lasttouch < 400) {
    lasttouch = millis();
    return;
  }
  lasttx = tx;
  lastty = ty;
  lasttouch = millis();
  cx = tx;
  cy = ty;
  press(tx, ty);
  if (alloff()) solved = 18;
}

const Game lampsGame = { "lamps", "Lampjes", 1, lampssetup, lampsstep, 120, lampstouch, true };
