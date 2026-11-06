#include "display.h"
#include "pca9685.h"

boolean current_state[COLS][ROWS];
boolean next_state[COLS][ROWS];

void flipdot(uint16_t x, uint16_t y, bool color) {
  // This routine seems a bit convoluted, calculating the addresses for each pixel when you could make a map beforehand.
  // However the math takes up < 1ms for 8 panels in total (64x64 sets of calculations) on an 80mhz ESP8266.

  // Input sanitation. Out of range? We wrap around.
  // Sanitation is needed because this panel is headed for hacker-events...
  // Stuff will break (actual short circuits, blown chips, coils and diodes) when addressing is wrong.
  x %= COLS;
  y %= ROWS;

  // Quick mafs
  // Find out what panel we're on
  byte xpanelcoordinate = x / PANEL_WIDTH;
  byte ypanelcoordinate = y / PANEL_HEIGHT;
  byte panelnumber = xpanelcoordinate + (ypanelcoordinate * PANELS_X);

  // Determine which drivers we want to speak to
  byte columndriveraddress = 0x40 + ((panelnumber * 4) + ((x % PANEL_WIDTH)  / 8));
  byte rowdriveraddress    = 0x60 + ((panelnumber * 2) + ((y % PANEL_HEIGHT) / 8));

  // Now we determine what column and row we're on (%8) and determine which register matches
  // If color is HIGH then we need the column to be at positive rail and row at gnd-rail.
  // If color is LOW then we need the row to be at positive rail and column at gnd-rail.
  // This is done in hardware by using the first 8 outputs of the chip for the gnd-rail and the second 8 for the positive rail.
  byte column = x % 8;
  byte columnregisteroffset = (color) ? 0x07 : 0x27;
  byte columnregister = 4 * column + columnregisteroffset;

  byte row = y % 8;
  byte rowregisteroffset = (color) ? 0x27 : 0x07;
  byte rowregister =  4 * row + rowregisteroffset;

  // Now we will flip some bits!
  // Powerup!
  i2cwrite(rowdriveraddress,    rowregister          , 0x10);
  delayMicroseconds(100);
  i2cwrite(columndriveraddress, columnregister       , 0x10);
  delayMicroseconds(100);

  // And powerdown!
  i2cwrite(rowdriveraddress,    rowregister          , 0x00);
  delayMicroseconds(100);
  i2cwrite(columndriveraddress, columnregister       , 0x00);
  delayMicroseconds(100);
}

static void countpulse(uint32_t cycles) {
  i2cstats.pulses++;
  i2cstats.pulsetotal += cycles;
  if (cycles > i2cstats.pulsemax) {
    i2cstats.pulsemax = cycles;
  }
}

// Flips every changed dot from current_state to next_state, two panels at a time.
// Only works on two panels stacked on top of each other!
void fullupdate() {
#ifdef DEBUG
  unsigned long starttijd = millis();
#endif

  for (uint8_t x = 0; x < 32; x++) {
    for (uint8_t y = 0; y < 16; y++) {

      if ((current_state[x][y] != next_state[x][y]) || (current_state[x][y + 16] != next_state[x][y + 16])) {

        // Determine which drivers we want to speak to
        byte columndriveraddress = 0x40 + (x / 8);
        byte rowdriveraddress    = 0x60 + (y / 8);
        byte column2driveraddress = 0x44 + (x / 8);
        byte row2driveraddress    = 0x62 + (y / 8);

        byte column = x % 8;

        byte columnregisteroffset = (next_state[x][y]) ? 0x07 : 0x27;
        byte column2registeroffset = (next_state[x][y + 16]) ? 0x07 : 0x27;

        byte columnregister = 4 * column + columnregisteroffset;
        byte column2register = 4 * column + column2registeroffset;

        byte row = y % 8;
        byte rowregisteroffset = (next_state[x][y]) ? 0x27 : 0x07;
        byte row2registeroffset = (next_state[x][y + 16]) ? 0x27 : 0x07;

        byte rowregister =  4 * row + rowregisteroffset;
        byte row2register =  4 * row + row2registeroffset;

        // Now we will flip some bits!
        // Powerup!
        i2cwrite(rowdriveraddress,    rowregister          , 0x10);
        i2cwrite(row2driveraddress,    row2register          , 0x10);

        delayMicroseconds(1);

        i2cwrite(columndriveraddress, columnregister       , 0x10);
        uint32_t pulsestart = ESP.getCycleCount();
        i2cwrite(column2driveraddress, column2register       , 0x10);
        uint32_t pulse2start = ESP.getCycleCount();

        delayMicroseconds(1);

        // And powerdown!
        i2cwrite(rowdriveraddress,    rowregister          , 0x00);
        uint32_t pulseend = ESP.getCycleCount();
        i2cwrite(row2driveraddress,    row2register          , 0x00);
        uint32_t pulse2end = ESP.getCycleCount();

        delayMicroseconds(1);

        i2cwrite(columndriveraddress, columnregister       , 0x00);
        i2cwrite(column2driveraddress, column2register       , 0x00);

        delayMicroseconds(1);

        countpulse(pulseend - pulsestart);
        countpulse(pulse2end - pulse2start);
      }
    }
  }
#ifdef DEBUG
  unsigned long eindtijd = millis();
  Serial.println(eindtijd - starttijd);
#endif
  memcpy(current_state, next_state, sizeof(next_state));
}

// Sets a dot in next_state, dots outside the screen are ignored.
void setdot(int16_t x, int16_t y, bool on) {
  if (x >= 0 && x < COLS && y >= 0 && y < ROWS) {
    next_state[x][y] = on;
  }
}

// Draws a line into next_state (Bresenham), dots outside the screen are ignored.
void drawline(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on) {
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  while (true) {
    setdot(x0, y0, on);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int16_t e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

// Flips every dot to next_state, also the ones that should already be right.
void forcefullupdate() {
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      current_state[x][y] = !next_state[x][y];
    }
  }
  fullupdate();
}

void ClearScreen() {
  memset(current_state, 1, sizeof(current_state));
  memset(next_state, 0, sizeof(next_state));
  fullupdate();
}

void YellowScreen() {
  memset(current_state, 0, sizeof(current_state));
  memset(next_state, 1, sizeof(next_state));
  fullupdate();
}

void pixelwalker() {
  for (uint8_t x = 0; x < COLS; x++) {
    for (uint8_t y = 0; y < ROWS; y++) {
      flipdot(x, y, 1);
      delay(50);
      flipdot(x, y, 0);
    }
  }
}

// picture is stored as [row][column]
void showpicture(const boolean picture[32][32]) {
  for (uint8_t x = 0; x < 32; x++) {
    for (uint8_t y = 0; y < 32; y++) {
      next_state[y][x] = picture[x][y];
    }
  }
  fullupdate();
}
