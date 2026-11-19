#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "animation.h"

// Maze: dug out one cell per frame (recursive backtracker), then a short snake
// walks the way from the top left to the bottom right, then a new maze.
// Cell (cx, cy) is the dot at (2cx+1, 2cy+1), walls are on and passages are off.

#define MAZE_W ((COLS - 1) / 2)
#define MAZE_H ((ROWS - 1) / 2)
#define MAZE_CELLS (MAZE_W * MAZE_H)
#define MAZE_TAIL  3      // Length of the snake behind its head
#define MAZE_PAUSE 50     // Frames to wait when the snake has arrived

typedef uint16_t MazeCell;  // 64x64 dots make 961 cells
#define MAZE_NONE 0xFFFF
static_assert(MAZE_CELLS < MAZE_NONE, "Maze cells are stored in a MazeCell");

enum MazeState { DIGGING, WALKING, PAUSING };

static MazeState state;
static bool visited[MAZE_W][MAZE_H];
static MazeCell stack[MAZE_CELLS];
static MazeCell stacksize;
static uint16_t path[2 * MAZE_CELLS];  // Dots of the solution as x * ROWS + y
static uint16_t pathlength;
static uint16_t position;

static const int8_t directions[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

static void solve() {
  // Breadth first search from the first to the last cell through the open walls
  // Static: at 64x64 these arrays would take almost 6 kB of the 8 kB stack of loop()
  static MazeCell parent[MAZE_CELLS];
  static MazeCell queue[MAZE_CELLS];
  MazeCell head = 0, tail = 0;
  memset(parent, 0xFF, sizeof(parent));
  parent[0] = 0;
  queue[tail++] = 0;
  while (head < tail) {
    MazeCell cell = queue[head++];
    int8_t cx = cell % MAZE_W, cy = cell / MAZE_W;
    for (uint8_t d = 0; d < 4; d++) {
      int8_t nx = cx + directions[d][0], ny = cy + directions[d][1];
      if (nx < 0 || nx >= MAZE_W || ny < 0 || ny >= MAZE_H) continue;
      if (next_state[2 * cx + 1 + directions[d][0]][2 * cy + 1 + directions[d][1]]) continue;  // wall
      MazeCell n = ny * MAZE_W + nx;
      if (parent[n] != MAZE_NONE) continue;
      parent[n] = cell;
      queue[tail++] = n;
    }
  }

  // Walk back from the last cell, then reverse into dots including the ones in between
  static MazeCell cells[MAZE_CELLS];
  MazeCell count = 0;
  for (MazeCell c = MAZE_CELLS - 1; ; c = parent[c]) {
    cells[count++] = c;
    if (c == 0) break;
  }
  pathlength = 0;
  for (int16_t i = count - 1; i >= 0; i--) {
    int16_t x = 2 * (cells[i] % MAZE_W) + 1, y = 2 * (cells[i] / MAZE_W) + 1;
    if (i < count - 1) {
      int16_t px = 2 * (cells[i + 1] % MAZE_W) + 1, py = 2 * (cells[i + 1] / MAZE_W) + 1;
      path[pathlength++] = ((px + x) / 2) * ROWS + (py + y) / 2;
    }
    path[pathlength++] = x * ROWS + y;
  }
}

static void mazesetup() {
  for (uint8_t x = 0; x < 2 * MAZE_W + 1; x++) {
    for (uint8_t y = 0; y < 2 * MAZE_H + 1; y++) {
      next_state[x][y] = true;
    }
  }
  memset(visited, 0, sizeof(visited));
  visited[0][0] = true;
  next_state[1][1] = false;
  stack[0] = 0;
  stacksize = 1;
  state = DIGGING;
}

static void dig() {
  while (stacksize > 0) {
    MazeCell cell = stack[stacksize - 1];
    int8_t cx = cell % MAZE_W, cy = cell / MAZE_W;

    uint8_t options[4], count = 0;
    for (uint8_t d = 0; d < 4; d++) {
      int8_t nx = cx + directions[d][0], ny = cy + directions[d][1];
      if (nx >= 0 && nx < MAZE_W && ny >= 0 && ny < MAZE_H && !visited[nx][ny]) {
        options[count++] = d;
      }
    }
    if (count == 0) {
      stacksize--;  // Dead end, backtrack without using a frame
      continue;
    }

    uint8_t d = options[random(count)];
    int8_t nx = cx + directions[d][0], ny = cy + directions[d][1];
    visited[nx][ny] = true;
    next_state[2 * cx + 1 + directions[d][0]][2 * cy + 1 + directions[d][1]] = false;
    next_state[2 * nx + 1][2 * ny + 1] = false;
    stack[stacksize++] = ny * MAZE_W + nx;
    return;
  }

  solve();
  position = 0;
  state = WALKING;
}

static void walk() {
  uint16_t dot = path[position];
  next_state[dot / ROWS][dot % ROWS] = true;
  if (position > MAZE_TAIL) {
    dot = path[position - MAZE_TAIL - 1];
    next_state[dot / ROWS][dot % ROWS] = false;
  }
  if (++position >= pathlength) {
    position = 0;
    state = PAUSING;
  }
}

static void mazestep() {
  switch (state) {
    case DIGGING:
      dig();
      break;
    case WALKING:
      walk();
      break;
    case PAUSING:
      if (++position >= MAZE_PAUSE) {
        mazesetup();
      }
      break;
  }
}

const Animation mazeAnimation = { "maze", "Doolhof", mazesetup, mazestep, 30 };
