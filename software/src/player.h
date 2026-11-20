#pragma once

#include <Arduino.h>

// Decides what is on the screen, following the settings:
//  - power off: black screen
//  - clock on, interval > 0: the clock, and every interval minutes (aligned to the hour)
//    a break of animationsPerBreak animations
//  - clock on, interval 0: only the clock
//  - clock off: the enabled animations one after another
// Animations play for their own duration, in the order of the animations list.

void playersetup();
void playerloop();
void playerreload();               // Call after the settings changed
void playerpreview(uint8_t index); // Play one animation now, then go back to the schedule
String playerstatus();             // What is on the screen right now
