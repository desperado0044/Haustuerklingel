#pragma once
#include <Arduino.h>
#include "../storage/button_style.h"

// Das virtuelle "Klingel"-Button-Overlay: eine per /klingelstil konfigurierbare (Position, Farbe,
// Schriftstärke) abgerundete Outline (transparente Füllung im Ruhezustand, sodass das Foto
// darunter durchscheint) mit einer großzügigen, unsichtbaren Touch-Fläche drumherum.
class KlingelButton {
public:
  static KlingelButton& instance();

  // Lädt Position/Aussehen aus NVS (siehe storage/button_style.h). Einmalig nach
  // tft.setRotation() aufrufen.
  void begin();

  // Zeichnet die Ruhezustand-Outline (+ "Klingel"-Beschriftung). Muss nach jedem vollständigen
  // Bild-Redraw aufgerufen werden, da der JPEG-Draw sie sonst übermalen würde.
  void drawIdle();

  bool isInside(int16_t px, int16_t py) const;

  bool isRinging() const { return ringing; }

  // Übernimmt eine neue Position/Farbe/Schriftstärke-Einstellung (von /klingelstil, siehe
  // net/upload_route.cpp) und persistiert sie gleich mit in NVS - siehe storage/button_style.h.
  void setStyle(uint16_t newColor, TextWeight newWeight, int16_t newX, int16_t newY);

  // Blendet Rahmen + "Klingel"-Beschriftung aus bzw. wieder ein (Foto bleibt in jedem Fall
  // sichtbar) - die Touch-Trefferfläche (isInside()) bleibt davon unberührt aktiv. Von /overlay
  // bzw. MQTT gesetzt (siehe net/upload_route.cpp, net/mqtt_manager.cpp) und gleich mit in NVS
  // persistiert - siehe storage/overlay_mode.h.
  void setHidden(bool newHidden);
  bool isHidden() const { return hidden; }

  // Startet die Klingel-Zustandsmaschine: füllt den Button (vorinvertiert, damit er nach
  // tft.invertDisplay(true) auf dem ganzen Panel rot erscheint), reiht das MQTT-"pressed"-Event
  // für Core 0 ein. No-op, falls bereits geklingelt wird (erneute Berührung während einer
  // laufenden Klingel-Sequenz wird ignoriert).
  void trigger();

  // Bei jedem display-task-Loop-Tick aufrufen. Liefert genau einmal true, wenn die 500ms-
  // Invertierungsperiode gerade beendet und tft.invertDisplay(false) angewendet wurde - der
  // Aufrufer muss dann das komplette gecachte Bild neu zeichnen (was über drawIdle() auch den
  // Ruhezustand-Button wiederherstellt).
  bool update();

private:
  KlingelButton() = default;
  int16_t x = 0, y = 0, w = 0, h = 0;
  uint16_t color = 0xFFFF;
  TextWeight weight = TextWeight::Normal;
  bool ringing = false;
  unsigned long ringStartMs = 0;
  bool hidden = false;
};
