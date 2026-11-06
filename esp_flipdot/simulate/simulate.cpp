// Runs the real display.cpp and pca9685.cpp against a model of the flipdot hardware.
// See run.sh. Every I2C write is logged and checked, because a wrong write can blow up the board.
//
// Model, from the schematic and measurements on the panel:
//  - CH0-7 of a chip drive the ULN2803 (to ground), CH8-15 the UDN2982 (to 20 V)
//  - 32-side: both drivers on the same line. 16-side: separate wires, joined by the BAV99 of every dot
//  - dot coil between the middle of its BAV99 and its 32-side line
//
// Rules:
//  1. Never CHn and CHn+8 of a chip on together: a short from 20 V to ground
//  2. At most one coil per panel carries current
//  3. Everything is off between dots and after an update
//  4. After an update every dot on the panels matches next_state

#include <stdio.h>
#include <vector>

#include "Arduino.h"
#include "Wire.h"
#include "display.h"
#include "pca9685.h"

FakeSerial Serial;
FakeESP ESP;
TwoWire Wire;

static FILE *trace;
static uint32_t cycles;

// Fault injection
static long dropwrite = -1;     // Number of the first LED write that gets lost
static long droplength = 1;
static uint8_t dropresult = 2;
static long ledwrites;
static int deadchip = -1;       // Chip that stopped working altogether

uint32_t FakeESP::getCycleCount() { return cycles; }
void delayMicroseconds(unsigned int us) {
  cycles += us * 240;
  if (trace) fprintf(trace, "wait %u\n", us);
}

static bool outputs[0x80][16];
static bool fulloff[0x80];
static bool dots[PANELS_Y][32][16];
static long violations;
static int coilsnow[PANELS_Y];
static int maxcoils;
static bool rule1, rule2;

static void violation(const char *text) {
  if (violations++ < 5) printf("    OVERTREDING: %s\n", text);
}

static bool active(int chip, int channel) {
  return outputs[chip][channel] && !fulloff[chip];
}

static bool chipexists(uint8_t a) {
  return (a >= 0x40 && a < 0x40 + 4 * PANELS_Y) || (a >= 0x60 && a < 0x60 + 2 * PANELS_Y) || a == 0x70;
}

static void updatemodel() {
  for (int a = 0x40; a < 0x70; a++) {
    for (int ch = 0; ch < 8; ch++) {
      if (active(a, ch) && active(a, ch + 8)) {
        rule1 = true;
        violation("CHn en CHn+8 tegelijk aan: kortsluiting");
      }
    }
  }
  for (int p = 0; p < PANELS_Y; p++) {
    int coils = 0;
    for (int x = 0; x < 32; x++) {
      uint8_t column = 0x40 + p * 4 + x / 8;
      for (int y = 0; y < 16; y++) {
        uint8_t row = 0x60 + p * 2 + y / 8;
        bool yellow = active(row, 8 + y % 8) && active(column, x % 8);
        bool black  = active(row, y % 8) && active(column, 8 + x % 8);
        if (yellow) dots[p][x][y] = true;
        if (black)  dots[p][x][y] = false;
        coils += yellow || black;
      }
    }
    if (coils > maxcoils) maxcoils = coils;
    coilsnow[p] = coils;
    if (coils > 1) {
      rule2 = true;
      violation("meer dan 1 spoel tegelijk op een paneel");
    }
  }
}

uint8_t TwoWire::endTransmission() {
  cycles += 40 * 240;
  if (!chipexists(address) || address == deadchip) return 2;
  if (length != 2) return 0;
  uint8_t reg = data[0], value = data[1];
  bool led = reg >= 0x07 && reg <= 0x43 && (reg - 7) % 4 == 0;
  if (trace) fprintf(trace, "write %02X %02X %02X\n", address, reg, value);
  if (led) {
    long number = ledwrites++;
    if (dropwrite >= 0 && number >= dropwrite && number < dropwrite + droplength) return dropresult;
    outputs[address][(reg - 7) / 4] = value == 0x10;
  } else if (reg == 0xFD && value == 0x10) {
    if (address == 0x70) {
      for (int a = 0x40; a < 0x70; a++) if (a != deadchip) fulloff[a] = true;
    } else {
      fulloff[address] = true;
    }
  } else if (reg == 0xFD || reg == 0xFB) {
    memset(outputs[address], 0, 16);
    fulloff[address] = false;
  }
  updatemodel();
  return 0;
}

static bool alloff() {
  for (int a = 0x40; a < 0x70; a++)
    for (int ch = 0; ch < 16; ch++)
      if (active(a, ch)) return false;
  return true;
}

static bool matches() {
  for (int x = 0; x < COLS; x++)
    for (int y = 0; y < ROWS; y++)
      if (dots[y / 16][x][y % 16] != (bool)next_state[x][y]) return false;
  return true;
}

static void randomframe(int percent) {
  for (int x = 0; x < COLS; x++)
    for (int y = 0; y < ROWS; y++)
      if (rand() % 100 < percent) next_state[x][y] = !next_state[x][y];
}

static void start() {
  memset(outputs, 0, sizeof(outputs));
  memset(fulloff, 0, sizeof(fulloff));
  violations = 0; maxcoils = 0; rule1 = rule2 = false;
  ledwrites = 0; dropwrite = -1; droplength = 1; deadchip = -1;
#ifdef HAS_FAULT
  i2cfault = false;
#endif
  resetPCA9685();
  YellowScreen();
  ClearScreen();
}

static void check(const char *name, bool ok, int *failures) {
  printf("  %-58s %s\n", name, ok ? "goed" : "FOUT");
  if (!ok) (*failures)++;
}

int main(int argc, char **argv) {
  int failures = 0;
  trace = argc > 1 ? fopen(argv[1], "w") : nullptr;

  printf("Normale werking\n");
  srand(1);
  start();
  long frames = 0;
  bool allmatch = true, offbetween = true;
  for (int percent : { 1, 5, 25, 50, 100 }) {
    for (int i = 0; i < 40; i++, frames++) {
      randomframe(percent);
      fullupdate();
      allmatch &= matches();
      offbetween &= alloff();
    }
  }
  YellowScreen(); allmatch &= matches();
  ClearScreen();  allmatch &= matches();
  for (int x = 0; x < COLS; x++) for (int y = 0; y < ROWS; y++) { flipdot(x, y, 1); allmatch &= alloff(); flipdot(x, y, 0); }
  printf("  %ld beelden, %ld LED-writes\n", frames, ledwrites);
  check("Regel 1: nooit CHn en CHn+8 samen", !rule1, &failures);
  check("Regel 2: maximaal 1 spoel per paneel", !rule2 && maxcoils == 1, &failures);
  check("Regel 3: alles uit na elke update", offbetween && alloff(), &failures);
  check("Regel 4: panelen tonen wat next_state zegt", allmatch, &failures);
  if (trace) { fclose(trace); trace = nullptr; }

#ifdef HAS_STATS
  check("Geen fouten geteld in normale werking", i2cstats.nack + i2cstats.timeout + i2cstats.other == 0, &failures);
  check("Pulsen geteld", i2cstats.pulses > 0, &failures);
  printf("  %u pulsen, langste %u cycles\n", i2cstats.pulses, i2cstats.pulsemax);

  printf("Teller na reset\n");
  start();
  resetPCA9685();
  check("Lege adressen tellen niet mee na resetPCA9685()", i2cstats.nack == 0 && i2cstats.writes == 0, &failures);
#endif

  printf("Bewijs dat de controles werken\n");
  start();
  outputs[0x40][0] = outputs[0x40][8] = true;
  updatemodel();
  check("Kortsluiting wordt herkend", rule1, &failures);

  start();
  Wire.beginTransmission(0x60); Wire.write(0x27); Wire.write(0x10); Wire.endTransmission();
  Wire.beginTransmission(0x40); Wire.write(0x07); Wire.write(0x10); Wire.endTransmission();
  Wire.beginTransmission(0x40); Wire.write(0x0B); Wire.write(0x10); Wire.endTransmission();
  check("Twee spoelen tegelijk worden herkend", rule2, &failures);

#ifdef HAS_FAULT
  printf("Elke write van een puls mislukt een keer\n");
  const char *names[8] = { "rij aan", "rij 2 aan", "kolom aan", "kolom 2 aan", "rij uit", "rij 2 uit", "kolom uit", "kolom 2 uit" };
  for (int position = 0; position < 8; position++) {
    start();
    violations = 0;
    long base = ledwrites;
    for (int x = 0; x < COLS; x++) for (int y = 0; y < ROWS; y++) next_state[x][y] = true;
    dropwrite = base + position;
    fullupdate();
    bool safe = alloff() && !rule1 && !rule2 && !i2cfault;
    fullupdate();  // A failed dot is flipped again by the next update
    char text[80];
    snprintf(text, sizeof(text), "%s: veilig, geen noodstop, beeld hersteld", names[position]);
    check(text, safe && matches(), &failures);
  }

  printf("Blijvende storing\n");
  for (int position = 4; position < 8; position++) {
    start();
    long base = ledwrites;
    for (int x = 0; x < COLS; x++) for (int y = 0; y < ROWS; y++) next_state[x][y] = true;
    dropwrite = base + position;
    droplength = 4;  // First try and all the retries
    fullupdate();
    long after = ledwrites;
    fullupdate();
    flipdot(1, 1, 1);
    char text[80];
    snprintf(text, sizeof(text), "%s lukt niet: noodstop, alles uit, er flipt niets meer", names[position]);
    check(text, i2cfault && alloff() && ledwrites == after && coilsnow[0] == 0 && coilsnow[1] == 0, &failures);
  }

  printf("Een chip valt helemaal uit\n");
  for (int chip : { 0x60, 0x40 }) {
    start();
    for (int x = 0; x < COLS; x++) for (int y = 0; y < ROWS; y++) next_state[x][y] = true;
    Wire.beginTransmission(chip == 0x60 ? 0x60 : 0x40);
    Wire.write(chip == 0x60 ? 0x27 : 0x07); Wire.write(0x10);
    Wire.endTransmission();  // The chip dies with this output on
    deadchip = chip;
    fullupdate();
    char text[80];
    snprintf(text, sizeof(text), "Chip 0x%02X dood met uitgang aan: noodstop, geen spoelstroom", chip);
    check(text, i2cfault && coilsnow[0] == 0 && coilsnow[1] == 0, &failures);
  }
#else
  start();
  for (int x = 0; x < COLS; x++) for (int y = 0; y < ROWS; y++) next_state[x][y] = true;
  dropwrite = ledwrites + 4;
  fullupdate();
#endif

  printf(failures ? "\nMISLUKT: %d fout(en)\n" : "\nAlles goed\n", failures);
  return failures != 0;
}
