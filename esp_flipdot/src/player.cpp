#include <time.h>

#include "display.h"
#include "animation.h"
#include "flipsettings.h"
#include "clockface.h"
#include "transition.h"
#include "player.h"

enum PlayerState { SHOWING_OFF, SHOWING_CLOCK, SHOWING_ANIMATION };

static PlayerState state = SHOWING_OFF;
static bool preview;
static bool reload;

static uint8_t current;        // Animation on the screen
static uint8_t next;           // Where to start looking for the next animation
static uint8_t breakleft;      // Animations left in this break
static unsigned long started;  // When the current animation started
static unsigned long nextframe;

static bool clockdirty;        // Redraw the clock, also when the time didn't change
static bool transitioning;     // A transition to the new time is running
static int lastminute = -1, lastsecond = -1, lasthour = -1;
static int lastbreak = -1;      // Minute of the year of the last break

// Next enabled animation after the previous one, -1 if none is enabled
static int8_t pickanimation() {
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    uint8_t index = (next + i) % ANIMATION_COUNT;
    if (settings.animation[index].enabled) {
      next = (index + 1) % ANIMATION_COUNT;
      return index;
    }
  }
  return -1;
}

static void startanimation(uint8_t index) {
  Serial.printf("Animation: %s\n", animations[index]->name);
  ClearScreen();
  animations[index]->setup();
  current = index;
  transitioning = false;
  started = millis();
  nextframe = started;
  state = SHOWING_ANIMATION;
}

static void showclock() {
  if (state == SHOWING_ANIMATION) {
    ClearScreen();
  }
  preview = false;
  clockdirty = true;
  transitioning = false;
  state = SHOWING_CLOCK;
}

// What to show when nothing special is going on
static void showdefault() {
  preview = false;
  if (!settings.clockEnabled) {
    int8_t index = pickanimation();
    if (index >= 0) {
      startanimation(index);
      return;
    }
  }
  showclock();  // Also when the clock is off but no animation is enabled
}

static void updateclock() {
  struct tm now;
  if (!getLocalTime(&now, 0)) {
    if (clockdirty) {
      clockdrawunsynced();
      fullupdate();
      clockdirty = false;
    }
    return;
  }

  // Time for a break?
  int minute = (now.tm_yday * 24 + now.tm_hour) * 60 + now.tm_min;
  if (settings.interval > 0 && now.tm_min % settings.interval == 0 && minute != lastbreak) {
    lastbreak = minute;
    int8_t index = pickanimation();
    if (index >= 0) {
      breakleft = settings.animationsPerBreak - 1;
      startanimation(index);
      return;
    }
  }

  if (transitioning) {
    if ((long)(millis() - nextframe) >= 0) {
      transitioning = transitionstep();
      fullupdate();
      nextframe = millis() + TRANSITION_FRAMETIME;
    }
    return;
  }

  bool ticking = settings.clockStyle == CLOCK_SECONDS || settings.clockStyle == CLOCK_BINARY ||
                 (settings.clockStyle == CLOCK_ANALOG && settings.clockSeconds);
  if (!clockdirty && now.tm_min == lastminute && (!ticking || now.tm_sec == lastsecond)) {
    return;
  }
  clockdraw(now, settings.clockStyle, settings.clockSeconds);
  if (now.tm_hour != lasthour) {
    forcefullupdate();  // Once an hour flip every dot, in case one got stuck
  } else if (!clockdirty && !ticking && settings.clockTransition != TRANSITION_NONE) {
    transitionstart(settings.clockTransition);
    transitioning = true;
    nextframe = millis();
  } else {
    fullupdate();
  }
  if (now.tm_min != lastminute) {
    Serial.printf("Clock: %02d:%02d\n", now.tm_hour, now.tm_min);
  }
  lastminute = now.tm_min;
  lastsecond = now.tm_sec;
  lasthour = now.tm_hour;
  clockdirty = false;
}

static void updateanimation() {
  const AnimationSettings &as = settings.animation[current];
  bool stop = millis() - started >= as.duration * 1000UL || (!preview && !as.enabled);

  if (stop) {
    if (preview || !settings.clockEnabled) {
      showdefault();
    } else if (breakleft > 0) {
      breakleft--;
      int8_t index = pickanimation();
      if (index >= 0) {
        startanimation(index);
      } else {
        showclock();
      }
    } else {
      showclock();
    }
    return;
  }

  if ((long)(millis() - nextframe) >= 0) {
    animations[current]->step();
    fullupdate();
    nextframe = millis() + animations[current]->frametime * 100UL / as.speed;
  }
}

void playersetup() {
  state = SHOWING_OFF;
  reload = true;
}

void playerloop() {
  if (driverfault()) {
    return;  // Nothing flips anymore until a restart
  }

  if (!settings.power) {
    if (state != SHOWING_OFF) {
      Serial.println("Power off");
      ClearScreen();
      state = SHOWING_OFF;
      preview = false;
    }
    return;
  }

  if (reload || state == SHOWING_OFF) {
    reload = false;
    if (state == SHOWING_OFF) {
      showdefault();
    } else if (state == SHOWING_CLOCK) {
      if (settings.clockEnabled) {
        clockdirty = true;  // The style might have changed
      } else {
        showdefault();
      }
    }
    // A running animation checks its own settings
  }

  if (state == SHOWING_CLOCK) {
    updateclock();
  } else if (state == SHOWING_ANIMATION) {
    updateanimation();
  }
}

void playerreload() {
  reload = true;
}

void playerpreview(uint8_t index) {
  if (!settings.power) {
    return;
  }
  breakleft = 0;
  startanimation(index);
  preview = true;
}

String playerstatus() {
  if (driverfault()) {
    return "fault";
  }
  switch (state) {
    case SHOWING_OFF:
      return "off";
    case SHOWING_CLOCK:
      return "clock";
    case SHOWING_ANIMATION:
      return animations[current]->id;
  }
  return "";
}
