#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Dauer des physischen Klingelton-Impulses (AO3400-Ansteuerung, siehe display/bell_output.h) -
// bewusst unabhängig von der visuellen Klingel-Animation (storage/button_style.h) konfigurierbar,
// in NVS persistiert. BellOutput (Core 1) ist die einzige Stelle, die tatsächlich schreibt - der
// Web-/MQTT-Handler auf Core 0 liest hier nur lesend zum Vorausfüllen des Formulars.
class BellSettingsManager {
public:
  static BellSettingsManager& instance();
  bool begin();

  uint16_t loadDurationMs();
  void saveDurationMs(uint16_t ms);

private:
  BellSettingsManager() = default;
  Preferences prefs;
};
