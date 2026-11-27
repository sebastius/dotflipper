#ifdef PULSETEST

// Test of the coil pulse on a bare ESP32-S3-DevKitC-1, no boards connected: pio run -e v3-pulsetest -t upload,
// then pio device monitor. Uses the real shiftchain.cpp: nOE (IO9) comes from the RMT.
//
// Core 0 watches IO9 with interrupts off and measures every low (the pulse) and high (the rest) it sees
// from start to end, to 1/240 us. For the crash test it can't: a crash stops both cores. Put a scope or
// logic analyser on IO9 and IO10 (LATCH) for that one.
//
// Commands:
//   p  10000 pulses, as fast as the chain allows
//   f  2000 pulses while writing to flash, which holds up interrupts: no false emergency stop
//   h  hang with interrupts off right after a pulse starts: the pulse must still be 224 us, then the
//      interrupt watchdog resets the ESP
//   c  crash (abort) right after a pulse starts: check on the scope that the pulse is 224 us
//   r  show the measurements again
//
// Expected: every low 224.0 us, every high at least 100 us, LATCH never while IO9 is low.

#include <Arduino.h>
#include <Preferences.h>
#include <esp_rom_sys.h>

#include "config.h"
#include "shiftchain.h"
#include "chainpins.h"

static volatile uint32_t lows, lowmin = 0xFFFFFFFF, lowmax, highmin = 0xFFFFFFFF;
static volatile bool monitoring = true;
static volatile bool reportfromcore0 = false;
static volatile bool inwindow = false;  // Core 0 is measuring with its interrupts off

static void resetmeasurements() {
  lows = lowmax = 0;
  lowmin = highmin = 0xFFFFFFFF;
}

// Measures only lows and highs that start and end in the same window with interrupts off
static void monitor(void *) {
  while (true) {
    portDISABLE_INTERRUPTS();
    inwindow = true;
    bool last = readnoe(), valid = false;
    uint32_t edge = chaincycles();
    for (uint32_t i = 0; i < 400000 && monitoring; i++) {
      bool now = readnoe();
      if (now == last) {
        continue;
      }
      uint32_t t = chaincycles();
      if (valid) {
        uint32_t length = t - edge;
        if (now) {  // A low ended: that was a pulse
          lows++;
          if (length < lowmin) lowmin = length;
          if (length > lowmax) lowmax = length;
        } else if (length < highmin) {
          highmin = length;
        }
      }
      valid = true;
      edge = t;
      last = now;
    }
    inwindow = false;
    portENABLE_INTERRUPTS();
    if (reportfromcore0) {
      // Core 1 hangs with its interrupts off: Serial needs it, the ROM printf doesn't
      reportfromcore0 = false;
      esp_rom_delay_us(2000);
      esp_rom_printf("core 0: %u pulsen gemeten, laag %u tot %u ns\n", lows,
                     lowmin == 0xFFFFFFFF ? 0 : lowmin * 1000 / 240, lowmax * 1000 / 240);
    }
    vTaskDelay(1);
  }
}

static void report() {
  delay(20);  // Let core 0 finish its window
  Serial.printf("chain: %u pulsen gegeven, storing: %s\n", chainstats.pulses, chainfault ? "JA" : "nee");
  if (!lows) {
    Serial.println("core 0: geen pulsen gemeten");
    return;
  }
  // The pulses alternate between the colors: every low must be one of the two lengths
  Serial.printf("core 0: %u pulsen gemeten, laag %.2f tot %.2f us (doel %d geel, %d zwart), kortste rust %.2f us (minstens %d)\n", lows,
                lowmin / 240.0, lowmax / 240.0, PULSE_MICROS_YELLOW, PULSE_MICROS_BLACK, highmin == 0xFFFFFFFF ? 0 : highmin / 240.0, PULSE_GAP_MICROS);
  const double shortest = min(PULSE_MICROS_YELLOW, PULSE_MICROS_BLACK), longest = PULSE_MICROS;
  bool ok = lowmin / 240.0 > shortest - 0.5 && lowmax / 240.0 < longest + 0.5 &&
            (highmin == 0xFFFFFFFF || highmin / 240.0 >= PULSE_GAP_MICROS - 0.5);
  Serial.println(ok ? "GOED" : "FOUT: pulsen niet zoals verwacht");
}

static void pulses(uint32_t count, bool flash) {
  uint32_t pattern[CHAIN_WORDS] = { 0 };
  chainsetbit(pattern, 0, 0, 0);
  chainsetbit(pattern, 0, 4, 0);
  Preferences preferences;
  if (flash) {
    preferences.begin("pulsetest", false);
  }
  resetmeasurements();
  uint32_t start = micros();
  for (uint32_t i = 0; i < count && !chainfault; i++) {
    chainpulse(pattern, i & 1);
    if (flash && i % 20 == 0) {
      preferences.putUInt("count", i);  // Flash write: interrupts wait meanwhile
    }
  }
  chainclear();
  uint32_t took = micros() - start;
  if (flash) {
    preferences.end();
  }
  Serial.printf("%u pulsen in %u ms, %.0f us per stap\n", count, took / 1000, (double)took / count);
  report();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  chainbegin(false);  // No boards: count them, but pulse anyway
  Serial.printf("\nPulstest v3, %d borden ingesteld, %d gevonden. Commando's: p f h c r\n", CHAIN_BOARDS, chainboardsfound);
  if (chainfault) {
    Serial.println("Storing bij het opstarten: de RMT is niet gestart");
  }
  xTaskCreatePinnedToCore(monitor, "monitor", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
  if (!Serial.available()) {
    return;
  }
  char command = Serial.read();
  uint32_t pattern[CHAIN_WORDS] = { 0 };
  chainsetbit(pattern, 0, 0, 0);
  switch (command) {
    case 'p':
      pulses(10000, false);
      break;
    case 'f':
      pulses(2000, true);
      break;
    case 'h':
      Serial.println("Hangt over 1 s met interrupts uit, de watchdog reset daarna");
      delay(1000);
      resetmeasurements();
      while (inwindow) {  // Start at the beginning of a window, so core 0 sees the whole pulse
      }
      while (!inwindow) {
      }
      chainpulse(pattern, true);
      reportfromcore0 = true;
      portDISABLE_INTERRUPTS();
      while (true) {
      }
    case 'c':
      Serial.println("Crasht over 1 s, kijk op de scoop naar IO9");
      delay(1000);
      chainpulse(pattern, true);
      abort();
    case 'r':
      report();
      break;
  }
}

#endif
