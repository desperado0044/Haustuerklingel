#include "bell_settings.h"
#include "../config.h"

BellSettingsManager& BellSettingsManager::instance() {
  static BellSettingsManager manager;
  return manager;
}

bool BellSettingsManager::begin() {
  // Kurzer, eigener NVS-Namespace (siehe storage/calibration.cpp für den Hintergrund zum
  // 15-Zeichen-Limit von ESP-IDF-Namespace-Namen).
  return prefs.begin("htk_bell", false);
}

uint16_t BellSettingsManager::loadDurationMs() {
  return prefs.getUShort("duration_ms", BELL_DEFAULT_DURATION_MS);
}

void BellSettingsManager::saveDurationMs(uint16_t ms) {
  prefs.putUShort("duration_ms", ms);
}
