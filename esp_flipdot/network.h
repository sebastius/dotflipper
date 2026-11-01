#pragma once

// WiFi is configured with WiFiManager: when no known network is reachable the ESP starts
// an access point (WIFI_AP_NAME). Connect to it with your phone and pick a network,
// the credentials are stored in flash, so nothing secret lives in this source code.

void setupWiFi();
void resetWiFiSettings();   // Forget stored credentials, config portal starts on next boot
void setupOTA();
