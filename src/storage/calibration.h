#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Rohe XPT2046-Ablesungen an den vier Bildschirmrändern. In NVS persistiert, damit die 4-Ecken-
// Kalibrierroutine (display/touch_calibration.cpp) nur einmal pro physischer Einheit laufen muss.
//
// Bewusst als "Rohwert an linkem/rechtem/oberem/unterem Rand" statt als simples min/max
// gespeichert: welcher Rand den numerisch kleineren oder größeren Rohwert liefert, hängt von der
// (nicht vorhersagbaren) Achsrichtung des jeweiligen Touch-Panels ab. map() in touch_calibration.cpp
// interpoliert auch korrekt, wenn z.B. rawXLeft > rawXRight ist - so wird eine invertierte Achse
// automatisch richtig behandelt, statt Touches spiegelverkehrt zu registrieren.
struct TouchCalibration {
  uint16_t rawXLeft;
  uint16_t rawXRight;
  uint16_t rawYTop;
  uint16_t rawYBottom;
};

class CalibrationManager {
public:
  static CalibrationManager& instance();
  bool begin();

  TouchCalibration load();
  void save(const TouchCalibration& cal);

private:
  CalibrationManager() = default;
  Preferences prefs;
};
