#pragma once

#include <Arduino.h>

#include "config.h"

// Games: like an animation a game draws into next_state, the game framework (game.cpp) flips the dots,
// keeps the time, waits for the first button, shows the score at the end and starts again.
// The buttons come from the web interface: the controller page on /play sends them to /api/input.

enum : uint8_t {
  BUTTON_UP = 1,
  BUTTON_DOWN = 2,
  BUTTON_LEFT = 4,
  BUTTON_RIGHT = 8,
  BUTTON_A = 16,
  BUTTON_B = 32,
};
#define BUTTONS_DIRECTION (BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT)

#define GAME_PLAYERS 4
#define GAME_IDLE_SECONDS 90  // Back to the clock and animations when nobody pressed a button this long
#define GAME_OVER_SECONDS 30  // ... or this long after a game over
#define DRAW_IDLE_SECONDS 300 // ... or this long for the drawing boards, to look at what you made

struct Game {
  const char *id;      // Short name for the web API
  const char *name;
  uint8_t players;     // How many people can play; the other sides are played by the computer
  void (*setup)();     // The screen is cleared before setup. Draw the start position
  void (*step)();      // One frame: read the buttons, draw into next_state, call gameover() at the end
  uint16_t frametime;  // in ms at the start, a game can change gameframetime while it runs
  void (*touch)(int16_t x, int16_t y, bool on);  // Optional: a dot set by tapping the screen on the phone
  bool waits;          // Wait for a button before the first step (games); false: start right away (drawing)
};

extern const Game snakeGame;
extern const Game trailsGame;
extern const Game tennisGame;
extern const Game tennis4Game;
extern const Game wallGame;
extern const Game blocksGame;
extern const Game doublingGame;
extern const Game lampsGame;
extern const Game invadersGame;
extern const Game rocksGame;
extern const Game landerGame;
extern const Game flutterGame;
extern const Game runnerGame;
extern const Game crossingGame;
extern const Game drawGame;
extern const Game lifeGame;
extern const Game etchGame;

#define GAME_COUNT 17
extern const Game *const games[GAME_COUNT];
int8_t findgame(const char *id);  // -1 if there is no game with this id

// For the games
extern uint16_t gameframetime;            // ms until the next step
uint8_t gameheld(uint8_t player);         // Buttons held down right now
uint8_t gamepress(uint8_t player);        // Next button that was pressed, oldest first, 0 if none
bool gamehuman(uint8_t player);           // This player pressed a button since the game was chosen (player 0 always)
void gameover(int32_t score, bool best = true);  // The game has ended. score < 0: no score, best: show the best score too

// One sheet for the drawing boards, they never run at the same time. Cleared before their setup
extern bool gamepaper[COLS][ROWS];

// A cursor for the drawing games: a press moves one dot, holding repeats
struct GameCursor {
  int16_t x, y;
  uint8_t wait;
};
void cursorpress(GameCursor &c, uint8_t button);  // Call for every press
void cursorheld(GameCursor &c, uint8_t held);     // Call once per step

// For the player and the web interface
void gamestart(uint8_t index);
bool gameloop();                          // False when nobody plays anymore
const Game *gamecurrent();
void gameinput(uint8_t player, uint8_t held, uint8_t pressed);  // pressed: buttons that went down since the last call
void gametouch(int16_t x, int16_t y, bool on);                  // A tap on the screen of the phone

// Drawing helpers for the games
void gamedigits(int32_t number, int16_t x, int16_t y, uint8_t scale, bool right);  // 3x5 digits, right: x is the right edge
int16_t gamedigitswidth(int32_t number, uint8_t scale);
void gamecircle(int16_t cx, int16_t cy, int16_t r, bool on);  // Outline
