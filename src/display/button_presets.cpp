#include "button_presets.h"
#include "../config.h"

const ColorOption COLOR_OPTIONS[] = {
    {"Weiss", 0xFFFF},
    {"Rot", 0xF800},
    {"Gruen", 0x07E0},
    {"Blau", 0x001F},
    {"Gelb", 0xFFE0},
    {"Orange", 0xFDA0},
    {"Cyan", 0x07FF},
    {"Magenta", 0xF81F},
    {"Violett", 0x915C},
    {"Rosa", 0xFE19},
    {"Silber", 0xC618},
    {"Himmelblau", 0x867D},
};
const int COLOR_OPTION_COUNT = sizeof(COLOR_OPTIONS) / sizeof(COLOR_OPTIONS[0]);

const ColorOption* findColorByName(const String& name) {
  for (int i = 0; i < COLOR_OPTION_COUNT; i++) {
    if (name.equalsIgnoreCase(COLOR_OPTIONS[i].name)) {
      return &COLOR_OPTIONS[i];
    }
  }
  return nullptr;
}

const char* const WEIGHT_NAMES[4] = {"Normal", "Fett", "Extra Fett", "Ultra Fett"};
const int WEIGHT_COUNT = 4;

int findWeightIndexByName(const String& name) {
  for (int i = 0; i < WEIGHT_COUNT; i++) {
    if (name.equalsIgnoreCase(WEIGHT_NAMES[i])) {
      return i;
    }
  }
  return -1;
}

const int POSITION_GRID_ROWS = 3;
const int POSITION_GRID_COLS = 5;
const int POSITION_COUNT = POSITION_GRID_ROWS * POSITION_GRID_COLS;

// BUTTON_WIDTH > Zellenbreite (320/5=64 vs. 80), benachbarte Spalten überlappen sich dadurch etwas
// - beabsichtigt, es sind grobe Ankerpunkte, keine exklusiven Zonen. BUTTON_HEIGHT trifft die
// Zeilenhöhe bei 3 Zeilen exakt (240/3=80), daher dort kein Clamping nötig.
GridPos gridPositionByIndex(int index) {
  if (index < 1 || index > POSITION_COUNT) {
    return {0, 0};
  }
  int zeroBased = index - 1;
  int row = zeroBased / POSITION_GRID_COLS;
  int col = zeroBased % POSITION_GRID_COLS;
  int cellW = SCREEN_WIDTH / POSITION_GRID_COLS;
  int cellH = SCREEN_HEIGHT / POSITION_GRID_ROWS;
  int centerX = cellW * col + cellW / 2;
  int centerY = cellH * row + cellH / 2;
  int16_t x = constrain(centerX - BUTTON_WIDTH / 2, 0, SCREEN_WIDTH - BUTTON_WIDTH);
  int16_t y = constrain(centerY - BUTTON_HEIGHT / 2, 0, SCREEN_HEIGHT - BUTTON_HEIGHT);
  return {x, y};
}
