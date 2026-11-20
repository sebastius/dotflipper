#include <Preferences.h>

#include "flipsettings.h"
#include "clockface.h"

Settings settings;

static const char *const clockstyles[CLOCK_STYLES] = { "big", "date", "analog", "words", "seconds", "binary" };
static const char *const transitions[TRANSITIONS] = { "none", "dissolve", "fall", "sand" };
static const uint8_t intervals[] = { 0, 5, 10, 15, 20, 30, 60 };  // Divisors of 60, so breaks line up with the hour

static void defaultsettings() {
  settings.power = true;
  settings.clockEnabled = true;
  settings.clockStyle = CLOCK_BIG;
  settings.clockSeconds = false;
  settings.clockFont = 0;
  settings.clockTransition = TRANSITION_NONE;
  settings.interval = 15;
  settings.animationsPerBreak = 1;
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    settings.animation[i] = { true, 100, 30 };
  }
}

void settingstojson(JsonObject json) {
  json["power"] = settings.power;

  JsonObject clock = json["clock"].to<JsonObject>();
  clock["enabled"] = settings.clockEnabled;
  clock["style"] = clockstyles[settings.clockStyle];
  clock["seconds"] = settings.clockSeconds;
  clock["font"] = clockfontid(settings.clockFont);
  clock["transition"] = transitions[settings.clockTransition];
  clock["interval"] = settings.interval;
  clock["animations_per_break"] = settings.animationsPerBreak;

  JsonObject list = json["animations"].to<JsonObject>();
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    JsonObject a = list[animations[i]->id].to<JsonObject>();
    a["enabled"] = settings.animation[i].enabled;
    a["speed"] = settings.animation[i].speed;
    a["duration"] = settings.animation[i].duration;
  }
}

void settingsfromjson(JsonObjectConst json) {
  if (json["power"].is<bool>()) {
    settings.power = json["power"];
  }

  JsonObjectConst clock = json["clock"];
  if (clock["enabled"].is<bool>()) {
    settings.clockEnabled = clock["enabled"];
  }
  if (clock["style"].is<const char *>()) {
    for (uint8_t i = 0; i < CLOCK_STYLES; i++) {
      if (strcmp(clock["style"], clockstyles[i]) == 0) {
        settings.clockStyle = (ClockStyle)i;
      }
    }
  }
  if (clock["seconds"].is<bool>()) {
    settings.clockSeconds = clock["seconds"];
  }
  if (clock["font"].is<const char *>()) {
    for (uint8_t i = 0; i < clockfonts(); i++) {
      if (strcmp(clock["font"], clockfontid(i)) == 0 && clockfontfits(i)) {
        settings.clockFont = i;
      }
    }
  }
  if (clock["transition"].is<const char *>()) {
    for (uint8_t i = 0; i < TRANSITIONS; i++) {
      if (strcmp(clock["transition"], transitions[i]) == 0) {
        settings.clockTransition = (Transition)i;
      }
    }
  }
  if (clock["interval"].is<int>()) {
    for (uint8_t interval : intervals) {
      if (clock["interval"] == interval) {
        settings.interval = interval;
      }
    }
  }
  if (clock["animations_per_break"].is<int>()) {
    settings.animationsPerBreak = constrain(clock["animations_per_break"].as<int>(), 1, 10);
  }

  for (JsonPairConst pair : json["animations"].as<JsonObjectConst>()) {
    int8_t i = findanimation(pair.key().c_str());
    if (i < 0) {
      continue;
    }
    JsonObjectConst a = pair.value();
    if (a["enabled"].is<bool>()) {
      settings.animation[i].enabled = a["enabled"];
    }
    if (a["speed"].is<int>()) {
      settings.animation[i].speed = constrain(a["speed"].as<int>(), 25, 400);
    }
    if (a["duration"].is<int>()) {
      settings.animation[i].duration = constrain(a["duration"].as<int>(), 5, 600);
    }
  }
}

void loadsettings() {
  defaultsettings();

  Preferences preferences;
  preferences.begin("flipdot", true);
  String stored = preferences.getString("settings", "");
  preferences.end();

  JsonDocument doc;
  if (stored.length() && !deserializeJson(doc, stored)) {
    settingsfromjson(doc.as<JsonObjectConst>());
  }
}

void savesettings() {
  JsonDocument doc;
  settingstojson(doc.to<JsonObject>());
  String json;
  serializeJson(doc, json);

  Preferences preferences;
  preferences.begin("flipdot", false);
  preferences.putString("settings", json);
  preferences.end();
}
