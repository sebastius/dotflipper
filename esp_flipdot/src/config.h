#pragma once

// Features
//#define DEBUG
#define GOL
//#define NETWORK   // Show UDP framebuffers instead of Game of Life

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

// Network
#define HOSTNAME            "flipdot"
#define WIFI_AP_NAME        "flipdot-setup"  // Access point name of the WiFiManager config portal
#define WIFI_PORTAL_TIMEOUT 180              // in seconds, reboot when nobody configured WiFi
#define OTA_PASSWORD        "admin"
#define UDP_PORT            1337

// Game of Life
#define GOL_GENERATIONS 100
#define GOL_FRAMETIME   75   // in ms
#define GOL_DENSITY     10   // percentage of cells alive at start
