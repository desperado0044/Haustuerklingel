#include "button_style.h"
#include "../config.h"

ButtonStyleManager& ButtonStyleManager::instance() {
  static ButtonStyleManager manager;
  return manager;
}

bool ButtonStyleManager::begin() {
  // Eigener, kurzer NVS-Namespace statt NVS_NAMESPACE "_btn": ESP-IDF erlaubt maximal 15 Zeichen
  // für Namespace-Namen, NVS_NAMESPACE ("haustuerklingel") ist bereits exakt 15 Zeichen lang - ein
  // Suffix würde das Limit überschreiten (ESP-IDF-Verhalten dabei nicht verlässlich definiert).
  return prefs.begin("htk_button", false);
}

ButtonStyle ButtonStyleManager::load() {
  ButtonStyle style;
  style.color = prefs.getUShort("color", BUTTON_DEFAULT_COLOR);
  style.weight = static_cast<TextWeight>(prefs.getUChar("weight", static_cast<uint8_t>(BUTTON_DEFAULT_WEIGHT)));
  style.x = prefs.getShort("x", BUTTON_DEFAULT_X);
  style.y = prefs.getShort("y", BUTTON_DEFAULT_Y);
  return style;
}

void ButtonStyleManager::save(const ButtonStyle& style) {
  prefs.putUShort("color", style.color);
  prefs.putUChar("weight", static_cast<uint8_t>(style.weight));
  prefs.putShort("x", style.x);
  prefs.putShort("y", style.y);
}
