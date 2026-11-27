#pragma once

#include <Arduino.h>

#include "config.h"

//  Theory of operation to flip a dot
//   - Column positive voltage + row gnd = BLACK dot
//   - Column gnd + row positive voltage = YELLOW dot
//
//   To get a black dot:  Column driven by UDN2981 and Row driven by ULN2803
//   To get a yellow dot: Column driven by ULN2803 and Row driven by UDN2981

// Framebuffers
extern boolean current_state[COLS][ROWS];
extern boolean next_state[COLS][ROWS];

// Hardware, v1 or v3 (see config.h)
void driverbegin();  // First thing at startup: every coil off
bool driverfault();  // True after an emergency stop: nothing flips until a restart

void flipdot(uint16_t x, uint16_t y, bool color);
void setdot(int16_t x, int16_t y, bool on);
void drawline(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on);
void fullupdate();
void forcefullupdate();
void ClearScreen();
void YellowScreen();
void pixelwalker();
void showpicture(const boolean picture[32][32]);

#ifdef HARDWARE_V3
// Test: flips one dot while sampling the coil current on IO4, see display595.cpp
uint16_t measureflip(uint16_t x, uint16_t y, bool color, uint16_t *us, uint16_t *mv, uint16_t max, uint16_t *baseline);
#endif
