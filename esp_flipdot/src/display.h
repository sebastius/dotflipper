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

void flipdot(uint16_t x, uint16_t y, bool color);
void setdot(int16_t x, int16_t y, bool on);
void drawline(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool on);
void fullupdate();
void forcefullupdate();
void ClearScreen();
void YellowScreen();
void pixelwalker();
void showpicture(const boolean picture[32][32]);
