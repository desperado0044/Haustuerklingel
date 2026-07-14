#include "bell_output.h"
#include "../config.h"
#include "../storage/bell_settings.h"

BellOutput& BellOutput::instance() {
  static BellOutput output;
  return output;
}

void BellOutput::begin() {
  pinMode(BELL_MOSFET_PIN, OUTPUT);
  digitalWrite(BELL_MOSFET_PIN, LOW);
  durationMs = BellSettingsManager::instance().loadDurationMs();
}

void BellOutput::trigger() {
  if (active) {
    return;
  }
  active = true;
  startMs = millis();
  digitalWrite(BELL_MOSFET_PIN, HIGH);
}

void BellOutput::update() {
  if (!active) {
    return;
  }
  if (millis() - startMs >= durationMs) {
    active = false;
    digitalWrite(BELL_MOSFET_PIN, LOW);
  }
}

void BellOutput::setDurationMs(uint16_t ms) {
  durationMs = constrain(ms, (uint16_t)0, (uint16_t)BELL_MAX_DURATION_MS);
  BellSettingsManager::instance().saveDurationMs(durationMs);
}
