#include <ArduinoOTA.h>

#include "config.h"
#include "display.h"
#include "gol.h"

void GoLnext() {
  memset(next_state, 0, sizeof(next_state));
  int x;
  int y;

  boolean value;

  for (int r = 0; r < ROWS; r++) { // for each row
    for (int c = 0; c < COLS; c++) { // and each column

      // count how many live neighbors this cell has
      int liveNeighbors = 0;
      for (int i = -1; i < 2; i++) {
        y = r + i;

        // wrap y
        if (y == -1) {
          y = ROWS - 1;
        } else if (y == ROWS) {
          y = 0;
        }

        for (int j = -1; j < 2; j++) {
          if (i != 0 || j != 0) {
            x = c + j;

            // wrap
            if (x == -1) {
              x = COLS - 1;
            } else if (x == COLS) {
              x = 0;
            }

            if (current_state[x][y]) {
              liveNeighbors++;
            }
          }
        }
      }

      // apply the rules
      if (current_state[c][r] && liveNeighbors >= 2 && liveNeighbors <= 3) { // live cells with 2 or 3 neighbors remain alive
        value = true;
      } else if (!current_state[c][r] && liveNeighbors == 3) { // dead cells with 3 neighbors become alive
        value = true;
      } else {
        value = false;
      }

      next_state[c][r] = value;
    }
  }
}

void GoLrandomize() {
  randomSeed(millis());

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (random(100) < GOL_DENSITY) {
        next_state[c][r] = true;
      }
    }
  }
}

void GoL() {
  GoLrandomize();
  fullupdate();
  for (uint8_t i = 0; i < GOL_GENERATIONS; i++) {
    ArduinoOTA.handle();
    yield();
    long starttijd = millis();
    GoLnext();
    fullupdate();
    long gewacht = (millis() - starttijd);
    if (gewacht < GOL_FRAMETIME) {
      delay(GOL_FRAMETIME - gewacht);
    }
  }
}
