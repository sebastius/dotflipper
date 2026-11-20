#pragma once

#include <stdint.h>

// The I2C pins of the simulated board, see simulate.cpp.
void sdalow();
void sdarelease();
void scllow();
void sclrelease();
bool sdaread();
bool sclread();
uint32_t i2ccycles();
