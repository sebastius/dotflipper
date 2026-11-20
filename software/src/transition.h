#pragma once

#include "flipsettings.h"

// Transitions from what is on the screen (current_state) to a new picture:
//  - dissolve: the changed dots flip a few at a time in random order
//  - fall: old dots disappear, then the new dots fall in from the top, bottom ones first
//  - sand: old dots crumble and fall out at the bottom, then the new dots appear

void transitionstart(Transition type);  // The new picture is in next_state
bool transitionstep();                  // Draws the next frame into next_state, false when done

#define TRANSITION_FRAMETIME 30  // in ms
