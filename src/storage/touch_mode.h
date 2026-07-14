#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Ob eine Berührung überall auf dem Bildschirm die Klingel auslöst ("vollflächig") oder nur
// innerhalb der Klingel-Button-Trefferfläche (Standard) - über Web-UI (Checkbox) und MQTT
// (haustuer/touch_modus) umschaltbar, in NVS persistiert. display_task.cpp ist die einzige
// Stelle, die tatsächlich schreibt - Web-/MQTT-Handler auf Core 0 lesen hier nur lesend zum
// Vorausfüllen des Formulars.
class TouchModeManager {
public:
  static TouchModeManager& instance();
  bool begin();

  bool loadFullscreen();
  void saveFullscreen(bool fullscreen);

private:
  TouchModeManager() = default;
  Preferences prefs;
};
