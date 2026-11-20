#ifdef HARDWARE_V3

#include "shiftchain.h"
#include "chainpins.h"

// The coil pulse is made by the RMT peripheral, not by the CPU: it drives nOE low for PULSE_MICROS
// and then keeps it high for PULSE_GAP_MICROS, counting on the crystal. Once a pulse has started it
// ends on time whatever the firmware does next: a crash, a hang, an interrupt, WiFi. The CPU only
// shifts, latches and sets DIR, and LATCH and DIR only change when the RMT is done with a pulse and
// its rest. Shifting may go on during a pulse: the 595 outputs only change at LATCH.

ChainStats chainstats = { 0 };
bool chainfault = false;
int8_t chainboardsfound = -1;
uint32_t chaincyclesperus;

static uint32_t halfclock;     // in cycles, at CHAIN_CLOCK
static uint32_t pulsestarted;  // in cycles, when the RMT got the last pulse
static bool pulsing = false;
static const uint32_t zeros[CHAIN_WORDS] = { 0 };

#ifndef SIMULATE
// The RMT on nOE. invert_out: level 1 in a symbol is nOE low (pulse), level 0 and idle are nOE high.

#include <driver/rmt_tx.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>

static rmt_channel_handle_t pulsechannel;
static rmt_encoder_handle_t pulseencoder;
static rmt_symbol_word_t pulsesymbol;

static bool pulsebegin(uint16_t on, uint16_t rest) {
  rmt_tx_channel_config_t channel = {};
  channel.gpio_num = (gpio_num_t)CHAIN_PIN_NOE;
  channel.clk_src = RMT_CLK_SRC_DEFAULT;
  channel.resolution_hz = 1000000;  // 1 us per tick
  channel.mem_block_symbols = 48;
  channel.trans_queue_depth = 1;
  channel.flags.invert_out = true;
  channel.flags.init_level = 0;    // nOE high from the first moment
  channel.flags.io_loop_back = 1;  // Keeps the input of the pin on, readnoe() checks the real level
  rmt_copy_encoder_config_t encoder = {};
  pulsesymbol.level0 = 1;
  pulsesymbol.duration0 = on;
  pulsesymbol.level1 = 0;
  pulsesymbol.duration1 = rest;
  return rmt_new_tx_channel(&channel, &pulsechannel) == ESP_OK &&
         rmt_new_copy_encoder(&encoder, &pulseencoder) == ESP_OK &&
         rmt_enable(pulsechannel) == ESP_OK;
}

static bool pulsestart() {
  rmt_transmit_config_t config = {};
  config.loop_count = 0;
  config.flags.eot_level = 0;  // nOE high after the symbol, also if the CPU never comes back
  return rmt_transmit(pulsechannel, pulseencoder, &pulsesymbol, sizeof(pulsesymbol), &config) == ESP_OK;
}

static bool pulsebusy() {
  return pulsechannel && rmt_tx_wait_all_done(pulsechannel, 0) != ESP_OK;
}

// Takes nOE back from the RMT and holds it high
static void pulserelease() {
  setnoe(true);
  esp_rom_gpio_connect_out_signal(CHAIN_PIN_NOE, SIG_GPIO_OUT_IDX, false, false);
  if (pulsechannel) {
    rmt_disable(pulsechannel);
  }
}
#endif

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

// Waits until the RMT is done with the last pulse and its rest, and nOE really is high. Two ways
// to fail, both end in chainstop(), which takes nOE back from the RMT:
//  - nOE still low PULSE_LIMIT_MICROS after the start: the RMT doesn't end the pulse
//  - the driver not done after PULSE_STUCK_MICROS: its interrupt can be late (a flash write holds
//    them up), that alone is safe as long as nOE is high, so this limit is long
#define PULSE_LIMIT_MICROS_V3 (PULSE_MICROS + 20)
#define PULSE_STUCK_MICROS    10000
static bool waitpulse() {
  if (!pulsing) {
    return true;
  }
  while (true) {
    uint32_t since = chaincycles() - pulsestarted;
    bool low = !readnoe();
    if (low && since > PULSE_LIMIT_MICROS_V3 * chaincyclesperus) {
      chainstop();
      return false;
    }
    if (!low && !pulsebusy()) {
      pulsing = false;
      return true;
    }
    if (since > PULSE_STUCK_MICROS * chaincyclesperus) {
      chainstop();
      return false;
    }
  }
}

// Copies the shifted bits to the outputs: only with nOE high, so after waitpulse()
static void latch() {
  waitcycles(halfclock);
  setlatch(true);
  waitcycles(halfclock);
  setlatch(false);
}

void chainbegin(bool needboards) {
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

  // Empty latches before anything can pulse: a glitch on nOE while handing it to the RMT
  // then has nothing to switch on
  shiftout(zeros);
  latch();

  chainboardsfound = chaincount();
  if (needboards && chainboardsfound != CHAIN_BOARDS) {
    // Wrong chain or JP1 open: the bits wouldn't end up where we think, so don't flip anything
    chainfault = true;
  }
  shiftout(zeros);
  latch();

  if (!pulsebegin(PULSE_MICROS, PULSE_GAP_MICROS)) {
    pulserelease();
    chainfault = true;
  }
}

// nOE stays high and nothing gets latched, so whatever is shifted here never reaches a coil.
// Chain of n boards: a bit comes back over the return line after n * 48 clocks.
int8_t chaincount() {
  const uint32_t half = chaincyclesperus * 1000000UL / CHAIN_CHECK_CLOCK / 2;
  const uint16_t length = CHAIN_MAX_BOARDS * CHAIN_BOARD_BITS + CHAIN_BOARD_BITS;

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
  shiftout(zeros);
  if (waitpulse()) {
    latch();
  }
}

void chainstop() {
  pulserelease();
  pulsing = false;
  chainfault = true;
  shiftout(zeros);
  latch();
}

bool chainpulse(const uint32_t pattern[CHAIN_WORDS], bool color) {
  if (chainfault) {
    return false;
  }

  // The previous pulse may still be running: the new pattern goes into the shift registers only
  shiftout(pattern);
  if (!waitpulse()) {
    return false;
  }

  // nOE is high and stays high until the RMT starts the pulse
  latch();
  setdirx1(!color);
  setdirrow(color);
  pulsestarted = chaincycles();
  pulsing = true;
  if (!pulsestart()) {
    chainstop();
    return false;
  }
  chainstats.pulses++;
  return true;
}

#endif
