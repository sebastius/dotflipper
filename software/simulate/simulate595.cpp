// Runs the real v3 code (shiftchain.cpp, display595.cpp, display.cpp) against a model of a chain of
// v3 boards with flip dot panels. See run.sh.
//
// Model, from hardware-v3 (schematic, bits.csv):
//  - per board 48 bits of 595s and a flip-flop to the next board; the 595s shift on the rising edge
//    of SCLK, the flip-flop takes the last bit on the falling edge; LATCH copies the shift registers
//    to the outputs
//  - an output is on when its latched bit is 1 and nOE is low; DIR picks 20 V (high) or ground (low)
//  - lines 0-31 in groups 0-3 with DIR_X1, rows 0-15 in groups 4-5 with DIR_ROW
//  - a coil carries current when its line and its row are on, on opposite sides: line to ground and
//    row to 20 V flips the dot to yellow (true), the other way to black
//  - JP1 on the last board returns its flip-flop over the return line
//
// Rules:
//  1. LATCH and DIR only change while nOE is high
//  2. At most one coil per board carries current
//  3. Everything is off between updates: nOE high and no 1 latched anywhere
//  4. After an update every dot matches next_state
//  5. Every pulse is PULSE_MICROS long, and between two pulses there is PULSE_GAP_MICROS of rest
//
// nOE comes from a model of the RMT once the firmware has handed it over: low for the length of
// the pulse, then high for the rest, on the clock of the model, whatever the firmware does meanwhile.

#include <stdio.h>

#include "Arduino.h"
#include "display.h"
#include "shiftchain.h"
#include "simulatechain.h"

FakeSerial Serial;
FakeESP ESP;

#define HW_MAX 9
#define MHZ 240

static uint64_t cycles;  // 64 bits: 32 would wrap after 18 s of virtual time
uint32_t FakeESP::getCycleCount() { return (uint32_t)cycles; }
void delayMicroseconds(unsigned int us) { cycles += us * MHZ; }

// The hardware
static int hwboards = CHAIN_BOARDS;
static bool loopback = true, stuckdata = false;
static bool sr[HW_MAX][48], ff[HW_MAX], latched[HW_MAX][48];
static bool pdata, psclk, platch, pnoe = true, pdirx1, pdirrow;
static bool dots[HW_MAX][32][16];

// What we saw
static long violations, coilcount;
static bool rule1, rule2, rule5;
static int maxcoils;
static bool coilon[HW_MAX];
static uint64_t coilstart[HW_MAX], lastcoilend;
static bool anycoilended;
static uint32_t pulseshortest = 0xFFFFFFFF, pulselongest;
static uint32_t gapshortest = 0xFFFFFFFF;
static long interruptat = -1;  // Pulse number that gets interrupted for a long time

// The RMT
static bool rmtowned;              // nOE comes from the RMT, GPIO writes don't reach the pin
static bool rmtactive, rmtlow, rmtstuck;
static uint16_t rmton, rmtrest;
static uint64_t rmtlowend, rmtend;
static void updatemodel();

static void violation(const char *text) {
  if (violations++ < 5) printf("    OVERTREDING: %s\n", text);
}

static void updatemodel() {
  bool nowon = false;
  for (int b = 0; b < hwboards; b++) {
    int coils = 0;
    for (int x = 0; x < 32; x++) {
      bool line = latched[b][x] && !pnoe;
      if (!line) continue;
      for (int y = 0; y < 16; y++) {
        bool row = latched[b][32 + y] && !pnoe;
        if (!row || pdirx1 == pdirrow) continue;  // Both sides at the same voltage: no current
        dots[b][x][y] = pdirrow;  // Row to 20 V: yellow
        coils++;
      }
    }
    if (coils > maxcoils) maxcoils = coils;
    if (coils > 1) {
      rule2 = true;
      violation("meer dan 1 spoel tegelijk op een bord");
    }
    bool on = coils > 0;
    if (on && !coilon[b]) {
      coilstart[b] = cycles;
      coilcount++;
      if (anycoilended && cycles - lastcoilend < gapshortest) gapshortest = cycles - lastcoilend;
    } else if (!on && coilon[b]) {
      uint32_t length = cycles - coilstart[b];
      if (length < pulseshortest) pulseshortest = length;
      if (length > pulselongest) pulselongest = length;
      lastcoilend = cycles;
      anycoilended = true;
    }
    coilon[b] = on;
    nowon |= on;
  }
  (void)nowon;
}

// The RMT catches up with the clock: nOE goes high exactly at the end of the pulse
static void rmtupdate() {
  if (!rmtactive || rmtstuck) return;
  if (rmtlow && cycles >= rmtlowend) {
    uint64_t now = cycles;
    cycles = rmtlowend;
    pnoe = true;
    rmtlow = false;
    updatemodel();
    cycles = now;
  }
  if (!rmtlow && cycles >= rmtend) rmtactive = false;
}

bool pulsebegin(uint16_t on, uint16_t rest) {
  rmtowned = true;
  rmton = on;
  rmtrest = rest;
  return true;
}

// The driver call takes time before nOE really goes low: measured 42 us on the first call (code not
// in the cache yet) and about 13 us after that. The model starts the pulse at the end of the call.
static bool rmtfirstcall = true;
bool pulsestart() {
  cycles += (uint64_t)(rmtfirstcall ? 60 : 15) * MHZ;
  rmtfirstcall = false;
  rmtupdate();
  if (!rmtowned) return false;
  if (rmtactive) {
    rule5 = true;
    violation("puls gestart terwijl de vorige nog loopt");
  }
  rmtactive = rmtlow = true;
  rmtlowend = cycles + (uint64_t)rmton * MHZ;
  rmtend = rmtlowend + (uint64_t)rmtrest * MHZ;
  pnoe = false;
  updatemodel();
  return true;
}

bool pulsebusy() {
  cycles += 2;
  rmtupdate();
  return rmtactive;
}

void pulserelease() {
  cycles += 4;
  rmtowned = rmtactive = rmtlow = false;
  pnoe = true;
  updatemodel();
}

void setdata(bool high) { cycles += 4; rmtupdate(); pdata = high; }

void setsclk(bool high) {
  cycles += 4;
  rmtupdate();
  bool data = stuckdata || pdata;
  if (high && !psclk) {
    for (int b = hwboards - 1; b >= 0; b--) {
      for (int i = 47; i > 0; i--) sr[b][i] = sr[b][i - 1];
      sr[b][0] = b == 0 ? data : ff[b - 1];
    }
  } else if (!high && psclk) {
    for (int b = 0; b < hwboards; b++) ff[b] = sr[b][47];
  }
  psclk = high;
}

void setlatch(bool high) {
  cycles += 4;
  rmtupdate();
  if (high && !platch) {
    if (!pnoe) {
      rule1 = true;
      violation("LATCH terwijl nOE laag is");
    }
    memcpy(latched, sr, sizeof(sr));
    updatemodel();
  }
  platch = high;
}

void setnoe(bool high) {
  cycles += 4;
  rmtupdate();
  if (rmtowned) return;  // The pin belongs to the RMT
  pnoe = high;
  updatemodel();
}

void setdirx1(bool high) {
  cycles += 4;
  rmtupdate();
  if (high != pdirx1 && !pnoe) {
    rule1 = true;
    violation("DIR_X1 verandert terwijl nOE laag is");
  }
  pdirx1 = high;
  updatemodel();
}

void setdirrow(bool high) {
  cycles += 4;
  rmtupdate();
  if (high != pdirrow && !pnoe) {
    rule1 = true;
    violation("DIR_ROW verandert terwijl nOE laag is");
  }
  pdirrow = high;
  updatemodel();
}

bool readnoe() {
  cycles += 4;
  rmtupdate();
  return pnoe;
}

bool readreturn() {
  cycles += 4;
  rmtupdate();
  return loopback && ff[hwboards - 1];
}

uint32_t chaincycles() {
  cycles += 2;
  // An interrupt that somehow still got through during a pulse
  if (!pnoe && interruptat >= 0 && coilcount > interruptat) {
    cycles += 500 * MHZ;
    interruptat = -1;
  }
  rmtupdate();
  return cycles;
}

static bool alloff() {
  if (!pnoe) return false;
  for (int b = 0; b < hwboards; b++)
    for (int i = 0; i < 48; i++)
      if (latched[b][i]) return false;
  return true;
}

static bool matches() {
  for (int x = 0; x < COLS; x++)
    for (int y = 0; y < ROWS; y++) {
      int b = x / PANEL_WIDTH + (y / PANEL_HEIGHT) * PANELS_X;
      if (dots[b][x % PANEL_WIDTH][y % PANEL_HEIGHT] != (bool)next_state[x][y]) return false;
    }
  return true;
}

static void randomframe(int percent) {
  for (int x = 0; x < COLS; x++)
    for (int y = 0; y < ROWS; y++)
      if (rand() % 100 < percent) next_state[x][y] = !next_state[x][y];
}

// Power on: the 595s hold garbage, R1-R3 keep nOE high and DIR low
static void poweron(int boards, bool jp1) {
  hwboards = boards;
  loopback = jp1;
  stuckdata = false;
  for (int b = 0; b < HW_MAX; b++) {
    for (int i = 0; i < 48; i++) sr[b][i] = latched[b][i] = rand() & 1;
    ff[b] = rand() & 1;
  }
  pdata = psclk = platch = pdirx1 = pdirrow = false;
  pnoe = true;
  memset(coilon, 0, sizeof(coilon));
  violations = coilcount = 0;
  maxcoils = 0;
  rule1 = rule2 = false;
  interruptat = -1;
  rmtowned = rmtactive = rmtlow = rmtstuck = false;
  chainfault = false;
  chainboardsfound = -1;
  chainstats = { 0 };
}

static void start(int boards = CHAIN_BOARDS, bool jp1 = true) {
  poweron(boards, jp1);
  driverbegin();
  YellowScreen();
  ClearScreen();
}

static void check(const char *name, bool ok, int *failures) {
  printf("  %-62s %s\n", name, ok ? "goed" : "FOUT");
  if (!ok) (*failures)++;
}

int main() {
  int failures = 0;
  srand(1);

  printf("v3: normale werking met %d borden\n", CHAIN_BOARDS);
  poweron(CHAIN_BOARDS, true);
  driverbegin();
  check("Opstarten: borden geteld, geen storing", chainboardsfound == CHAIN_BOARDS && !chainfault, &failures);
  check("Opstarten: geen spoelstroom en alles leeg", coilcount == 0 && alloff(), &failures);
  YellowScreen();
  ClearScreen();
  long frames = 0, pulsesbefore = chainstats.pulses;
  uint64_t before = cycles;
  bool allmatch = true, offbetween = true;
  for (int percent : { 1, 5, 25, 50, 100 }) {
    for (int i = 0; i < 40; i++, frames++) {
      randomframe(percent);
      fullupdate();
      allmatch &= matches();
      offbetween &= alloff();
    }
  }
  double perpulse = (double)(cycles - before) / (chainstats.pulses - pulsesbefore) / MHZ;
  YellowScreen(); allmatch &= matches();
  ClearScreen();  allmatch &= matches();
  for (int x = 0; x < COLS; x++) for (int y = 0; y < ROWS; y++) {
    flipdot(x, y, 1); allmatch &= alloff(); flipdot(x, y, 0); offbetween &= alloff();
  }
  printf("  %ld beelden, %u pulsen\n", frames, chainstats.pulses);
  printf("  spoelpuls in het model: %.1f tot %.1f us, doel %d us\n", pulseshortest / (double)MHZ, pulselongest / (double)MHZ, PULSE_MICROS);
  printf("  kortste rust tussen pulsen: %.1f us, doel %d us\n", gapshortest / (double)MHZ, PULSE_GAP_MICROS);
  printf("  per stap (puls, rust, schuiven) gemiddeld %.0f us\n", perpulse);
  check("Regel 1: LATCH en DIR alleen met nOE hoog", !rule1, &failures);
  check("Regel 2: maximaal 1 spoel per bord", !rule2 && maxcoils == 1, &failures);
  check("Regel 3: alles uit na elke update", offbetween && alloff(), &failures);
  check("Regel 4: panelen tonen wat next_state zegt", allmatch, &failures);
  check("Regel 5: elke puls binnen 1 us van het doel", chainstats.pulses > 0 &&
        pulseshortest / (double)MHZ > PULSE_MICROS - 1 && pulselongest / (double)MHZ < PULSE_MICROS + 1, &failures);
  check("Regel 5: genoeg rust tussen pulsen", chainstats.pulses > 0 && gapshortest / (double)MHZ >= PULSE_GAP_MICROS, &failures);

  printf("Borden flippen tegelijk\n");
  start();
  memset(next_state, 1, sizeof(next_state));
  uint32_t pulses = chainstats.pulses;
  fullupdate();
  char text[100];
  snprintf(text, sizeof(text), "Heel scherm naar geel: %u pulsen, %d dots per bord",
           chainstats.pulses - pulses, PANEL_WIDTH * PANEL_HEIGHT);
  check(text, chainstats.pulses - pulses == PANEL_WIDTH * PANEL_HEIGHT && matches(), &failures);

  printf("Keten klopt niet: er flipt niets\n");
  struct { int boards; bool jp1; bool stuck; const char *name; } wrong[] = {
    { CHAIN_BOARDS, false, false, "JP1 open op het laatste bord" },
    { CHAIN_BOARDS + 1, true, false, "een bord meer dan ingesteld" },
    { CHAIN_BOARDS - 1, true, false, "een bord minder dan ingesteld" },
    { CHAIN_BOARDS, true, true, "datalijn zit vast op hoog" },
  };
  for (auto &w : wrong) {
    if (w.boards < 1) continue;
    poweron(w.boards, w.jp1);
    stuckdata = w.stuck;
    driverbegin();
    memset(next_state, 1, sizeof(next_state));
    fullupdate();
    flipdot(3, 3, 0);
    snprintf(text, sizeof(text), "%s: storing, geen spoelstroom", w.name);
    check(text, chainfault && coilcount == 0 && pnoe, &failures);
  }

  printf("Firmware staat stil of crasht tijdens een puls\n");
  start();
  pulseshortest = 0xFFFFFFFF;
  pulselongest = 0;
  interruptat = coilcount + 10;  // 500 us stil, midden in een puls
  memset(next_state, 1, sizeof(next_state));
  fullupdate();
  printf("  pulsen met 500 us stilstand: %.1f tot %.1f us\n", pulseshortest / (double)MHZ, pulselongest / (double)MHZ);
  check("500 us stilstand: puls blijft precies even lang, geen storing",
        pulseshortest / (double)MHZ > PULSE_MICROS - 1 && pulselongest / (double)MHZ < PULSE_MICROS + 1 && !chainfault && matches(), &failures);

  start();
  pulselongest = 0;
  {
    uint32_t pattern[CHAIN_WORDS] = { 0 };
    chainsetbit(pattern, 0, 0, 3);
    chainsetbit(pattern, 0, 4, 5);
    chainpulse(pattern, true);
  }
  cycles += 100000ULL * MHZ;  // The firmware never comes back
  rmtupdate();
  printf("  puls waarna de firmware nooit meer terugkomt: %.1f us\n", pulselongest / (double)MHZ);
  check("Crash na de start van een puls: de RMT beëindigt hem op tijd", pnoe && pulselongest / (double)MHZ < PULSE_MICROS + 1, &failures);

  printf("RMT hangt: de puls stopt niet vanzelf\n");
  start();
  pulselongest = 0;
  memset(next_state, 1, sizeof(next_state));
  {
    uint32_t pattern[CHAIN_WORDS] = { 0 };
    chainsetbit(pattern, 0, 0, 3);
    chainsetbit(pattern, 0, 4, 5);
    rmtstuck = true;
    chainpulse(pattern, true);
    long after = coilcount;
    fullupdate();
    flipdot(1, 1, 1);
    printf("  langste puls voordat de firmware nOE terugpakt: %.0f us\n", pulselongest / (double)MHZ);
    check("Die puls duurt niet langer dan PULSE_MICROS + 25 us", pulselongest / (double)MHZ < PULSE_MICROS + 25, &failures);
    check("Noodstop: nOE terug en hoog, alles uit, er flipt daarna niets meer", chainfault && alloff() && coilcount == after, &failures);
  }

  printf("Bewijs dat de controles werken\n");
  poweron(CHAIN_BOARDS, true);
  memset(latched, 0, sizeof(latched));
  latched[0][0] = latched[0][1] = latched[0][32] = true;
  pdirrow = true;
  pnoe = false;
  updatemodel();
  check("Twee spoelen tegelijk worden herkend", rule2, &failures);
  setlatch(true);
  check("LATCH met nOE laag wordt herkend", rule1, &failures);

  printf(failures ? "\nMISLUKT: %d fout(en)\n" : "\nAlles goed\n", failures);
  return failures != 0;
}
