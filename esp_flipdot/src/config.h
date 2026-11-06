#pragma once

// Features
//#define DEBUG
//#define NETWORK    // Only show UDP framebuffers, instead of the clock and animations

// Panel layout
#define PANELS_X     1   // in number of panels
#define PANELS_Y     2   // in number of panels
#define PANEL_WIDTH  32  // in dots
#define PANEL_HEIGHT 16  // in dots
#define COLS (PANELS_X * PANEL_WIDTH)
#define ROWS (PANELS_Y * PANEL_HEIGHT)

#if PANELS_X == 0 || PANELS_Y == 0 || PANELS_X * PANELS_Y > 8
#error "Please check PANELS_X and PANELS_Y: both should be > 0 and PANELS_X*PANELS_Y should not be > 8!"
#endif

// I2C bus to the PCA9685 drivers
#define I2C_SDA 25
#define I2C_SCL 26
#define I2C_CLOCK 800000

// Length of the coil pulse. It sets the peak current: 20 V over 20 ohm would end up above what
// the drivers can take, only a short pulse keeps the current down.
#define PULSE_MICROS       224
#define PULSE_LIMIT_MICROS 350  // Emergency stop when a pulse takes longer, one retry of an off write fits

// Network
#define HOSTNAME            "flipdot"
#define WIFI_AP_NAME        "flipdot-setup"  // Access point name of the WiFiManager config portal
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
