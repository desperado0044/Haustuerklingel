#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Aussehen + Position des Klingel-Button-Overlays (Rahmen-/Textfarbe als RGB565, Schriftstärke,
// Position als linke obere Ecke - aus einem 3x5-Raster gewählt, siehe net/upload_route.cpp).
// Änderbar über die Upload-Seite (/klingelstil), in NVS persistiert. KlingelButton (Core 1) ist
// die einzige Stelle, die tatsächlich schreibt (siehe button.cpp) - der Web-Handler auf Core 0
// liest hier nur lesend zum Vorausfüllen des Formulars; reine NVS-Lesezugriffe sind laut ESP-IDF
// intern threadsicher, anders als der SPI/TFT-Zugriff selbst.
enum class TextWeight : uint8_t {
  Normal = 0,
  Bold = 1,
  ExtraBold = 2,
  UltraBold = 3,
};

struct ButtonStyle {
  uint16_t color;
  TextWeight weight;
  int16_t x;
  int16_t y;
};

class ButtonStyleManager {
public:
  static ButtonStyleManager& instance();
  bool begin();

  ButtonStyle load();
  void save(const ButtonStyle& style);

private:
  ButtonStyleManager() = default;
  Preferences prefs;
};
