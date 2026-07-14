#pragma once
#include <Arduino.h>
#include <ElegantOTA.h>
#include <ESPAsyncWebServer.h>

// Dünner Wrapper um ElegantOTA: stellt die Upload-UI/Endpunkte unter /update auf dem gemeinsamen
// AsyncWebServer bereit, geschützt per HTTP Basic Auth (OTA_HOSTNAME/OTA_PASSWORD). ElegantOTA.loop()
// muss weiterhin bei jeder network-task-Iteration aufgerufen werden (siehe net/network_task.cpp),
// damit ein abgeschlossener Upload erkannt wird und tatsächlich neu gestartet wird.
class OtaManager {
public:
  static OtaManager& instance();
  void begin(AsyncWebServer& server);
};
