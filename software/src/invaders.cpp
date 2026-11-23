#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Invaders: a block of aliens steps sideways and down, every step one click of the whole block and faster
// as fewer are left. Left and right move the gun, A or up fires (one shot at a time). On 64x64 there are
// four shelters that wear away. Three lives, shown top left. Score: aliens hit.

#define INV_BIG     (ROWS >= 64)
#define INV_W       (INV_BIG ? 5 : 3)   // Size of an alien
#define INV_H       (INV_BIG ? 4 : 2)
#define INV_COLS    (INV_BIG ? 8 : 6)
#define INV_ROWS    (INV_BIG ? 4 : 3)
#define INV_DX      (INV_W + 2)         // Distance between aliens
#define INV_DY      (INV_H + 2)
#define INV_DROP    (INV_BIG ? 2 : 1)
#define INV_GUN_W   INV_W
#define INV_GUN_Y   (ROWS - 2)          // Top row of the gun
#define INV_BOMBS   3
#define INV_LIVES   3
#define SHELTERS    4
#define SHELTER_W   6
#define SHELTER_H   3
#define SHELTER_Y   (ROWS - 10)

// Two frames per alien, rows of bits with bit 0 on the left
static const uint8_t bigalien[2][2][4] = {
  { { 0b01110, 0b10101, 0b11111, 0b10001 }, { 0b01110, 0b10101, 0b11111, 0b01010 } },
  { { 0b00100, 0b01110, 0b11011, 0b10001 }, { 0b00100, 0b01110, 0b11011, 0b01010 } },
};
static const uint8_t smallalien[2][2][2] = {
  { { 0b111, 0b101 }, { 0b111, 0b010 } },
  { { 0b010, 0b111 }, { 0b111, 0b101 } },
};

static bool alive[INV_COLS][INV_ROWS];
static uint8_t left, lives, frame2, wave;
static int16_t ox, oy;        // Top left of the block
static int8_t dir;
static uint16_t marchtimer;
static float gunx;
static int16_t shotx, shoty;  // shoty < 0: no shot
static int16_t bombx[INV_BOMBS], bomby[INV_BOMBS];
static bool shelter[SHELTERS][SHELTER_W][SHELTER_H];
static uint8_t hitwait, bombtick;
static uint16_t score;

static int16_t sheltex(uint8_t s) {
  return (COLS / SHELTERS) * s + (COLS / SHELTERS - SHELTER_W) / 2;
}

// Shelter dot at (x, y)? Then it wears away
static bool hitshelter(int16_t x, int16_t y) {
  if (!INV_BIG || y < SHELTER_Y || y >= SHELTER_Y + SHELTER_H) return false;
  for (uint8_t s = 0; s < SHELTERS; s++) {
    int16_t sx = x - sheltex(s);
    if (sx >= 0 && sx < SHELTER_W && shelter[s][sx][y - SHELTER_Y]) {
      shelter[s][sx][y - SHELTER_Y] = false;
      return true;
    }
  }
  return false;
}

static void newwave() {
  memset(alive, true, sizeof(alive));
  left = INV_COLS * INV_ROWS;
  ox = (COLS - (INV_COLS - 1) * INV_DX - INV_W) / 2;
  oy = (INV_BIG ? 4 : 3) + min((int)wave, 4) * INV_DROP;
  dir = 1;
  marchtimer = 0;
  for (uint8_t b = 0; b < INV_BOMBS; b++) bomby[b] = -1;
  shoty = -1;
}

static void draw() {
  memset(next_state, 0, sizeof(next_state));
  for (uint8_t i = 1; i < lives; i++) setdot(2 * i - 2, 0, true);
  for (uint8_t c = 0; c < INV_COLS; c++) {
    for (uint8_t r = 0; r < INV_ROWS; r++) {
      if (!alive[c][r]) continue;
      uint8_t kind = r == 0 ? 1 : 0;
      for (uint8_t y = 0; y < INV_H; y++) {
        uint8_t bits = INV_BIG ? bigalien[kind][frame2][y] : smallalien[kind][frame2][y];
        for (uint8_t x = 0; x < INV_W; x++) {
          if (bits & (1 << x)) setdot(ox + c * INV_DX + x, oy + r * INV_DY + y, true);
        }
      }
    }
  }
  if (INV_BIG) {
    for (uint8_t s = 0; s < SHELTERS; s++)
      for (uint8_t x = 0; x < SHELTER_W; x++)
        for (uint8_t y = 0; y < SHELTER_H; y++)
          if (shelter[s][x][y]) setdot(sheltex(s) + x, SHELTER_Y + y, true);
  }
  if (hitwait == 0 || (hitwait / 4) % 2 == 0) {  // The gun blinks after a hit
    int16_t gx = (int16_t)gunx;
    setdot(gx + INV_GUN_W / 2, INV_GUN_Y, true);
    for (int16_t x = 0; x < INV_GUN_W; x++) setdot(gx + x, INV_GUN_Y + 1, true);
  }
  if (shoty >= 0) {
    setdot(shotx, shoty, true);
    setdot(shotx, shoty + 1, true);
  }
  for (uint8_t b = 0; b < INV_BOMBS; b++) {
    if (bomby[b] >= 0) setdot(bombx[b], bomby[b], true);
  }
}

static void invaderssetup() {
  lives = INV_LIVES;
  wave = 0;
  score = 0;
  hitwait = 0;
  frame2 = 0;
  gunx = (COLS - INV_GUN_W) / 2;
  memset(shelter, true, sizeof(shelter));
  for (uint8_t s = 0; s < SHELTERS; s++) {  // An arch underneath
    shelter[s][2][SHELTER_H - 1] = shelter[s][3][SHELTER_H - 1] = false;
    shelter[s][0][0] = shelter[s][SHELTER_W - 1][0] = false;
  }
  newwave();
  draw();
}

static void march() {
  int16_t minx = COLS, maxx = -1, maxy = -1;
  for (uint8_t c = 0; c < INV_COLS; c++) {
    for (uint8_t r = 0; r < INV_ROWS; r++) {
      if (!alive[c][r]) continue;
      minx = min(minx, (int16_t)(ox + c * INV_DX));
      maxx = max(maxx, (int16_t)(ox + c * INV_DX + INV_W - 1));
      maxy = max(maxy, (int16_t)(oy + r * INV_DY + INV_H - 1));
    }
  }
  if ((dir > 0 && maxx >= COLS - 1) || (dir < 0 && minx <= 0)) {
    oy += INV_DROP;
    dir = -dir;
  } else {
    ox += dir;
  }
  frame2 ^= 1;
  // Aliens walk through the shelters
  if (INV_BIG) {
    for (uint8_t c = 0; c < INV_COLS; c++)
      for (uint8_t r = 0; r < INV_ROWS; r++)
        if (alive[c][r])
          for (uint8_t x = 0; x < INV_W; x++)
            for (uint8_t y = 0; y < INV_H; y++) hitshelter(ox + c * INV_DX + x, oy + r * INV_DY + y);
  }
}

static int16_t lowestalien(uint8_t c) {
  for (int8_t r = INV_ROWS - 1; r >= 0; r--) {
    if (alive[c][r]) return r;
  }
  return -1;
}

static bool reachedgun() {
  for (uint8_t c = 0; c < INV_COLS; c++) {
    int16_t r = lowestalien(c);
    if (r >= 0 && oy + r * INV_DY + INV_H > INV_GUN_Y) return true;
  }
  return false;
}

static void invadersstep() {
  bool fire = false;
  for (uint8_t b; (b = gamepress(0));) fire |= b == BUTTON_A || b == BUTTON_UP;
  uint8_t held = gameheld(0);

  if (hitwait > 0) {
    hitwait--;
    draw();
    return;
  }

  float speed = INV_BIG ? 1.0f : 0.5f;
  if (held & BUTTON_LEFT) gunx -= speed;
  if (held & BUTTON_RIGHT) gunx += speed;
  gunx = constrain(gunx, 0.0f, (float)(COLS - INV_GUN_W));
  if (fire && shoty < 0) {
    shotx = (int16_t)gunx + INV_GUN_W / 2;
    shoty = INV_GUN_Y - 2;
  }

  // The shot: one dot per frame
  if (shoty >= 0) {
    shoty--;
    if (shoty < 0 || hitshelter(shotx, shoty)) {
      shoty = -1;
    } else {
      for (uint8_t c = 0; c < INV_COLS && shoty >= 0; c++) {
        for (uint8_t r = 0; r < INV_ROWS; r++) {
          int16_t x = shotx - (ox + c * INV_DX), y = shoty - (oy + r * INV_DY);
          if (alive[c][r] && x >= 0 && x < INV_W && y >= -1 && y < INV_H) {
            alive[c][r] = false;
            left--;
            score++;
            shoty = -1;
            break;
          }
        }
      }
      for (uint8_t b = 0; b < INV_BOMBS && shoty >= 0; b++) {
        if (bomby[b] >= 0 && bombx[b] == shotx && abs(bomby[b] - shoty) <= 1) {
          bomby[b] = shoty = -1;
        }
      }
    }
  }

  // The block marches, faster with fewer aliens
  uint16_t interval = 1 + (uint32_t)left * 20 / (INV_COLS * INV_ROWS);
  if (++marchtimer >= interval) {
    marchtimer = 0;
    march();
  }

  // Bombs: one dot every other frame, from the lowest alien of a random column
  bombtick ^= 1;
  for (uint8_t b = 0; b < INV_BOMBS; b++) {
    if (bomby[b] < 0) {
      uint8_t c = random(INV_COLS);
      int16_t r = lowestalien(c);
      if (r >= 0 && random(60) == 0) {
        bombx[b] = ox + c * INV_DX + INV_W / 2;
        bomby[b] = oy + r * INV_DY + INV_H;
      }
      continue;
    }
    if (!bombtick) continue;
    bomby[b]++;
    if (bomby[b] >= ROWS || hitshelter(bombx[b], bomby[b])) {
      bomby[b] = -1;
    } else if (bomby[b] >= INV_GUN_Y && bombx[b] >= (int16_t)gunx && bombx[b] < (int16_t)gunx + INV_GUN_W) {
      bomby[b] = -1;
      if (--lives == 0) {
        draw();
        gameover(score);
        return;
      }
      hitwait = 40;
      for (uint8_t i = 0; i < INV_BOMBS; i++) bomby[i] = -1;
    }
  }

  if (left == 0) {
    wave++;
    newwave();
  } else if (reachedgun()) {
    draw();
    gameover(score);
    return;
  }
  draw();
}

const Game invadersGame = { "invaders", "Indringers", 1, invaderssetup, invadersstep, 30, nullptr, true };
