#pragma once

// Just enough of Arduino to build the real display and PCA9685 code on a PC.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>

typedef uint8_t byte;
typedef bool boolean;

#define HEX 16
#define DEC 10
#define INPUT 1
#define OUTPUT 3
#define IRAM_ATTR

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL(mux)
inline uint32_t getCpuFrequencyMhz() { return 240; }
inline void pinMode(int, int) {}

inline void delay(unsigned long) {}
inline void yield() {}
inline unsigned long millis() { return 0; }
inline unsigned long micros() { return 0; }
inline uint32_t analogReadMilliVolts(int) { return 0; }  // No coil current in the model
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
