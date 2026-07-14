#include "touch_calibration.h"
#include "hardware.h"
#include "../config.h"
#include "../storage/calibration.h"

TouchCalibrationRoutine& TouchCalibrationRoutine::instance() {
  static TouchCalibrationRoutine routine;
  return routine;
}

bool TouchCalibrationRoutine::pollLocalTrigger(bool touchedNow) {
  bool risingEdge = touchedNow && !lastTouched;
  lastTouched = touchedNow;
  if (!risingEdge) {
    return false;
  }

  unsigned long now = millis();
  if (pressCount == 0 || now - windowStartMs > CALIBRATION_LOCAL_TRIGGER_WINDOW_MS) {
    windowStartMs = now;
    pressCount = 0;
  }
  pressCount++;

  if (pressCount >= CALIBRATION_LOCAL_TRIGGER_COUNT) {
    pressCount = 0;
    runSequence();
    return true;
  }
  return false;
}

void TouchCalibrationRoutine::runNow() {
  runSequence();
}

namespace {

struct RawPoint {
  int32_t x;
  int32_t y;
};

RawPoint waitForTouchAt(int16_t crossX, int16_t crossY) {
  tft.fillScreen(TFT_BLACK);
  tft.drawLine(crossX - 10, crossY, crossX + 10, crossY, TFT_WHITE);
  tft.drawLine(crossX, crossY - 10, crossX, crossY + 10, TFT_WHITE);

  // Blockierendes Warten auf Touch/Loslassen: nur hier akzeptabel, da die Kalibrierung eine
  // seltene, explizite, reine Core-1-Nutzerinteraktion ist - Core 0 (WLAN/MQTT) läuft unbeeinflusst weiter.
  //
  // Beim lokalen 10x-Trigger (siehe pollLocalTrigger()) ist der 10. Tap physisch noch aktiv,
  // wenn runSequence() anläuft - ts.touched() wäre also sofort true, und diese "alte" Berührung
  // (irgendwo auf dem Screen, nicht am Fadenkreuz) würde fälschlich als erste Eckenmessung
  // übernommen. Deshalb hier zuerst auf Loslassen warten, erst danach auf die echte Berührung.
  while (ts.touched()) {
    delay(10);
  }
  while (!ts.touched()) {
    delay(10);
  }
  TS_Point p = ts.getPoint();
  while (ts.touched()) {
    delay(10);
  }
  return RawPoint{p.x, p.y};
}

}  // namespace

void TouchCalibrationRoutine::runSequence() {
  const int16_t inset = 20;
  RawPoint tl = waitForTouchAt(inset, inset);
  RawPoint tr = waitForTouchAt(SCREEN_WIDTH - inset, inset);
  RawPoint bl = waitForTouchAt(inset, SCREEN_HEIGHT - inset);
  RawPoint br = waitForTouchAt(SCREEN_WIDTH - inset, SCREEN_HEIGHT - inset);

  // Pro Rand gemittelt (nicht global min/max über alle 4 Ecken) - dadurch bleibt bekannt, welcher
  // Rohwert zu welcher physischen Seite gehört. map() in getCalibratedTouch() interpoliert auch
  // korrekt, wenn z.B. rawXLeft > rawXRight ist, und behandelt eine invertierte Touch-Achse damit
  // automatisch richtig, statt Touches spiegelverkehrt/um 180° gedreht zu registrieren.
  TouchCalibration cal;
  cal.rawXLeft = (tl.x + bl.x) / 2;
  cal.rawXRight = (tr.x + br.x) / 2;
  cal.rawYTop = (tl.y + tr.y) / 2;
  cal.rawYBottom = (bl.y + br.y) / 2;
  CalibrationManager::instance().save(cal);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Kalibrierung gespeichert", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);
  tft.setTextDatum(TL_DATUM);
  delay(1000);
}

bool getCalibratedTouch(int16_t& outX, int16_t& outY) {
  if (!ts.touched()) {
    return false;
  }
  TS_Point p = ts.getPoint();
  TouchCalibration cal = CalibrationManager::instance().load();
  long x = map(p.x, cal.rawXLeft, cal.rawXRight, 0, SCREEN_WIDTH);
  long y = map(p.y, cal.rawYTop, cal.rawYBottom, 0, SCREEN_HEIGHT);
  outX = constrain(x, 0, SCREEN_WIDTH - 1);
  outY = constrain(y, 0, SCREEN_HEIGHT - 1);
  return true;
}
