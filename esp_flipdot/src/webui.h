#pragma once

// Web interface on port 80, plus a JSON API that Home Assistant can use too:
//   GET  /api/state           what is on the screen, the time, all settings and the list of animations
//   POST /api/settings        change settings, the body is (part of) the settings JSON, see flipsettings.h.
//                             Send it with Content-Type: application/json
//   POST /api/play?id=<id>    play one animation now, then go back to the schedule

void webuisetup();  // Call after WiFi and OTA are started
void webuiloop();
