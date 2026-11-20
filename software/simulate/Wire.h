#pragma once

#include <stdint.h>

// Fake bus, see simulate.cpp: it feeds every finished write to the hardware model.
struct TwoWire {
  uint8_t address;
  uint8_t data[4];
  uint8_t length;
  void beginTransmission(uint8_t a) { address = a; length = 0; }
  void write(uint8_t b) { if (length < 4) data[length++] = b; }
  uint8_t endTransmission();
};
extern TwoWire Wire;
