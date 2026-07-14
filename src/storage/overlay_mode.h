#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Ob das virtuelle Klingel-Button-Overlay (Rahmen + "Klingel"-Beschriftung, siehe
// display/button.h) sichtbar gezeichnet oder ausgeblendet wird (Foto ohne Overlay - die
// Touch-Trefferfläche bleibt davon unberührt aktiv) - über Web-UI (Checkbox) und MQTT
// (haustuer/klingel_ausblenden) umschaltbar, in NVS persistiert. KlingelButton (Core 1) ist die
// einzige Stelle, die tatsächlich schreibt - der Web-/MQTT-Handler auf Core 0 liest hier nur
// lesend zum Vorausfüllen des Formulars.
class OverlayModeManager {
public:
  static OverlayModeManager& instance();
  bool begin();

  bool loadHidden();
  void saveHidden(bool hidden);

private:
  OverlayModeManager() = default;
  Preferences prefs;
};
