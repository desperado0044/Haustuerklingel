#pragma once
#include <Arduino.h>

// Steuert den physischen Klingelton (AO3400 N-Kanal-MOSFET, Low-Side-Schalter an
// BELL_MOSFET_PIN, logic-level, direkt per 3.3V-GPIO angesteuert) - eine eigene, von der
// visuellen Klingel-Animation (button.h) unabhängige, nicht-blockierende Zustandsmaschine mit
// eigener, über Web-UI/MQTT konfigurierbarer Dauer.
class BellOutput {
public:
  static BellOutput& instance();

  // pinMode + lädt die zuletzt gespeicherte Dauer aus NVS (siehe storage/bell_settings.h).
  void begin();

  // Startet den Impuls (MOSFET an). No-op, falls bereits aktiv.
  void trigger();

  // Bei jedem display-task-Loop-Tick aufrufen; schaltet den MOSFET nach Ablauf der konfigurierten
  // Dauer wieder aus.
  void update();

  // Übernimmt + persistiert eine neue Dauer (von /klingelton bzw. MQTT haustuer/klingelton_dauer).
  void setDurationMs(uint16_t ms);

private:
  BellOutput() = default;
  uint16_t durationMs = 500;
  bool active = false;
  unsigned long startMs = 0;
};
