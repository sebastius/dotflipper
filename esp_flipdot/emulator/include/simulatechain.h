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
uint32_t chaincycles();
