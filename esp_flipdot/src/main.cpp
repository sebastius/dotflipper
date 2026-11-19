//  Flipdot
//  Sebastius 2018
//  CC BY-SA 4.0
//
//  https://github.com/sebastius/dotflipper
//  https://revspace.nl/dotflipper
//
//  The Dotflipper is a generic FlipDot controller, for 32x16 panels without their own driver chip.
//  There can be up to 8 panels on the i2c bus for a maximum of 4096 dots of flippin' goodness.
//  Adressing is done per panel, starting top left so:
//
//  0 1    or  0 1 2 3 4 5 6 7   or 0    or 0 1 2   etc...
//  2 3                             1       3 4 5
//  4 5                             2
//  6 7                             3
//
//  Set the adresses on the board using the DIP switch top right.
//  A cheat-sheet is provided on the board.
//  (0 = 000, 1 = 001, 2 = 010, 3 = 011, 4 = 100, 5 = 101, 6 = 110, 7 = 111)
//
//  Build with PlatformIO, see platformio.ini.
//
//  Files (in src/):
//    config.h       panel layout, features and settings
//    pca9685.*      bit-banged I2C to the PCA9685 driver chips (v1 boards)
//    i2cpins.h      direct access to the I2C pins (v1)
//    shiftchain.*   the chain of 74HC595 shift registers (v3 boards, HARDWARE_V3)
//    chainpins.h    direct access to the chain pins (v3)
//    display.*      framebuffers and flipping dots (v1)
//    display595.cpp flipping dots on the v3 boards
//    pictures.h     bitmaps
//    flipsettings.* settings from the web interface, stored in flash
//    player.*       decides what is on the screen: clock, animations or nothing
//    webui.*        web interface and JSON API on http://flipdot.local
//    clockface.*    clock faces: big, time + date, analog, words, seconds and binary, NTP
//    transition.*   transitions when the minute changes
//    fonts.h        fonts for the clock
//    animation.h    interface every animation implements
//    animations.cpp list of all animations
//    life.cpp       Game of Life, HighLife, Day & Night, Seeds and the clock in Life
//    rain.cpp       falling raindrops
//    automaton.cpp  1D cellular automaton
//    plasma.cpp     dithered plasma
//    cube.cpp       rotating wireframe cube
//    mandelbrot.cpp zooming into the Mandelbrot set
//    balls.cpp      bouncing balls
//    maze.cpp       maze being dug and solved
//    langton.cpp    Langton's ant
//    sand.cpp       falling sand
//    starfield.cpp  flying through the stars
//    crystal.cpp    crystal growth (diffusion limited aggregation)
//    chaos.cpp      chaos game: fern and Sierpinski triangle
//    truchet.cpp    Truchet tiles turning one by one
//    spirograph.cpp spirograph and Lissajous figures
//    hilbert.cpp    Hilbert curve drawing itself
//    boids.cpp      flock of birds
//    fireworks.cpp  fireworks
//    pendulum.cpp   pendulum wave
//    snow.cpp       snow that piles up
//    lightning.cpp  lightning
//    metaballs.cpp  outlines of melting blobs
//    wifisetup.*    WiFiManager and OTA updates
//    udpdisplay.*   show framebuffers received over UDP (NETWORK)
//
//  WiFi: on first boot (or when the known network is gone) the ESP starts an access point
//  called "flipdot-setup". Connect to it and choose your network.
//  Hold the BOOT button during startup to forget the stored network.

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "display.h"
#ifndef HARDWARE_V3
#include "pca9685.h"
#endif
#include "wifisetup.h"

#ifdef NETWORK
#include "udpdisplay.h"
#else
#include "flipsettings.h"
#include "player.h"
#include "webui.h"
#include "clockface.h"
#endif

#define WIFI_RESET_PIN 0  // BOOT button on most ESP32 boards

void setup() {
  // First thing: after a reset in the middle of a pulse the drivers can still be on
  driverbegin();

  Serial.begin(115200);
  Serial.println();

  Serial.println("Flipdot software");

#ifdef DEBUG
#ifndef HARDWARE_V3
  i2cscanner();
#endif
  for (uint8_t t = 10; t > 0; t--) {
    Serial.print("Waiting ");
    Serial.print(t);
    Serial.println(" seconds to continue");
    delay(1000);
  }
  Serial.print("Resuming regular code");
#endif

  Serial.println("Flipdot getting ready to go!");

#ifndef NETWORK
  loadsettings();
#endif

  YellowScreen();
  ClearScreen();

  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    Serial.println("BOOT button pressed, forgetting WiFi settings");
    resetWiFiSettings();
  }

  setupWiFi();
  setupOTA();

#ifdef NETWORK
  udpdisplaysetup();
#else
  clocksetup();
  webuisetup();
  playersetup();
  Serial.printf("Web interface on http://%s.local/\n", HOSTNAME);
#endif

  Serial.println("Ready");
}

void loop() {
  ArduinoOTA.handle();
  yield();

#ifdef NETWORK
  udpdisplayloop();
#else
  webuiloop();
  playerloop();
#endif
}
