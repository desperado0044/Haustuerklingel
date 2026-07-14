#include "status_screen.h"
#include "hardware.h"
#include "../config.h"

void StatusScreen::draw(const String& text) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextWrap(true, true);
  tft.drawString(text, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
  tft.setTextDatum(TL_DATUM);
}
