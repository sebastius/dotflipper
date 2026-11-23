#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "game.h"

// Trails: every rider leaves a wall behind. Whoever hits a wall or the edge is out and their
// wall disappears; the last one riding wins. With one colour the riders are told apart by their
// wall: rider 1 draws a solid line, the others dotted and dashed lines. The gaps are only for the
// eye, they are as solid as the rest. Riders without a person on the controller are the computer.
// Score: how far player 1 got.

#define TRAILS_RIDERS ((COLS >= 64 && ROWS >= 64) ? 4 : 2)
#define TRAILS_FAST   35     // Fastest frame time in ms
#define TRAILS_SPEEDUP 25    // Steps between speeding up by 1 ms
#define TRAILS_REACH  400    // How far the computer looks into the free space

struct Rider {
  int16_t x, y;
  int8_t dx, dy;
  bool alive;
  uint16_t steps;
};

static Rider riders[TRAILS_RIDERS];
static uint8_t owner[COLS][ROWS];  // 0 free, else rider + 1
static uint16_t stepcount;

// Which dots of the wall show, per rider: solid, dotted, dash dot, dashed
static bool visible(uint8_t rider, uint16_t step) {
  switch (rider) {
    case 0: return true;
    case 1: return step % 2 == 0;
    case 2: return step % 3 != 2;
    default: return step % 4 < 2;
  }
}

static bool isfree(int16_t x, int16_t y) {
  return x >= 0 && x < COLS && y >= 0 && y < ROWS && owner[x][y] == 0;
}

static void start(uint8_t r, int16_t x, int16_t y, int8_t dx, int8_t dy) {
  riders[r] = { x, y, dx, dy, true, 0 };
  owner[x][y] = r + 1;
  setdot(x, y, true);
}

static void trailssetup() {
  memset(owner, 0, sizeof(owner));
  start(0, COLS / 8, ROWS * 3 / 8, 1, 0);
  start(1, COLS * 7 / 8 - 1, ROWS * 5 / 8, -1, 0);
  if (TRAILS_RIDERS > 2) {
    start(2, COLS * 5 / 8, ROWS / 8, 0, 1);
    start(3, COLS * 3 / 8, ROWS * 7 / 8 - 1, 0, -1);
  }
  stepcount = 0;
}

// Free cells reachable from (x, y), up to TRAILS_REACH
static uint16_t space(int16_t x, int16_t y) {
  static uint8_t seen[COLS][ROWS];  // Visited in this search when equal to stamp
  static uint8_t stamp;
  static uint16_t queue[TRAILS_REACH];
  if (++stamp == 0) {
    memset(seen, 0, sizeof(seen));
    stamp = 1;
  }
  uint16_t head = 0, tail = 0;
  queue[tail++] = x * ROWS + y;
  seen[x][y] = stamp;
  while (head < tail && tail < TRAILS_REACH) {
    int16_t cx = queue[head] / ROWS, cy = queue[head] % ROWS;
    head++;
    static const int8_t d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (uint8_t i = 0; i < 4 && tail < TRAILS_REACH; i++) {
      int16_t nx = cx + d[i][0], ny = cy + d[i][1];
      if (isfree(nx, ny) && seen[nx][ny] != stamp) {
        seen[nx][ny] = stamp;
        queue[tail++] = nx * ROWS + ny;
      }
    }
  }
  return tail;
}

// The computer: the direction with the most room, rather straight on, and not next to another head
static void think(uint8_t r) {
  Rider &me = riders[r];
  const int8_t options[3][2] = { { me.dx, me.dy }, { me.dy, (int8_t)-me.dx }, { (int8_t)-me.dy, me.dx } };
  int32_t bestscore = -1;
  for (uint8_t o = 0; o < 3; o++) {
    int16_t nx = me.x + options[o][0], ny = me.y + options[o][1];
    if (!isfree(nx, ny)) {
      continue;
    }
    int32_t score = space(nx, ny) * 8 + random(6);
    if (o == 0) {
      score += 4;
    }
    for (uint8_t other = 0; other < TRAILS_RIDERS; other++) {
      if (other != r && riders[other].alive && abs(riders[other].x - nx) + abs(riders[other].y - ny) == 1) {
        score -= 40;  // It could go there too
      }
    }
    if (score > bestscore) {
      bestscore = score;
      me.dx = options[o][0];
      me.dy = options[o][1];
    }
  }
}

static void steer(uint8_t r) {
  Rider &me = riders[r];
  for (uint8_t button; (button = gamepress(r));) {
    int8_t ndx = button == BUTTON_LEFT ? -1 : button == BUTTON_RIGHT ? 1 : 0;
    int8_t ndy = button == BUTTON_UP ? -1 : button == BUTTON_DOWN ? 1 : 0;
    if ((ndx || ndy) && ndx != -me.dx && ndy != -me.dy && (ndx != me.dx || ndy != me.dy)) {
      me.dx = ndx;
      me.dy = ndy;
      break;  // One turn per step
    }
  }
}

static void crash(uint8_t r) {
  riders[r].alive = false;
  for (int16_t x = 0; x < COLS; x++) {
    for (int16_t y = 0; y < ROWS; y++) {
      if (owner[x][y] == r + 1) {
        owner[x][y] = 0;
        next_state[x][y] = false;
      }
    }
  }
}

static void trailsstep() {
  int16_t nx[TRAILS_RIDERS], ny[TRAILS_RIDERS];
  bool hit[TRAILS_RIDERS] = { false };

  for (uint8_t r = 0; r < TRAILS_RIDERS; r++) {
    if (!riders[r].alive) {
      continue;
    }
    if (gamehuman(r)) {
      steer(r);
    } else {
      think(r);
    }
    nx[r] = riders[r].x + riders[r].dx;
    ny[r] = riders[r].y + riders[r].dy;
    hit[r] = !isfree(nx[r], ny[r]);
  }
  for (uint8_t a = 0; a < TRAILS_RIDERS; a++) {  // Head on
    for (uint8_t b = a + 1; b < TRAILS_RIDERS; b++) {
      if (riders[a].alive && riders[b].alive && nx[a] == nx[b] && ny[a] == ny[b]) {
        hit[a] = hit[b] = true;
      }
    }
  }

  for (uint8_t r = 0; r < TRAILS_RIDERS; r++) {
    Rider &me = riders[r];
    if (!me.alive) {
      continue;
    }
    if (hit[r]) {
      crash(r);
      continue;
    }
    setdot(me.x, me.y, visible(r, me.steps));  // The old head becomes wall
    me.x = nx[r];
    me.y = ny[r];
    me.steps++;
    owner[me.x][me.y] = r + 1;
    setdot(me.x, me.y, true);
  }

  uint8_t alive = 0, humans = 0;
  for (uint8_t r = 0; r < TRAILS_RIDERS; r++) {
    if (riders[r].alive) {
      alive++;
      humans += gamehuman(r);
    }
  }
  if (alive <= 1 || humans == 0) {
    gameover(riders[0].steps);
    return;
  }

  if (++stepcount % TRAILS_SPEEDUP == 0 && gameframetime > TRAILS_FAST) {
    gameframetime--;
  }
}

const Game trailsGame = { "trails", "Sporen", TRAILS_RIDERS, trailssetup, trailsstep, ROWS >= 64 ? 70 : 100, nullptr, true };
