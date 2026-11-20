#include <WiFi.h>
#include <WiFiUdp.h>

#include "config.h"
#include "display.h"
#include "font.h"
#include "udpdisplay.h"

static WiFiUDP udp;

static uint8_t prevFramebuffer[ROWS * COLS / 8] = { 0 };
static uint8_t framebuffer[ROWS * COLS / 8] = { 0 };

static void showip() {
  uint32_t ip = (uint32_t)(WiFi.localIP());
  for (int octet = 0; octet < 4; octet++) {
    uint8_t b = (ip >> (octet * 8)) & 0xff;
    for (int fy = 0; fy < 5; fy++) {
      for (int fx = 0; fx < 3; fx++) {
        for (int d = 0, e = 100; d < 3; d++, e /= 10) {
          uint8_t num = (b / e) % 10;
          if (num || d) flipdot(fx + d * 4, fy + octet * 6, (fontDigits[num][fy] >> (2 - fx)) & 1);
        }
      }
    }
  }
}

void udpdisplaysetup() {
  showip();
  udp.begin(UDP_PORT);
  Serial.printf("Running UDP server on port: %d\n", UDP_PORT);
  delay(5000);
  ClearScreen();
}

void udpdisplayloop() {
  int c = udp.parsePacket();
  if (c) {
    Serial.printf("Received %d bytes from %s, port %d\n", c, udp.remoteIP().toString().c_str(), udp.remotePort());
    udp.read(framebuffer, sizeof(framebuffer));
  }

  for (uint16_t y = 0; y < ROWS; y++) {
    for (uint16_t x = 0; x < COLS; x++) {
      uint16_t i = y * COLS + x;

      uint8_t a = (framebuffer[i >> 3] >> (i & 7)) & 1,
              b = (prevFramebuffer[i >> 3] >> (i & 7)) & 1;
      if (a ^ b) {
        flipdot(x, y, a);
      }
    }
  }

  memcpy(prevFramebuffer, framebuffer, sizeof(framebuffer));
}
