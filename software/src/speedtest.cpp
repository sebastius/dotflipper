#ifdef SPEEDTEST

#pragma GCC optimize("O2")  // The framework builds without optimisation (-O0): far too slow for this loop

// How fast can the chain be shifted? Safe with panels and 20 V on: after chainbegin() (empty latches)
// this never latches and never pulses, nOE stays high from the RMT and DIR stays low. The 595 outputs
// only change at LATCH, so the coils see nothing of the bits going through.
//
// DATA and SCLK go through dedicated GPIO (one CPU cycle per write instead of ~250 ns through the GPIO
// matrix), the return line (via JP1 on the last board) is read the same way. A random pattern goes
// through the chain at several clocks and is compared with what comes back.
//
// pio run -e v3-speedtest -t upload, then send 's' over serial (115200). Put the logic analyser on SCLK
// to see the real clock.

#include <Arduino.h>
#include <driver/dedic_gpio.h>
#include <hal/dedic_gpio_cpu_ll.h>

#include "config.h"
#include "shiftchain.h"
#include "chainpins.h"

#define TEST_BITS 300

static dedic_gpio_bundle_handle_t outbundle, inbundle;
static uint32_t databit, sclkbit, retbit;

static inline void IRAM_ATTR waituntil(uint32_t start, uint32_t cycles) {
  while (chaincycles() - start < cycles) {
  }
}

// Shifts bits[] (then zeros) through the chain with a half clock of `half` cycles and records the return
// line just before every rising edge. Returns the clock in kHz that it really ran at.
static uint32_t IRAM_ATTR shiftloop(const uint8_t *bits, uint8_t *back, uint16_t clocks, uint32_t half) {
  uint32_t mask = databit | sclkbit;
  portDISABLE_INTERRUPTS();
  uint32_t begin = chaincycles();
  uint32_t start = begin;
  for (uint16_t i = 0; i < clocks; i++) {
    dedic_gpio_cpu_ll_write_mask(mask, i < TEST_BITS && bits[i] ? databit : 0);
    waituntil(start, half);
    back[i] = (dedic_gpio_cpu_ll_read_in() & retbit) != 0;
    dedic_gpio_cpu_ll_write_mask(sclkbit, sclkbit);
    waituntil(start, 2 * half);
    dedic_gpio_cpu_ll_write_mask(mask, 0);
    start += 2 * half;
  }
  uint32_t took = chaincycles() - begin;
  portENABLE_INTERRUPTS();
  return (uint64_t)clocks * 240000 / took;
}

static void speedtest() {
  static uint8_t bits[TEST_BITS], back[TEST_BITS + 600];
  uint16_t length = chainboardsfound * CHAIN_BOARD_BITS;
  uint16_t clocks = TEST_BITS + length + 16;
  Serial.printf("Keten van %d borden, %d bits per rondje\n", chainboardsfound, length);
  for (uint32_t mhz10 : { 10, 20, 40, 60, 80, 120 }) {  // in 0.1 MHz
    uint32_t half = 240 * 10 / mhz10 / 2;
    uint16_t worst = 0;
    int16_t offset = -1;
    uint32_t khz = 0;
    for (uint8_t round = 0; round < 5; round++) {
      for (uint16_t i = 0; i < TEST_BITS; i++) bits[i] = esp_random() & 1;
      khz = shiftloop(bits, back, clocks, half);
      // Where does the pattern come back? The best offset around the chain length
      uint16_t besterrors = TEST_BITS;
      int16_t best = -1;
      for (int16_t k = length - 4; k <= length + 4; k++) {
        if (k < 0) continue;
        uint16_t errors = 0;
        for (uint16_t i = 0; i < TEST_BITS; i++) errors += back[i + k] != bits[i];
        if (errors < besterrors) {
          besterrors = errors;
          best = k;
        }
      }
      if (round == 0) offset = best;
      if (best != offset) besterrors = TEST_BITS;  // Moving around is as bad as wrong bits
      worst = max(worst, besterrors);
    }
    Serial.printf("%5.1f MHz ingesteld, %5.2f MHz gemeten: na %d klokken terug, %d van %d bits fout%s\n",
                  mhz10 / 10.0, khz / 1000.0, offset, worst, TEST_BITS, worst == 0 ? "  GOED" : "  FOUT");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  chainbegin(false);  // Empty latches, count the boards, nOE to the RMT (stays high: no pulses here)
  Serial.printf("\nSnelheidstest v3: %d borden gevonden. Commando: s\n", chainboardsfound);

  int out[] = { CHAIN_PIN_DATA, CHAIN_PIN_SCLK };
  dedic_gpio_bundle_config_t outconfig = { .gpio_array = out, .array_size = 2, .flags = { .in_en = 0, .in_invert = 0, .out_en = 1, .out_invert = 0 } };
  int in[] = { CHAIN_PIN_RETURN };
  dedic_gpio_bundle_config_t inconfig = { .gpio_array = in, .array_size = 1, .flags = { .in_en = 1, .in_invert = 0, .out_en = 0, .out_invert = 0 } };
  if (dedic_gpio_new_bundle(&outconfig, &outbundle) != ESP_OK || dedic_gpio_new_bundle(&inconfig, &inbundle) != ESP_OK) {
    Serial.println("Dedicated GPIO lukt niet");
    return;
  }
  uint32_t outmask, inmask;
  dedic_gpio_get_out_mask(outbundle, &outmask);
  dedic_gpio_get_in_mask(inbundle, &inmask);
  databit = outmask & -outmask;  // Lowest channel: the first pin of the bundle
  sclkbit = outmask & ~databit;
  retbit = inmask;
  dedic_gpio_cpu_ll_write_mask(outmask, 0);
}

void loop() {
  if (Serial.available() && Serial.read() == 's') {
    speedtest();
  }
}

#endif
