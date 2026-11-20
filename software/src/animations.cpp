#include "animation.h"

const Animation *const animations[ANIMATION_COUNT] = {
  &golAnimation,
  &highlifeAnimation,
  &daynightAnimation,
  &seedsAnimation,
  &rainAnimation,
  &automatonAnimation,
  &plasmaAnimation,
  &cubeAnimation,
  &mandelbrotAnimation,
  &ballsAnimation,
  &mazeAnimation,
  &langtonAnimation,
  &sandAnimation,
  &starfieldAnimation,
  &crystalAnimation,
  &chaosAnimation,
  &truchetAnimation,
  &spirographAnimation,
  &hilbertAnimation,
  &boidsAnimation,
  &fireworksAnimation,
  &pendulumAnimation,
  &snowAnimation,
  &lightningAnimation,
  &metaballsAnimation,
  &clocklifeAnimation,
};

int8_t findanimation(const char *id) {
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    if (strcmp(animations[i]->id, id) == 0) {
      return i;
    }
  }
  return -1;
}
