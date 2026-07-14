#include "calibration.h"
#include "../config.h"

CalibrationManager& CalibrationManager::instance() {
  static CalibrationManager manager;
  return manager;
}

bool CalibrationManager::begin() {
  // Eigener, kurzer NVS-Namespace statt NVS_NAMESPACE "_cal": ESP-IDF erlaubt maximal 15 Zeichen
  // für Namespace-Namen, NVS_NAMESPACE ("haustuerklingel") ist bereits exakt 15 Zeichen lang - ein
  // Suffix würde das Limit überschreiten (ESP-IDF-Verhalten dabei nicht verlässlich definiert).
  // Separat von CredentialsManager, damit eine "WLAN zurücksetzen"-Aktion anderswo nicht
  // versehentlich eine physisch durchgeführte Kalibrierung löscht.
  return prefs.begin("htk_calib", false);
}

TouchCalibration CalibrationManager::load() {
  TouchCalibration cal;
  cal.rawXLeft = prefs.getUShort("x_left", TOUCH_DEFAULT_MINX);
  cal.rawXRight = prefs.getUShort("x_right", TOUCH_DEFAULT_MAXX);
  cal.rawYTop = prefs.getUShort("y_top", TOUCH_DEFAULT_MINY);
  cal.rawYBottom = prefs.getUShort("y_bottom", TOUCH_DEFAULT_MAXY);
  return cal;
}

void CalibrationManager::save(const TouchCalibration& cal) {
  prefs.putUShort("x_left", cal.rawXLeft);
  prefs.putUShort("x_right", cal.rawXRight);
  prefs.putUShort("y_top", cal.rawYTop);
  prefs.putUShort("y_bottom", cal.rawYBottom);
}
