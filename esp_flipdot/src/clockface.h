#pragma once

#include <time.h>

#include "flipsettings.h"

// Clock faces, drawn into next_state: big, time + date, analog, words, with seconds and binary. Time comes from NTP, CLOCK_TIMEZONE takes care of daylight saving time.

void clocksetup();  // Call after WiFi is connected
void clockdraw(const struct tm &now, ClockStyle style, bool seconds);
void clockdrawunsynced();  // Dashes until the first NTP sync
