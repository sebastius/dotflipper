#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "font.h"
#include "game.h"

const Game *const games[GAME_COUNT] = {
  &snakeGame,
  &trailsGame,
  &tennisGame,
  &tennis4Game,
  &wallGame,
  &blocksGame,
  &invadersGame,
  &rocksGame,
  &landerGame,
  &flutterGame,
  &runnerGame,
  &crossingGame,
  &doublingGame,
  &lampsGame,
  &drawGame,
  &lifeGame,
  &etchGame,
};

int8_t findgame(const char *id) {
  for (uint8_t i = 0; i < GAME_COUNT; i++) {
    if (strcmp(games[i]->id, id) == 0) {
      return i;
    }
  }
  return -1;
}

// Buttons per player, filled by gameinput() from the web server. That runs in the same loop() as the
// games, so nothing changes while a game step runs.

#define PRESS_QUEUE 8

struct PlayerInput {
  uint8_t held;
  uint8_t queue[PRESS_QUEUE];  // Buttons pressed, one per entry
  uint8_t head, count;
};

static PlayerInput input[GAME_PLAYERS];
static uint8_t humans;           // Bit per player that pressed something since the game was chosen
static unsigned long lastinput;  // millis() of the last button

enum GameState { WAITING, RUNNING, FROZEN, SCORE };

static const Game *game;
static GameState state;
static unsigned long nextframe, statesince;
static int32_t finalscore;
static bool showbest;
static int32_t best[GAME_COUNT];
static uint8_t gameindex;
uint16_t gameframetime;
bool gamepaper[COLS][ROWS];

static void clearpresses() {
  for (uint8_t p = 0; p < GAME_PLAYERS; p++) {
    input[p].count = 0;
  }
}

static bool anypress() {
  bool any = false;
  for (uint8_t p = 0; p < GAME_PLAYERS; p++) {
    any |= input[p].count > 0;
  }
  clearpresses();
  return any;
}

void gameinput(uint8_t player, uint8_t held, uint8_t pressed) {
  if (player >= GAME_PLAYERS) {
    return;
  }
  PlayerInput &in = input[player];
  pressed |= held & ~in.held;  // Down now but not before: also a press, even if the client didn't say so
  in.held = held;
  for (uint8_t bit = 1; bit && bit <= BUTTON_B; bit <<= 1) {
    if ((pressed & bit) && in.count < PRESS_QUEUE) {
      in.queue[(in.head + in.count++) % PRESS_QUEUE] = bit;
    }
  }
  if (pressed) {
    humans |= 1 << player;
  }
  lastinput = millis();
}

uint8_t gameheld(uint8_t player) {
  return player < GAME_PLAYERS ? input[player].held : 0;
}

uint8_t gamepress(uint8_t player) {
  if (player >= GAME_PLAYERS || input[player].count == 0) {
    return 0;
  }
  PlayerInput &in = input[player];
  uint8_t button = in.queue[in.head];
  in.head = (in.head + 1) % PRESS_QUEUE;
  in.count--;
  return button;
}

bool gamehuman(uint8_t player) {
  return player == 0 || (humans & (1 << player));
}

static void begin() {
  ClearScreen();
  memset(gamepaper, 0, sizeof(gamepaper));
  gameframetime = game->frametime;
  game->setup();
  fullupdate();
  clearpresses();
  state = game->waits ? WAITING : RUNNING;
  statesince = millis();
  nextframe = statesince;
}

void gametouch(int16_t x, int16_t y, bool on) {
  if (game && game->touch && state != FROZEN && x >= 0 && x < COLS && y >= 0 && y < ROWS) {
    game->touch(x, y, on);
    if (state == WAITING) {
      state = RUNNING;
      nextframe = millis();
    }
  }
  lastinput = millis();
}

#define CURSOR_DELAY  8  // Steps before a held button repeats
#define CURSOR_REPEAT 1

static void cursormove(GameCursor &c, uint8_t buttons) {
  if (buttons & BUTTON_LEFT) c.x--;
  if (buttons & BUTTON_RIGHT) c.x++;
  if (buttons & BUTTON_UP) c.y--;
  if (buttons & BUTTON_DOWN) c.y++;
  c.x = constrain(c.x, 0, COLS - 1);
  c.y = constrain(c.y, 0, ROWS - 1);
}

void cursorpress(GameCursor &c, uint8_t button) {
  if (button & BUTTONS_DIRECTION) {
    cursormove(c, button);
    c.wait = CURSOR_DELAY;
  }
}

void cursorheld(GameCursor &c, uint8_t held) {
  if (!(held & BUTTONS_DIRECTION)) {
    c.wait = CURSOR_DELAY;
  } else if (c.wait > 0) {
    c.wait--;
  } else {
    cursormove(c, held);
    c.wait = CURSOR_REPEAT;
  }
}

void gamecircle(int16_t cx, int16_t cy, int16_t r, bool on) {
  int16_t x = r, y = 0, err = 1 - r;
  while (x >= y) {
    setdot(cx + x, cy + y, on); setdot(cx - x, cy + y, on);
    setdot(cx + x, cy - y, on); setdot(cx - x, cy - y, on);
    setdot(cx + y, cy + x, on); setdot(cx - y, cy + x, on);
    setdot(cx + y, cy - x, on); setdot(cx - y, cy - x, on);
    y++;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

void gamestart(uint8_t index) {
  Serial.printf("Game: %s\n", games[index]->name);
  game = games[index];
  gameindex = index;
  humans = 1;
  memset(input, 0, sizeof(input));
  lastinput = millis();
  begin();
}

const Game *gamecurrent() {
  return game;
}

void gameover(int32_t score, bool best) {
  finalscore = score;
  showbest = best;
  state = FROZEN;
  statesince = millis();
}

void gamedigits(int32_t number, int16_t x, int16_t y, uint8_t scale, bool right) {
  char text[12];
  snprintf(text, sizeof(text), "%ld", (long)number);
  int16_t width = strlen(text) * 4 * scale - scale;
  if (right) {
    x -= width;
  }
  for (const char *c = text; *c; c++, x += 4 * scale) {
    if (*c < '0' || *c > '9') {
      continue;
    }
    const uint8_t *glyph = fontDigits[*c - '0'];
    for (uint8_t row = 0; row < 5; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        bool on = glyph[row] & (4 >> col);
        for (uint8_t dx = 0; dx < scale; dx++) {
          for (uint8_t dy = 0; dy < scale; dy++) {
            setdot(x + col * scale + dx, y + row * scale + dy, on);
          }
        }
      }
    }
  }
}

int16_t gamedigitswidth(int32_t number, uint8_t scale) {
  char text[12];
  snprintf(text, sizeof(text), "%ld", (long)number);
  return strlen(text) * 4 * scale - scale;
}

static void showscore() {
  memset(next_state, 0, sizeof(next_state));
  if (finalscore >= 0) {
    if (showbest && finalscore > best[gameindex]) {
      best[gameindex] = finalscore;
    }
    uint8_t scale = ROWS >= 64 ? 4 : 2;
    while (scale > 1 && gamedigitswidth(finalscore, scale) > COLS - 2) {
      scale--;
    }
    int16_t top = ROWS / 2 - 5 * scale / 2 - 4;
    gamedigits(finalscore, (COLS - gamedigitswidth(finalscore, scale)) / 2, top, scale, false);

    // The best score this game since the start, small, under a line
    if (!showbest) {
      fullupdate();
      return;
    }
    uint8_t small = ROWS >= 64 ? 2 : 1;
    int16_t line = top + 5 * scale + 3;
    drawline(COLS / 2 - 4, line, COLS / 2 + 3, line, true);
    gamedigits(best[gameindex], (COLS - gamedigitswidth(best[gameindex], small)) / 2, line + 3, small, false);
  }
  fullupdate();
}

bool gameloop() {
  unsigned long now = millis();
  uint16_t idle = state == SCORE ? GAME_OVER_SECONDS : game->waits ? GAME_IDLE_SECONDS : DRAW_IDLE_SECONDS;
  if (now - lastinput > idle * 1000UL) {
    game = nullptr;
    return false;
  }

  switch (state) {
    case WAITING:  // The start position, until someone presses a button
      if (anypress()) {
        state = RUNNING;
        nextframe = now;
      }
      break;

    case RUNNING:
      if ((long)(now - nextframe) >= 0) {
        game->step();
        fullupdate();
        nextframe = millis() + gameframetime;
      }
      break;

    case FROZEN:  // The end position stays a moment, buttons pressed in a hurry don't count
      if (now - statesince > 1500) {
        showscore();
        clearpresses();
        state = SCORE;
        statesince = now;
      }
      break;

    case SCORE:
      if (now - statesince > 500 && anypress()) {
        begin();
      }
      break;
  }
  return true;
}
