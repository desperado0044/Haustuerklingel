#include "touch_mode.h"
#include "../config.h"

TouchModeManager& TouchModeManager::instance() {
  static TouchModeManager manager;
  return manager;
}

bool TouchModeManager::begin() {
  // Kurzer, eigener NVS-Namespace (siehe storage/calibration.cpp für den Hintergrund zum
  // 15-Zeichen-Limit von ESP-IDF-Namespace-Namen).
  return prefs.begin("htk_touch", false);
}

bool TouchModeManager::loadFullscreen() {
  return prefs.getBool("fullscreen", TOUCH_MODE_DEFAULT_FULLSCREEN);
}

void TouchModeManager::saveFullscreen(bool fullscreen) {
  prefs.putBool("fullscreen", fullscreen);
}
