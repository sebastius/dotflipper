#include <WiFi.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>

#include "config.h"
#include "display.h"
#include "pictures.h"
#include "network.h"

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);

  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT);
  wm.setAPCallback([](WiFiManager * wm) {
    Serial.printf("No WiFi, config portal started. Connect to access point '%s' and browse to %s\n",
                  wm->getConfigPortalSSID().c_str(), WiFi.softAPIP().toString().c_str());
  });

  if (!wm.autoConnect(WIFI_AP_NAME)) {
    Serial.println("WiFi not configured in time! Rebooting...");
    delay(1000);
    ESP.restart();
  }

  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void resetWiFiSettings() {
  WiFiManager wm;
  wm.resetSettings();
}

void setupOTA() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    ClearScreen();
    showpicture(OTA_picture);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u dots\r", (progress / (total / 32)));
    byte progressdot = ((progress / (total / 27)));
    flipdot(progressdot + 2, 3, 1);
    flipdot(progressdot + 2, 4, 1);
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}
