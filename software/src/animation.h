#pragma once

#include <Arduino.h>

// An animation only draws its frames into next_state,
// the player takes care of flipping the dots and the timing.
struct Animation {
  const char *id;      // Short name for settings and the web API
  const char *name;
  void (*setup)();     // The screen is cleared before setup
  void (*step)();      // Draw the next frame into next_state
  uint16_t frametime;  // in ms at 100% speed
};

// Every animation file defines one or more of these
extern const Animation golAnimation;
extern const Animation highlifeAnimation;
extern const Animation daynightAnimation;
extern const Animation seedsAnimation;
extern const Animation rainAnimation;
extern const Animation automatonAnimation;
extern const Animation plasmaAnimation;
extern const Animation cubeAnimation;
extern const Animation mandelbrotAnimation;
extern const Animation ballsAnimation;
extern const Animation mazeAnimation;
extern const Animation langtonAnimation;
extern const Animation sandAnimation;
extern const Animation starfieldAnimation;
extern const Animation crystalAnimation;
extern const Animation chaosAnimation;
extern const Animation truchetAnimation;
extern const Animation spirographAnimation;
extern const Animation hilbertAnimation;
extern const Animation boidsAnimation;
extern const Animation fireworksAnimation;
extern const Animation pendulumAnimation;
extern const Animation snowAnimation;
extern const Animation lightningAnimation;
extern const Animation metaballsAnimation;
extern const Animation clocklifeAnimation;

// All animations, in the order they are played. See animations.cpp.
#define ANIMATION_COUNT 26
extern const Animation *const animations[ANIMATION_COUNT];

int8_t findanimation(const char *id);  // -1 if there is no animation with this id
