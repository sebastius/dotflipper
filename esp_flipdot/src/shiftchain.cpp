#ifdef HARDWARE_V3

#include "shiftchain.h"
#include "chainpins.h"

// Everything that decides how long a coil gets current runs from IRAM with interrupts off, so the
// pulse takes the same time in every build. The rest, shifting and latching, only happens while nOE
// is high: then no output can be on, however long it takes.

ChainStats chainstats = { 0, 0xFFFFFFFF, 0, 0 };
bool chainfault = false;
int8_t chainboardsfound = -1;
uint32_t chaincyclesperus;

static portMUX_TYPE chainmux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t halfclock;     // in cycles, at CHAIN_CLOCK
static uint32_t lastpulseend;  // in cycles
static bool pulsed = false;

static inline void IRAM_ATTR waitcycles(uint32_t cycles) {
  uint32_t start = chaincycles();
  while (chaincycles() - start < cycles) {
  }
}

// One bit into the chain: the 595s take it on the rising edge, the flip-flop to the next board
// on the falling edge
static inline void IRAM_ATTR clockbit(bool bit, uint32_t half) {
  setdata(bit);
  waitcycles(half);
  setsclk(true);
  waitcycles(half);
  setsclk(false);
}

static void IRAM_ATTR shiftout(const uint32_t pattern[CHAIN_WORDS]) {
  for (int16_t position = CHAIN_BITS - 1; position >= 0; position--) {
    clockbit((pattern[position / 32] >> (position % 32)) & 1, halfclock);
  }
  setdata(false);
}

static void IRAM_ATTR latch() {
  waitcycles(halfclock);
  setlatch(true);
  waitcycles(halfclock);
  setlatch(false);
}

void chainbegin() {
  chaincyclesperus = getCpuFrequencyMhz();
  halfclock = chaincyclesperus * 1000000UL / CHAIN_CLOCK / 2;

  // Levels first, then outputs: nOE never goes low on the way. Until here R1-R3 on the board kept
  // nOE high and DIR low.
  setnoe(true);
  setdata(false);
  setsclk(false);
  setlatch(false);
  setdirx1(false);
  setdirrow(false);
  for (uint8_t pin : { CHAIN_PIN_NOE, CHAIN_PIN_DATA, CHAIN_PIN_SCLK, CHAIN_PIN_LATCH, CHAIN_PIN_DIR_X1, CHAIN_PIN_DIR_ROW }) {
    pinMode(pin, OUTPUT);
  }
  pinMode(CHAIN_PIN_RETURN, INPUT);
  setnoe(true);

  chainclear();

  chainboardsfound = chaincount();
  if (chainboardsfound != CHAIN_BOARDS) {
    // Wrong chain or JP1 open: the bits wouldn't end up where we think, so don't flip anything
    chainfault = true;
  }
  chainclear();
}

// nOE stays high and nothing gets latched, so whatever is shifted here never reaches a coil.
// Chain of n boards: a bit comes back over the return line after n * 48 clocks.
int8_t chaincount() {
  const uint32_t half = chaincyclesperus * 1000000UL / CHAIN_CHECK_CLOCK / 2;
  const uint16_t length = CHAIN_MAX_BOARDS * CHAIN_BOARD_BITS + CHAIN_BOARD_BITS;

  setnoe(true);
  for (uint16_t i = 0; i < length; i++) {
    clockbit(false, half);
  }

  int16_t found = -1;
  uint8_t ones = 0;
  for (uint16_t clock = 1; clock <= length; clock++) {
    clockbit(clock == 1, half);
    waitcycles(half);  // The return line passes the buffer of every board
    if (readreturn()) {
      ones++;
      found = clock;
    }
  }
  setdata(false);

  if (ones != 1 || found % CHAIN_BOARD_BITS != 0) {
    return 0;  // Nothing came back, or more than the one bit: something is wrong with the chain
  }
  return found / CHAIN_BOARD_BITS;
}

void chainclear() {
  static const uint32_t zeros[CHAIN_WORDS] = { 0 };
  setnoe(true);
  shiftout(zeros);
  latch();
}

void chainstop() {
  setnoe(true);
  chainfault = true;
  chainclear();
}

static void countpulse(uint32_t cycles) {
  chainstats.pulses++;
  chainstats.pulsetotal += cycles;
  if (cycles > chainstats.pulsemax) {
    chainstats.pulsemax = cycles;
  }
  if (cycles < chainstats.pulsemin) {
    chainstats.pulsemin = cycles;
  }
  if (cycles > PULSE_LIMIT_MICROS * chaincyclesperus) {
    chainstop();
  }
}

bool IRAM_ATTR chainpulse(const uint32_t pattern[CHAIN_WORDS], bool color) {
  if (chainfault) {
    return false;
  }

  // nOE is high: the new pattern and direction can't switch anything on yet
  shiftout(pattern);
  latch();
  setdirx1(!color);
  setdirrow(color);

  const uint32_t length = PULSE_MICROS * chaincyclesperus;
  const uint32_t gap = PULSE_GAP_MICROS * chaincyclesperus;
  while (pulsed && chaincycles() - lastpulseend < gap) {
  }

  portENTER_CRITICAL(&chainmux);
  uint32_t start = chaincycles();
  setnoe(false);
  while (chaincycles() - start < length) {
  }
  setnoe(true);
  uint32_t end = chaincycles();
  portEXIT_CRITICAL(&chainmux);

  lastpulseend = end;
  pulsed = true;
  countpulse(end - start);
  return true;
}

#endif
