#pragma once

#include <Arduino.h>

#include "config.h"

//  v3 boards: a chain of 74HC595 shift registers, see hardware-v3/README.md and bits.csv.
//
//  Every board has six groups of two 595s on the same data: one feeds an XL62783 (outputs to 20 V),
//  the other a ULN2803A (outputs to ground). Groups 0-3 drive the 32 lines on X1, group 4 rows 0-7
//  on X3, group 5 rows 8-15 on X2. Bit n of a group is line or row n of that group.
//  DIR_X1 and DIR_ROW pick the side: high is 20 V, low is ground. nOE high switches everything off.
//  The hardware never lets both sides of a wire on together, whatever the software does.
//
//  Chain from the ESP32: board 0 groups 0-5 (48 bits), board 1, and so on. The bit shifted out first
//  ends up furthest away. Between two boards sits a flip-flop on the falling edge of SCLK: it takes
//  the last bit half a clock after the 595 shifted it out, so it adds no bit of its own.

#define CHAIN_GROUP_BITS  8
#define CHAIN_BOARD_BITS  48  // 6 groups of 8
#define CHAIN_BITS        (CHAIN_BOARDS * CHAIN_BOARD_BITS)
#define CHAIN_WORDS       ((CHAIN_BITS + 31) / 32)
#define CHAIN_MAX_BOARDS  8

// Counters since startup. The pulse itself is timed by the RMT, its length is PULSE_MICROS.
struct ChainStats {
  uint32_t pulses;
};

extern ChainStats chainstats;
extern bool chainfault;          // Set by chainstop() or a failed board count, stays until a restart
extern int8_t chainboardsfound;  // Boards counted over the return line, 0 when the bit didn't come back
extern uint32_t chaincyclesperus;

// First thing at startup: everything off, chain empty, count the boards, hand nOE to the RMT.
// needboards false: flip also when the count is wrong, only for testing without boards.
void chainbegin(bool needboards = true);
int8_t chaincount();   // Sends one bit around the chain, returns the number of boards or 0
void chainclear();     // Shifts zeros into every 595 and latches them
void chainstop();      // nOE back from the RMT and high, everything off, nothing flips anymore

// One coil pulse with the pattern in the 595s. color true: lines to ground and rows to 20 V.
// Returns as soon as the RMT has started the pulse; the next call shifts during it and waits for
// the pulse and its rest to end before it latches. Returns false when nothing was pulsed because of a fault.
bool chainpulse(const uint32_t pattern[CHAIN_WORDS], bool color);

static inline void chainsetbit(uint32_t pattern[CHAIN_WORDS], uint8_t board, uint8_t group, uint8_t bit) {
  uint16_t position = board * CHAIN_BOARD_BITS + group * CHAIN_GROUP_BITS + bit;
  pattern[position / 32] |= 1UL << (position % 32);
}
