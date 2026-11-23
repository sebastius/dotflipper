#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Tetris: a well of 10 wide, the next piece and the number of lines on the right.
// Left and right move (hold to repeat), up or A turns clockwise, B turns back, down drops faster.
// Every 10 lines it gets faster. Score: lines.

#define TET_CELL    (ROWS >= 64 ? 3 : 1)                  // Dots per cell
#define TET_W       10
#define TET_H       min(24, (ROWS - 1) / TET_CELL)
#define TET_LEFT    1                                     // First dot inside the left wall
#define TET_TOP     (ROWS - 1 - TET_H * TET_CELL)         // First dot of the well
#define TET_SIDE    (TET_LEFT + TET_W * TET_CELL + 3)     // Next piece and lines
#define TET_PREVIEW (TET_CELL > 1 ? TET_CELL : 2)         // Dots per cell of the next piece
#define TET_DELAY   6                                     // Frames before left and right repeat
#define TET_REPEAT  2                                     // Frames between repeats
#define TET_FLASH   12                                    // Frames that full lines blink

// The seven pieces in their box (2, 3 or 4 wide), as rows of bits, bit 0 is the left column
struct Shape {
  uint8_t size;
  uint8_t rows[4];
};
static const Shape shapes[7] = {
  { 4, { 0b0000, 0b1111, 0b0000, 0b0000 } },  // I
  { 2, { 0b11, 0b11 } },                      // O
  { 3, { 0b010, 0b111, 0b000 } },             // T
  { 3, { 0b110, 0b011, 0b000 } },             // S
  { 3, { 0b011, 0b110, 0b000 } },             // Z
  { 3, { 0b001, 0b111, 0b000 } },             // J
  { 3, { 0b100, 0b111, 0b000 } },             // L
};

static bool well[TET_W][24];
static int8_t piece, rotation, px, py, nextpiece;
static uint8_t bag[7], baggot;
static uint16_t lines;
static uint8_t fall, repeat, flashing;
static int8_t repeatdir;
static bool full[24];

// Cell (cx, cy) of the box of piece p in rotation r, turned clockwise r times
static bool shapecell(int8_t p, int8_t r, int8_t cx, int8_t cy) {
  uint8_t n = shapes[p].size;
  for (int8_t i = 0; i < r; i++) {  // Turning back: the cell that ends up at (cx, cy)
    int8_t x = cy, y = n - 1 - cx;
    cx = x;
    cy = y;
  }
  return shapes[p].rows[cy] & (1 << cx);
}

static bool fits(int8_t p, int8_t r, int8_t x, int8_t y) {
  uint8_t n = shapes[p].size;
  for (int8_t cy = 0; cy < n; cy++) {
    for (int8_t cx = 0; cx < n; cx++) {
      if (!shapecell(p, r, cx, cy)) continue;
      int8_t wx = x + cx, wy = y + cy;
      if (wx < 0 || wx >= TET_W || wy >= TET_H) return false;
      if (wy >= 0 && well[wx][wy]) return false;
    }
  }
  return true;
}

// The next piece from a shuffled bag of all seven, so a piece never stays away long
static int8_t draw7() {
  if (baggot == 0) {
    for (uint8_t i = 0; i < 7; i++) bag[i] = i;
    for (uint8_t i = 6; i > 0; i--) {
      uint8_t j = random(i + 1), t = bag[i];
      bag[i] = bag[j];
      bag[j] = t;
    }
    baggot = 7;
  }
  return bag[--baggot];
}

static bool spawn() {
  piece = nextpiece;
  nextpiece = draw7();
  rotation = 0;
  px = (TET_W - shapes[piece].size) / 2;
  py = piece == 0 ? -1 : 0;
  fall = 0;
  return fits(piece, rotation, px, py);
}

static void block(int16_t x, int16_t y, uint8_t size) {
  for (uint8_t i = 0; i < size; i++) {
    for (uint8_t j = 0; j < size; j++) {
      setdot(x + i, y + j, true);
    }
  }
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  int16_t bottom = TET_TOP + TET_H * TET_CELL;
  drawline(TET_LEFT - 1, TET_TOP, TET_LEFT - 1, bottom, true);
  drawline(TET_LEFT + TET_W * TET_CELL, TET_TOP, TET_LEFT + TET_W * TET_CELL, bottom, true);
  drawline(TET_LEFT - 1, bottom, TET_LEFT + TET_W * TET_CELL, bottom, true);

  for (int8_t y = 0; y < TET_H; y++) {
    if (full[y] && (flashing / 3) % 2 == 1) continue;  // Full lines blink
    for (int8_t x = 0; x < TET_W; x++) {
      if (well[x][y]) block(TET_LEFT + x * TET_CELL, TET_TOP + y * TET_CELL, TET_CELL);
    }
  }
  if (!flashing) {
    uint8_t n = shapes[piece].size;
    for (int8_t cy = 0; cy < n; cy++) {
      for (int8_t cx = 0; cx < n; cx++) {
        if (shapecell(piece, rotation, cx, cy) && py + cy >= 0) {
          block(TET_LEFT + (px + cx) * TET_CELL, TET_TOP + (py + cy) * TET_CELL, TET_CELL);
        }
      }
    }
  }

  // Next piece, and the lines under it
  uint8_t n = shapes[nextpiece].size;
  for (int8_t cy = 0; cy < n; cy++) {
    for (int8_t cx = 0; cx < n; cx++) {
      if (shapecell(nextpiece, 0, cx, cy)) {
        block(TET_SIDE + cx * TET_PREVIEW, TET_TOP + 1 + cy * TET_PREVIEW, TET_PREVIEW);
      }
    }
  }
  gamedigits(lines, COLS - 1, TET_TOP + 1 + 4 * TET_PREVIEW + 2, TET_CELL > 1 ? 2 : 1, true);
}

static void tetrissetup() {
  memset(well, 0, sizeof(well));
  memset(full, 0, sizeof(full));
  lines = 0;
  baggot = 0;
  flashing = 0;
  repeatdir = 0;
  nextpiece = draw7();
  spawn();
  draw();
}

static void move(int8_t dx) {
  if (fits(piece, rotation, px + dx, py)) px += dx;
}

static void turn(int8_t dr) {
  int8_t r = (rotation + dr + 4) % 4;
  static const int8_t kicks[][2] = { { 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, -1 }, { -2, 0 }, { 2, 0 } };
  for (auto &k : kicks) {
    if (fits(piece, r, px + k[0], py + k[1])) {
      rotation = r;
      px += k[0];
      py += k[1];
      return;
    }
  }
}

// The piece lands: into the well, then full lines blink, or the next piece
static void land() {
  uint8_t n = shapes[piece].size;
  for (int8_t cy = 0; cy < n; cy++) {
    for (int8_t cx = 0; cx < n; cx++) {
      if (shapecell(piece, rotation, cx, cy) && py + cy >= 0) well[px + cx][py + cy] = true;
    }
  }
  bool any = false;
  for (int8_t y = 0; y < TET_H; y++) {
    full[y] = true;
    for (int8_t x = 0; x < TET_W; x++) full[y] = full[y] && well[x][y];
    any |= full[y];
  }
  if (any) {
    flashing = TET_FLASH;
  } else if (!spawn()) {
    gameover(lines);
  }
  draw();
}

static void removelines() {
  for (int8_t y = 0; y < TET_H; y++) {
    if (!full[y]) continue;
    for (int8_t row = y; row > 0; row--) {
      for (int8_t x = 0; x < TET_W; x++) well[x][row] = well[x][row - 1];
    }
    for (int8_t x = 0; x < TET_W; x++) well[x][0] = false;
    full[y] = false;
    lines++;
  }
}

static void tetrisstep() {
  if (flashing) {
    while (gamepress(0)) {}
    if (--flashing == 0) {
      removelines();
      if (!spawn()) {
        gameover(lines);
      }
    }
    draw();
    return;
  }

  for (uint8_t b; (b = gamepress(0));) {
    if (b == BUTTON_LEFT || b == BUTTON_RIGHT) {
      repeatdir = b == BUTTON_LEFT ? -1 : 1;
      repeat = TET_DELAY;
      move(repeatdir);
    } else if (b == BUTTON_UP || b == BUTTON_A) {
      turn(1);
    } else if (b == BUTTON_B) {
      turn(-1);
    }
  }
  uint8_t held = gameheld(0);
  if (repeatdir && (held & (repeatdir < 0 ? BUTTON_LEFT : BUTTON_RIGHT))) {
    if (--repeat == 0) {
      move(repeatdir);
      repeat = TET_REPEAT;
    }
  } else {
    repeatdir = 0;
  }

  uint8_t level = lines / 10;
  uint8_t interval = max(2, 20 - 2 * level);  // Frames per row: 0.6 s at the start
  if (++fall >= interval || (held & BUTTON_DOWN)) {
    fall = 0;
    if (fits(piece, rotation, px, py + 1)) {
      py++;
    } else {
      land();
      return;  // land() has drawn the screen if the game is over
    }
  }
  draw();
}

const Game tetrisGame = { "tetris", "Tetris", 1, tetrissetup, tetrisstep, 30 };
