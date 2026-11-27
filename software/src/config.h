#pragma once

// Features
//#define DEBUG
//#define NETWORK    // Only show UDP framebuffers, instead of the clock and animations

// Panel layout
// The v1 code only handles two panels on top of each other; the v3 env sets its own layout in platformio.ini
#ifndef PANELS_X
#define PANELS_X     1   // in number of panels
#endif
#ifndef PANELS_Y
#define PANELS_Y     2   // in number of panels
#endif
#define PANEL_WIDTH  32  // in dots
#define PANEL_HEIGHT 16  // in dots
#define COLS (PANELS_X * PANEL_WIDTH)
#define ROWS (PANELS_Y * PANEL_HEIGHT)

#if PANELS_X == 0 || PANELS_Y == 0 || PANELS_X * PANELS_Y > 8
#error "Please check PANELS_X and PANELS_Y: both should be > 0 and PANELS_X*PANELS_Y should not be > 8!"
#endif

// Hardware. Default: the v1 boards with PCA9685 drivers over I2C, on an ESP32 PICO.
// HARDWARE_V3 (pio env v3): the v3 boards with 74HC595 shift registers, on an ESP32-S3-DevKitC-1.

// I2C bus to the PCA9685 drivers (v1)
#define I2C_SDA 25
#define I2C_SCL 26
#define I2C_CLOCK 800000

// Chain of v3 boards: one board per panel, panel 0 on the board with the ESP32, the next panel on the
// board its J2 goes to, and so on. Close JP1 on the last board: at startup the firmware sends one bit
// around the chain and counts the boards. When that count isn't CHAIN_BOARDS nothing flips.
#define CHAIN_BOARDS       (PANELS_X * PANELS_Y)
#define CHAIN_PIN_DATA     11
#define CHAIN_PIN_SCLK     12
#define CHAIN_PIN_LATCH    10
#define CHAIN_PIN_NOE      9
#define CHAIN_PIN_DIR_X1   14
#define CHAIN_PIN_DIR_ROW  8
#define CHAIN_PIN_RETURN   13
#define CHAIN_PIN_CURRENT  4      // Coil current from the INA180, 0.45 A is about 1.5 V (not used yet)
#define CHAIN_CLOCK        2000000  // SCLK in Hz, each cable has 1k in series: a few MHz at most
#define CHAIN_CHECK_CLOCK  50000    // SCLK while counting boards: the return line passes every board
#define PULSE_GAP_MICROS   100      // Rest between two pulses, for the coil current to die out. Not measured yet

// Length of the coil pulse. It sets the peak current: 20 V over 20 ohm would end up above what
// the drivers can take, only a short pulse keeps the current down.
#ifdef HARDWARE_V3
// Per color, each pulse flips one color only. Found on the first panel (2026-11-27), with the coil current
// measured at the end of the pulse (POST /api/test/current): the dots need about 480-490 mA to flip every
// time, close to the 500 mA per output of the ULN2803A and XL62783.
//   20 V: yellow 225 us (455 mA) not every dot, 250 (490 mA) all; black 170 (465 mA) not every dot, 200 (530 mA) all
//   24 V: yellow 170 not every dot, 190 (555 mA) all; black 150 (480-600 mA) all
// Running at 20 V.
#define PULSE_MICROS_YELLOW 250
#define PULSE_MICROS_BLACK  200
#define PULSE_MICROS       (PULSE_MICROS_YELLOW > PULSE_MICROS_BLACK ? PULSE_MICROS_YELLOW : PULSE_MICROS_BLACK)
#else
#define PULSE_MICROS       224
#endif
#define PULSE_LIMIT_MICROS 350  // Emergency stop when a pulse takes longer, one retry of an off write fits

// Network
#ifndef HOSTNAME                             // The v3 envs set their own, so v1 and v3 can share a network
#define HOSTNAME            "flipdot"
#endif
#define WIFI_AP_NAME        HOSTNAME "-setup"  // Access point name of the WiFiManager config portal
#define WIFI_PORTAL_TIMEOUT 180              // in seconds, reboot when nobody configured WiFi
#define OTA_PASSWORD        "admin"
#define UDP_PORT            1337

// Clock
// Netherlands: CET (UTC+1), CEST (UTC+2) from the last Sunday of March 02:00 until the last Sunday of October 03:00
#define CLOCK_TIMEZONE    "CET-1CEST,M3.5.0,M10.5.0/3"
#define CLOCK_NTP_SERVER1 "nl.pool.ntp.org"
#define CLOCK_NTP_SERVER2 "pool.ntp.org"

// Rain
#define RAIN_DROPS    12   // Number of drops on or above the screen
#define RAIN_MAX_TAIL  2    // in dots, every drop gets a tail of 0 up to this
#define RAIN_SLOWEST   3    // Slowest drops fall one row every this many frames
#define RAIN_FRAMETIME 40   // in ms

// Game of Life
#define GOL_GENERATIONS 100
#define GOL_FRAMETIME   75   // in ms
#define GOL_DENSITY     10   // percentage of cells alive at start
