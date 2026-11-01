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
//    pca9685.*      low level I2C to the PCA9685 driver chips
//    display.*      framebuffers and flipping dots
//    pictures.h     bitmaps
//    gol.*          Game of Life
//    wifisetup.*    WiFiManager and OTA updates
//    udpdisplay.*   show framebuffers received over UDP (NETWORK)
//
//  WiFi: on first boot (or when the known network is gone) the ESP starts an access point
//  called "flipdot-setup". Connect to it and choose your network.
//  Hold the BOOT button during startup to forget the stored network.

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "pca9685.h"
#include "display.h"
#include "wifisetup.h"

#ifdef GOL
#include "gol.h"
#endif

#ifdef NETWORK
#include "udpdisplay.h"
#endif

#define WIFI_RESET_PIN 0  // BOOT button on most ESP32 boards

void setup() {
  Wire.begin();
  Wire.setClock(1000000); // ESP32

  Serial.begin(115200);
  Serial.println();

  Serial.println("Flipdot software");

#ifdef DEBUG
  i2cscanner();
  for (uint8_t t = 10; t > 0; t--) {
    Serial.print("Waiting ");
    Serial.print(t);
    Serial.println(" seconds to continue");
    delay(1000);
  }
  Serial.print("Resuming regular code");
#endif

  Serial.println("Flipdot getting ready to go!");

  resetPCA9685();
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
#endif

  Serial.println("Ready");
}

void loop() {
  ArduinoOTA.handle();
  yield();

#if defined(NETWORK)
  udpdisplayloop();
#elif defined(GOL)
  ClearScreen();
  GoL();
  ClearScreen();
#endif
}
