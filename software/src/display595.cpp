#ifdef HARDWARE_V3

// v3: flipping dots through the chain of 74HC595 boards, see shiftchain.h.
//
// Every board flips at most one dot per pulse: one line and one row, so one coil per wire, within
// what a driver output may carry. All boards pulse at the same time, but DIR is shared by the whole
// chain, so one pulse flips dots to one color only. fullupdate() therefore does all yellow dots,
// then all black ones, and pairs up the dots of different boards.
//
// To check on the first board: that x 0 is X1 pin 1 and y 0 is X3 pins 1 and 2, as on the v1 boards.

#include "display.h"
#include "shiftchain.h"

void driverbegin() {
  chainbegin();
}

bool driverfault() {
  return chainfault;
}

// Bits for dot (x, y) of the whole screen: its board, the line bit and the row bit
static void addot(uint32_t pattern[CHAIN_WORDS], uint16_t x, uint16_t y) {
  uint8_t board = (x / PANEL_WIDTH) + (y / PANEL_HEIGHT) * PANELS_X;
  uint8_t line = x % PANEL_WIDTH;  // 0-31, groups 0-3
  uint8_t row = y % PANEL_HEIGHT;  // 0-15, groups 4-5
  chainsetbit(pattern, board, line / 8, line % 8);
  chainsetbit(pattern, board, 4 + row / 8, row % 8);
}

void flipdot(uint16_t x, uint16_t y, bool color) {
  if (chainfault) {
    return;
  }
  // Input sanitation: a wrong address must never reach the hardware
  x %= COLS;
  y %= ROWS;

  uint32_t pattern[CHAIN_WORDS] = { 0 };
  addot(pattern, x, y);
  chainpulse(pattern, color);
  chainclear();
}

// Flips one dot and reads the coil current on IO4 as often as the ADC can, from the start of the pulse
// until 150 us after its end. The INA180 gives 5 V per A over the 0.1 ohm shunt, R6/R7 make that
// 3.33 V per A on IO4. Returns the number of samples; baseline: IO4 before the pulse, in mV.
uint16_t measureflip(uint16_t x, uint16_t y, bool color, uint16_t *us, uint16_t *mv, uint16_t max, uint16_t *baseline) {
  if (chainfault) {
    return 0;
  }
  x %= COLS;
  y %= ROWS;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++) {
    sum += analogReadMilliVolts(CHAIN_PIN_CURRENT);
  }
  *baseline = sum / 8;

  uint32_t pattern[CHAIN_WORDS] = { 0 };
  addot(pattern, x, y);
  if (!chainpulse(pattern, color)) {
    return 0;
  }
  uint32_t start = micros();  // The pulse started during chainpulse(), at most some us before this
  uint16_t n = 0;
  while (n < max && micros() - start < PULSE_MICROS + 150) {
    us[n] = micros() - start;
    mv[n] = analogReadMilliVolts(CHAIN_PIN_CURRENT);
    n++;
  }
  chainclear();
  current_state[x][y] = next_state[x][y] = color;
  return n;
}

// Flips every changed dot from current_state to next_state.
void fullupdate() {
  // Per board the dots still to flip, as local x * 16 + y
  static uint16_t todo[CHAIN_BOARDS][PANEL_WIDTH * PANEL_HEIGHT];
  uint16_t count[CHAIN_BOARDS];

  if (chainfault) {
    return;
  }

  for (bool color : { true, false }) {
    uint16_t steps = 0;
    for (uint8_t board = 0; board < CHAIN_BOARDS; board++) {
      count[board] = 0;
      uint16_t left = (board % PANELS_X) * PANEL_WIDTH;
      uint16_t top = (board / PANELS_X) * PANEL_HEIGHT;
      for (uint8_t x = 0; x < PANEL_WIDTH; x++) {
        for (uint8_t y = 0; y < PANEL_HEIGHT; y++) {
          bool want = next_state[left + x][top + y];
          if (want == color && current_state[left + x][top + y] != want) {
            todo[board][count[board]++] = x * PANEL_HEIGHT + y;
          }
        }
      }
      if (count[board] > steps) {
        steps = count[board];
      }
    }

    for (uint16_t step = 0; step < steps; step++) {
      uint32_t pattern[CHAIN_WORDS] = { 0 };
      for (uint8_t board = 0; board < CHAIN_BOARDS; board++) {
        if (step < count[board]) {
          uint16_t left = (board % PANELS_X) * PANEL_WIDTH;
          uint16_t top = (board / PANELS_X) * PANEL_HEIGHT;
          addot(pattern, left + todo[board][step] / PANEL_HEIGHT, top + todo[board][step] % PANEL_HEIGHT);
        }
      }
      if (!chainpulse(pattern, color)) {
        chainclear();
        return;  // Emergency stop: current_state stays as it was
      }
    }
  }

  chainclear();
  memcpy(current_state, next_state, sizeof(next_state));
}

#endif
