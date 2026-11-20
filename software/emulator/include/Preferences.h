#pragma once

// Settings survive a reload of the emulator page: they live in the localStorage of the browser.

#include "Arduino.h"

String emuload(const char *key);
void emusave(const char *key, const char *value);

class Preferences {
 public:
  bool begin(const char *name, bool = false) { space = name; return true; }
  void end() {}
  String getString(const char *key, const String &fallback = String()) {
    String value = emuload((space + "." + key).c_str());
    return value.isEmpty() ? fallback : value;
  }
  size_t putString(const char *key, const String &value) {
    emusave((space + "." + key).c_str(), value.c_str());
    return value.length();
  }

 private:
  String space;
};
