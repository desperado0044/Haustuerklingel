#pragma once
#include <Arduino.h>

// 4-Ecken-Fadenkreuz-Kalibrierung, auf zwei Wegen auslösbar (Lastenheft):
//  1. MQTT (haustuer/kalibrieren=start, von mqtt_manager per runNow() weitergeleitet) - braucht WLAN.
//  2. Lokal, immer aktiv: CALIBRATION_LOCAL_TRIGGER_COUNT Berührungen innerhalb von
//     CALIBRATION_LOCAL_TRIGGER_WINDOW_MS, rein über ts.touched() erkannt und daher unabhängig
//     vom WLAN-Status oder davon, ob die aktuellen Kalibrierwerte überhaupt brauchbar sind.
class TouchCalibrationRoutine {
public:
  static TouchCalibrationRoutine& instance();

  // Bei jedem display-task-Loop-Tick mit dem rohen touched()-Wert füttern. Liefert true, wenn der
  // lokale Trigger gerade ausgelöst hat (die Kalibrierung ist dann bereits synchron gelaufen) -
  // der Aufrufer sollte neu zeichnen.
  bool pollLocalTrigger(bool touchedNow);

  // Führt die Kalibriersequenz sofort aus (MQTT-getriggerter Pfad).
  void runNow();

private:
  TouchCalibrationRoutine() = default;
  void runSequence();

  bool lastTouched = false;
  int pressCount = 0;
  unsigned long windowStartMs = 0;
};

// Wendet die persistierte Kalibrierung auf den aktuellen rohen Touch-Wert an, in
// Bildschirmkoordinaten. Der einzige Touch-Lese-Einstiegspunkt für den Normalbetrieb (Button-
// Hit-Test etc.) - die Kalibrierroutine selbst liest ts.getPoint() direkt, da sie unkalibrierte
// Rohwerte braucht.
bool getCalibratedTouch(int16_t& outX, int16_t& outY);
