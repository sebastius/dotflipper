#pragma once

#include <stdint.h>

// The chain pins of the emulated v3 boards, see hardware.cpp.
void setdata(bool high);
void setsclk(bool high);
void setlatch(bool high);
void setnoe(bool high);
void setdirx1(bool high);
void setdirrow(bool high);
bool readreturn();
bool readnoe();  // The level on the nOE pin
uint32_t chaincycles();

// The RMT on nOE: one pulse of on us with nOE low, then rest us high, timed by the model
bool pulsebegin(uint16_t on, uint16_t rest);
bool pulsestart(uint16_t on);  // This pulse: on us
bool pulsebusy();
void pulserelease();  // nOE back to the CPU, held high
