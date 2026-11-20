#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "animation.h"

// Settings that can be changed from the web interface, stored in flash as JSON.
//
// {
//   "power": true,
//   "clock": { "enabled": true, "style": "big", "seconds": false, "font": "classic", "transition": "none",
//              "interval": 15, "animations_per_break": 1 },
//   "animations": { "gol": { "enabled": true, "speed": 100, "duration": 30 }, ... }
// }
//
// Every field is optional when changing settings, missing fields keep their value.

enum ClockStyle { CLOCK_BIG, CLOCK_DATE, CLOCK_ANALOG, CLOCK_WORDS, CLOCK_SECONDS, CLOCK_BINARY };
#define CLOCK_STYLES 6

enum Transition { TRANSITION_NONE, TRANSITION_DISSOLVE, TRANSITION_FALL, TRANSITION_SAND };
#define TRANSITIONS 4

struct AnimationSettings {
  bool enabled;
  uint16_t speed;     // in percent, 100 is the normal speed
  uint16_t duration;  // in seconds
};

struct Settings {
  bool power;               // Off leaves the screen black
  bool clockEnabled;        // Off plays animations all the time
  ClockStyle clockStyle;
  bool clockSeconds;        // Second hand on the analog clock
  uint8_t clockFont;        // Font of the word clock, see clockface.h; 0 is the classic one
  Transition clockTransition;  // When the minute changes
  uint8_t interval;         // Minutes between animation breaks, aligned to the hour. 0 is clock only
  uint8_t animationsPerBreak;
  AnimationSettings animation[ANIMATION_COUNT];
};

extern Settings settings;

void loadsettings();
void savesettings();
void settingstojson(JsonObject json);
void settingsfromjson(JsonObjectConst json);
