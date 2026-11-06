#pragma once

// Just enough of Arduino to build the real display and PCA9685 code on a PC.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t byte;
typedef bool boolean;

#define HEX 16
#define DEC 10

inline void delay(unsigned long) {}
inline void yield() {}
inline unsigned long millis() { return 0; }
void delayMicroseconds(unsigned int us);

struct FakeSerial {
  template <typename T> void print(T) {}
  template <typename T> void print(T, int) {}
  template <typename T> void println(T) {}
  template <typename T> void println(T, int) {}
};
extern FakeSerial Serial;

struct FakeESP {
  uint32_t getCycleCount();
};
extern FakeESP ESP;
