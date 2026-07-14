#include "ota_manager.h"
#include "../config.h"

OtaManager& OtaManager::instance() {
  static OtaManager manager;
  return manager;
}

void OtaManager::begin(AsyncWebServer& server) {
  ElegantOTA.begin(&server);
  ElegantOTA.setAuth(OTA_HOSTNAME, OTA_PASSWORD);
}
