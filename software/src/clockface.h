#pragma once

#include <time.h>

#include "flipsettings.h"

// Clock faces, drawn into next_state: big, time + date, analog, words, with seconds and binary. Time comes from NTP, CLOCK_TIMEZONE takes care of daylight saving time.

void clocksetup();  // Call after WiFi is connected

// Fonts of the word clock: 0 is the classic one, drawn on the 32x32 face and scaled up. The others come
// from wordfonts.h and are drawn at the resolution of the screen; they are only offered when every
// word fits.
uint8_t clockfonts();                // Number of fonts, available or not
const char *clockfontid(uint8_t font);
const char *clockfontname(uint8_t font);
bool clockfontfits(uint8_t font);
void clockdraw(const struct tm &now, ClockStyle style, bool seconds, uint8_t font = 0);
void clockdrawunsynced();  // Dashes until the first NTP sync
