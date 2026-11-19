#pragma once

// Just enough of Arduino and the ESP32 to run the firmware in a browser. Time is virtual: it is the
// cycle counter of the emulated ESP32-S3 at 240 MHz, and it only moves when the firmware waits,
// toggles a pin or finishes a pass of loop(). See hardware.cpp.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <initializer_list>
#include <string>

typedef uint8_t byte;
typedef bool boolean;

#define HEX 16
#define DEC 10
#define LOW 0
#define HIGH 1
#define INPUT 1
#define OUTPUT 3
#define INPUT_PULLUP 5
#define IRAM_ATTR
#define PROGMEM
#define F(s) (s)
#define PI 3.14159265358979323846
#define HALF_PI 1.57079632679489661923
#define TWO_PI 6.28318530717958647693
#define DEG_TO_RAD 0.017453292519943295769
#define RAD_TO_DEG 57.295779513082320876798

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL(mux)

using std::max;
using std::min;
template <typename T, typename L, typename H> T constrain(T x, L low, H high) {
  return x < low ? low : (x > high ? high : x);
}
inline long map(long x, long inmin, long inmax, long outmin, long outmax) {
  return (x - inmin) * (outmax - outmin) / (inmax - inmin) + outmin;
}

// Time
uint64_t emucycles();
void emuadvance(uint64_t cycles);
inline uint32_t getCpuFrequencyMhz() { return 240; }
inline unsigned long millis() { return emucycles() / 240000; }
inline unsigned long micros() { return emucycles() / 240; }
inline void delay(unsigned long ms) { emuadvance((uint64_t)ms * 240000); }
inline void delayMicroseconds(unsigned int us) { emuadvance((uint64_t)us * 240); }
inline void yield() {}
bool getLocalTime(struct tm *info, uint32_t ms = 5000);
void configTzTime(const char *tz, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);

// Pins: the chain pins are in hardware.cpp, everything else reads high (no button pressed)
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void digitalWrite(int, int) {}

// Random, the same as on the ESP32: random(max) and random(min, max), max exclusive
inline void randomSeed(unsigned long seed) { srand(seed); }
inline long random(long howbig) { return howbig <= 0 ? 0 : rand() % howbig; }
inline long random(long howsmall, long howbig) { return howsmall >= howbig ? howsmall : howsmall + random(howbig - howsmall); }

class String {
 public:
  String() {}
  String(const char *s) { if (s) text = s; }
  String(const std::string &s) : text(s) {}
  String(int value) : text(std::to_string(value)) {}
  String(unsigned int value) : text(std::to_string(value)) {}
  String(long value) : text(std::to_string(value)) {}
  String(unsigned long value) : text(std::to_string(value)) {}
  String &operator=(const char *s) { text = s ? s : ""; return *this; }
  bool concat(const char *s) { if (s) text += s; return true; }
  String &operator+=(const String &s) { text += s.text; return *this; }
  String &operator+=(const char *s) { if (s) text += s; return *this; }
  String &operator+=(char c) { text += c; return *this; }
  friend String operator+(String a, const String &b) { a += b; return a; }
  friend String operator+(String a, const char *b) { a += b; return a; }
  friend String operator+(const char *a, const String &b) { return String(a) + b; }
  bool operator==(const String &s) const { return text == s.text; }
  bool operator==(const char *s) const { return text == (s ? s : ""); }
  bool operator!=(const String &s) const { return text != s.text; }
  const char *c_str() const { return text.c_str(); }
  unsigned int length() const { return text.size(); }
  bool isEmpty() const { return text.empty(); }
  char operator[](unsigned int i) const { return i < text.size() ? text[i] : 0; }
  int toInt() const { return atoi(text.c_str()); }

 private:
  std::string text;
};

class EmuSerial {
 public:
  void begin(unsigned long) {}
  template <typename T> void print(T value) { out(String(value)); }
  template <typename T> void print(T value, int) { out(String(value)); }
  template <typename T> void println(T value) { out(String(value)); out("\n"); }
  template <typename T> void println(T value, int) { out(String(value)); out("\n"); }
  void println() { out("\n"); }
  template <typename... A> void printf(const char *format, A... args) {
    char text[256];
    snprintf(text, sizeof(text), format, args...);
    out(text);
  }

 private:
  void out(const String &s) { fputs(s.c_str(), stdout); }
};
template <> inline void EmuSerial::print(const char *value) { out(value); }
template <> inline void EmuSerial::print(char *value) { out(value); }
template <> inline void EmuSerial::print(String value) { out(value); }
template <> inline void EmuSerial::println(const char *value) { out(value); out("\n"); }
template <> inline void EmuSerial::println(char *value) { out(value); out("\n"); }
template <> inline void EmuSerial::println(String value) { out(value); out("\n"); }
template <> inline void EmuSerial::print(double value) { char t[32]; snprintf(t, sizeof(t), "%.2f", value); out(t); }
template <> inline void EmuSerial::println(double value) { print(value); out("\n"); }
template <> inline void EmuSerial::print(float value) { print((double)value); }
template <> inline void EmuSerial::println(float value) { println((double)value); }
template <> inline void EmuSerial::print(char value) { char t[2] = { value, 0 }; out(t); }
template <> inline void EmuSerial::println(char value) { print(value); out("\n"); }
extern EmuSerial Serial;
