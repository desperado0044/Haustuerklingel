#include "overlay_mode.h"
#include "../config.h"

OverlayModeManager& OverlayModeManager::instance() {
  static OverlayModeManager manager;
  return manager;
}

bool OverlayModeManager::begin() {
  // Kurzer, eigener NVS-Namespace (siehe storage/calibration.cpp für den Hintergrund zum
  // 15-Zeichen-Limit von ESP-IDF-Namespace-Namen).
  return prefs.begin("htk_overlay", false);
}

bool OverlayModeManager::loadHidden() {
  return prefs.getBool("hidden", OVERLAY_DEFAULT_HIDDEN);
}

void OverlayModeManager::saveHidden(bool hidden) {
  prefs.putBool("hidden", hidden);
}
