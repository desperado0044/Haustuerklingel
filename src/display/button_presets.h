#pragma once
#include <Arduino.h>

// Gemeinsam von der Weboberfläche (net/upload_route.cpp) und der MQTT-Steuerung
// (net/mqtt_manager.cpp) genutzte Presets für den Klingel-Button - eine einzige Quelle für
// Farbliste, Schriftstärke-Namen und das 3x5-Positionsraster, damit beide Bedienwege dieselben
// Werte/Nummerierungen verwenden.

struct ColorOption {
  const char* name;
  uint16_t rgb565;
};

// Aus TFT_eSPI.h übernommene Standardfarben (exakte RGB565-Werte, nicht aus dem Gedächtnis
// geraten) - 12 gut unterscheidbare Optionen.
extern const ColorOption COLOR_OPTIONS[];
extern const int COLOR_OPTION_COUNT;

// Groß-/Kleinschreibung wird ignoriert. Liefert nullptr, falls kein Treffer.
const ColorOption* findColorByName(const String& name);

// Index == TextWeight-Wert (storage/button_style.h): 0=Normal, 1=Fett, 2=Extra Fett, 3=Ultra Fett.
extern const char* const WEIGHT_NAMES[4];
extern const int WEIGHT_COUNT;

// Liefert -1, falls kein Treffer.
int findWeightIndexByName(const String& name);

struct GridPos {
  int16_t x;
  int16_t y;
};

extern const int POSITION_GRID_ROWS;
extern const int POSITION_GRID_COLS;
extern const int POSITION_COUNT;  // POSITION_GRID_ROWS * POSITION_GRID_COLS

// index ist 1-basiert (1..POSITION_COUNT), in Lesereihenfolge (Zeile für Zeile, links nach
// rechts) - dieselbe Nummerierung wie im Web-Formular angezeigt. Liefert {0,0} bei ungültigem Index.
GridPos gridPositionByIndex(int index);
