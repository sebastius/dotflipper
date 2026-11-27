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

// Flips one dot and records the coil current on IO4 with the ADC in continuous mode: a sample every
// 12 us (83333 Hz, the fastest of the S3) by DMA, while the CPU starts the pulse. The INA180 gives 5 V per A
// over the 0.1 ohm shunt, R6/R7 make that 3.33 V per A on IO4. Returns the number of samples written:
// from just before the current starts to rise, us relative to the first of those. baseline: IO4 in mV
// before the pulse. Not for the simulation: there is no ADC there.
#ifndef SIMULATE
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#define ADC_RATE 83333

uint16_t measureflip(uint16_t x, uint16_t y, bool color, uint16_t *us, uint16_t *mv, uint16_t max, uint16_t *baseline) {
  if (chainfault) {
    return 0;
  }
  x %= COLS;
  y %= ROWS;

  adc_continuous_handle_t adc = nullptr;
  adc_continuous_handle_cfg_t handleconfig = {};
  handleconfig.max_store_buf_size = 4096;  // 1024 samples: 12 ms
  handleconfig.conv_frame_size = 256;
  adc_digi_pattern_config_t pattern = {};
  pattern.atten = ADC_ATTEN_DB_12;
  pattern.channel = ADC_CHANNEL_3;  // GPIO4
  pattern.unit = ADC_UNIT_1;
  pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
  adc_continuous_config_t config = {};
  config.pattern_num = 1;
  config.adc_pattern = &pattern;
  config.sample_freq_hz = ADC_RATE;
  config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  config.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
  adc_cali_handle_t cali = nullptr;
  adc_cali_curve_fitting_config_t caliconfig = {};
  caliconfig.unit_id = ADC_UNIT_1;
  caliconfig.chan = ADC_CHANNEL_3;
  caliconfig.atten = ADC_ATTEN_DB_12;
  caliconfig.bitwidth = ADC_BITWIDTH_12;
  if (adc_continuous_new_handle(&handleconfig, &adc) != ESP_OK || adc_continuous_config(adc, &config) != ESP_OK ||
      adc_cali_create_scheme_curve_fitting(&caliconfig, &cali) != ESP_OK || adc_continuous_start(adc) != ESP_OK) {
    if (adc) adc_continuous_deinit(adc);
    if (cali) adc_cali_delete_scheme_curve_fitting(cali);
    return 0;
  }

  // 2 ms before, the pulse, 2 ms after: about 350 samples, well within the 1024 of the buffer
  delay(2);
  uint32_t pattern595[CHAIN_WORDS] = { 0 };
  addot(pattern595, x, y);
  bool pulsed = chainpulse(pattern595, color);
  delayMicroseconds(2000);
  chainclear();

  // Read while it still runs: the driver only hands out data in the running state
  static uint16_t raw[1024];
  uint16_t count = 0;
  uint8_t buffer[256];
  uint32_t got = 0;
  while (count < 1024 && adc_continuous_read(adc, buffer, sizeof(buffer), &got, 0) == ESP_OK) {
    for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= got && count < 1024; i += SOC_ADC_DIGI_RESULT_BYTES) {
      adc_digi_output_data_t *d = (adc_digi_output_data_t *)&buffer[i];
      if (d->type2.channel == ADC_CHANNEL_3) raw[count++] = d->type2.data;
    }
  }
  adc_continuous_stop(adc);
  adc_continuous_deinit(adc);

  // Baseline from the first 100 samples (the 2 ms before the pulse), then from 3 samples before the rise
  uint32_t sum = 0;
  uint16_t first = min<uint16_t>(100, count);
  for (uint16_t i = 0; i < first; i++) {
    int v = 0;
    adc_cali_raw_to_voltage(cali, raw[i], &v);
    sum += v;
  }
  *baseline = first ? sum / first : 0;
  int16_t rise = -1;
  for (uint16_t i = first; i < count && rise < 0; i++) {
    int v = 0;
    adc_cali_raw_to_voltage(cali, raw[i], &v);
    if (v > *baseline + 30) rise = i;  // 30 mV: about 9 mA
  }
  uint16_t n = 0;
  if (pulsed && rise >= 0) {
    uint16_t from = rise >= 3 ? rise - 3 : 0;
    for (uint16_t i = from; i < count && n < max; i++, n++) {
      int v = 0;
      adc_cali_raw_to_voltage(cali, raw[i], &v);
      us[n] = (uint32_t)(i - from) * 1000000UL / ADC_RATE;
      mv[n] = v;
    }
  }
  adc_cali_delete_scheme_curve_fitting(cali);
  current_state[x][y] = next_state[x][y] = color;
  return n;
}
#else
uint16_t measureflip(uint16_t, uint16_t, bool, uint16_t *, uint16_t *, uint16_t, uint16_t *baseline) {
  *baseline = 0;
  return 0;
}
#endif

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
