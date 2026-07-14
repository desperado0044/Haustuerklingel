#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Nachrichtentypen von Core 0 (network_task) -> Core 1 (display_task, Arduinos Standard-loop()).
enum class ToDisplayMsgType : uint8_t {
  NewImageAvailable,   // /current.jpg auf LittleFS wurde gerade aktualisiert - neu zeichnen.
  StartCalibration,    // haustuer/kalibrieren="start" kam per MQTT rein.
  SetBrightness,       // haustuer/helligkeit-Payload, bereits auf 0-255 geparst.
  StatusText,          // Status-Zeile für Boot/AP-Modus, auf schlichtem Hintergrund (noch kein Bild).
  SetButtonStyle,      // Farbe/Schriftstärke/Position des Klingel-Buttons, von /klingelstil gesetzt.
  SetBellDuration,     // Dauer des physischen Klingelton-Impulses (MOSFET), von /klingelton bzw. MQTT gesetzt.
  SetTouchMode,        // Vollflächig vs. nur Klingel-Button-Trefferfläche, von /touchmodus bzw. MQTT gesetzt.
  SetOverlayVisible,   // Klingel-Button-Overlay ein-/ausblenden, von /overlay bzw. MQTT gesetzt.
};

struct ToDisplayMsg {
  ToDisplayMsgType type;
  uint8_t brightness;   // gültig bei type == SetBrightness
  char text[64];         // gültig bei type == StatusText
  uint16_t buttonColor;  // gültig bei type == SetButtonStyle
  uint8_t buttonWeight;  // gültig bei type == SetButtonStyle; 0=Normal, 1=Fett, 2=Extra Fett (storage/button_style.h::TextWeight)
  int16_t buttonX;       // gültig bei type == SetButtonStyle
  int16_t buttonY;       // gültig bei type == SetButtonStyle
  uint16_t bellDurationMs;  // gültig bei type == SetBellDuration
  bool touchFullscreen;     // gültig bei type == SetTouchMode
  bool overlayHidden;       // gültig bei type == SetOverlayVisible
};

// Nachrichtentypen von Core 1 (display_task) -> Core 0 (network_task), da die MQTT-Client-Instanz
// ausschließlich von Core 0 angefasst wird.
enum class ToNetworkMsgType : uint8_t {
  KlingelPressed,  // Button wurde gerade berührt (steigende Flanke) - haustuer/klingel=pressed senden.
};

struct ToNetworkMsg {
  ToNetworkMsgType type;
};

// Die Queues werden einmalig in main.cpp's setup() angelegt (bevor einer der Tasks sie anfassen
// kann) und über diese externs bereitgestellt statt als Task-Parameter übergeben, da sowohl
// display_task's loop() (Arduinos impliziter Main-Task) als auch network_task ohne gemeinsames
// Kontextobjekt Zugriff brauchen.
extern QueueHandle_t g_toDisplayQueue;
extern QueueHandle_t g_toNetworkQueue;
