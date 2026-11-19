#pragma once

#include <stdint.h>

// The chain pins of the simulated v3 boards, see simulate595.cpp.
void setdata(bool high);
void setsclk(bool high);
void setlatch(bool high);
void setnoe(bool high);
void setdirx1(bool high);
void setdirrow(bool high);
bool readreturn();
uint32_t chaincycles();
