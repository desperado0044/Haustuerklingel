#include "button.h"
#include "hardware.h"
#include "../config.h"
#include "../ipc/messages.h"
#include "../storage/button_style.h"
#include "../storage/overlay_mode.h"

KlingelButton& KlingelButton::instance() {
  static KlingelButton button;
  return button;
}

void KlingelButton::begin() {
  w = BUTTON_WIDTH;
  h = BUTTON_HEIGHT;
  ButtonStyle style = ButtonStyleManager::instance().load();
  x = style.x;
  y = style.y;
  color = style.color;
  weight = style.weight;
  hidden = OverlayModeManager::instance().loadHidden();
}

void KlingelButton::setStyle(uint16_t newColor, TextWeight newWeight, int16_t newX, int16_t newY) {
  color = newColor;
  weight = newWeight;
  x = newX;
  y = newY;
  ButtonStyleManager::instance().save({color, weight, x, y});
}

void KlingelButton::setHidden(bool newHidden) {
  hidden = newHidden;
  OverlayModeManager::instance().saveHidden(hidden);
}

void KlingelButton::drawIdle() {
  if (hidden) {
    return;
  }

  // Nur Outline (keine Füllung), damit das Foto darunter im Ruhezustand weiter durchscheint.
  // Rahmendicke skaliert mit der Schriftstärke: Normal = 2 Linien, Fett = 3, Extra Fett = 4 -
  // jede weitere Linie um 1px nach innen versetzt.
  int outlineLines = 2 + static_cast<int>(weight);
  for (int i = 0; i < outlineLines; i++) {
    tft.drawRoundRect(x + i, y + i, w - 2 * i, h - 2 * i, BUTTON_CORNER_RADIUS, color);
  }

  tft.setTextColor(color);  // kein Hintergrund-Argument => transparenter Textdraw.
  tft.setTextDatum(MC_DATUM);
  // "Fake bold" per mehrfachem Zeichnen mit 1px Versatz - TFT_eSPIs eingebaute Fonts haben keine
  // eigene Bold-Variante; ein zusätzlicher Bold-Font würde für 5 Zeichen unverhältnismäßig viel
  // Flash kosten. Jede Stufe fügt der vorherigen weitere Versatz-Positionen hinzu: Fett verdickt
  // nach rechts, Extra Fett zusätzlich nach unten/diagonal (füllt die Zwischenräume), Ultra Fett
  // zusätzlich nach links/oben - damit wächst der Effekt rund um den Originaltext statt einseitig
  // nur nach unten-rechts zu wandern.
  static const int8_t OFFSETS[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {-1, 0}, {0, -1}};
  static const int DRAW_COUNT[4] = {1, 2, 4, 6};
  int drawCount = DRAW_COUNT[static_cast<int>(weight)];
  for (int i = 0; i < drawCount; i++) {
    tft.drawString("Klingel", x + w / 2 + OFFSETS[i][0], y + h / 2 + OFFSETS[i][1], 2);
  }
  tft.setTextDatum(TL_DATUM);
}

bool KlingelButton::isInside(int16_t px, int16_t py) const {
  return px >= x - BUTTON_TOUCH_MARGIN && px <= x + w + BUTTON_TOUCH_MARGIN &&
         py >= y - BUTTON_TOUCH_MARGIN && py <= y + h + BUTTON_TOUCH_MARGIN;
}

void KlingelButton::trigger() {
  if (ringing) {
    return;
  }
  ringing = true;
  ringStartMs = millis();

  // Füllung mit der bitinvertierten Zielfarbe (Cyan statt Rot) *bevor* die Hardware-Invertierung
  // aktiviert wird: tft.invertDisplay(true) invertiert jeden Pixel, den der Panel-Controller
  // ausgibt, auch bereits im Framebuffer stehende - diese Vorkompensation ist es, die den Button
  // am Ende visuell rot statt cyan erscheinen lässt, sobald die Panel-weite Invertierung unten
  // greift.
  tft.fillRoundRect(x, y, w, h, BUTTON_CORNER_RADIUS, static_cast<uint16_t>(~TFT_RED));
  tft.invertDisplay(true);

  ToNetworkMsg msg{ToNetworkMsgType::KlingelPressed};
  xQueueSend(g_toNetworkQueue, &msg, 0);
}

bool KlingelButton::update() {
  if (!ringing) {
    return false;
  }
  if (millis() - ringStartMs < BUTTON_INVERT_DURATION_MS) {
    return false;
  }
  ringing = false;
  tft.invertDisplay(false);
  return true;
}
