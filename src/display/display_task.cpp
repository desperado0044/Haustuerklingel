#include "display_task.h"
#include "hardware.h"
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include "../config.h"
#include "button.h"
#include "touch_calibration.h"
#include "status_screen.h"
#include "image_cache.h"
#include "../ipc/messages.h"
#include "../storage/calibration.h"
#include "../storage/button_style.h"
#include "../storage/touch_mode.h"
#include "../storage/overlay_mode.h"
#include "bell_output.h"

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

namespace {

bool lastButtonTouched = false;
// Wird gesetzt, sobald je ein echtes Foto gezeigt wurde - entscheidet, ob eingehende StatusText-
// Nachrichten noch gerendert werden (Vorgabe: Statustext ersetzt nur einen leeren/schwarzen
// Bildschirm während Boot/AP-Modus, wird nie über ein laufendes Bild gelegt).
bool imageEverShown = false;
// true = jede Berührung irgendwo auf dem Bildschirm klingelt, false (Standard) = nur innerhalb
// der Klingel-Button-Trefferfläche. Siehe storage/touch_mode.h.
bool touchFullscreen = false;

bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void renderCachedImage() {
  if (ImageCache::instance().hasCachedImage()) {
    // drawFsJpg() nutzt per Default-Parameter SPIFFS - das mounten wir nirgends, ImageCache
    // mountet LittleFS. Explizit angeben, sonst schlägt das Öffnen der Datei still fehl (kein
    // Fehler, es wird schlicht nichts gezeichnet und der alte Bildschirminhalt bleibt stehen).
    TJpgDec.drawFsJpg(0, 0, CURRENT_IMAGE_PATH, LittleFS);
    imageEverShown = true;
  } else {
    tft.fillScreen(TFT_BLACK);
  }
  KlingelButton::instance().drawIdle();
}

void applyBrightness(uint8_t value) {
  ledcWrite(TFT_BACKLIGHT_LEDC_CHANNEL, value);
}

}  // namespace

void displayTaskSetup() {
  // Öffnet die NVS-Namespaces für Touch-Kalibrierung und Klingel-Button-Stil - ohne diese Aufrufe
  // liefern load() stumm nur die Fallback-Defaults und save() schreibt ins Leere.
  CalibrationManager::instance().begin();
  ButtonStyleManager::instance().begin();
  TouchModeManager::instance().begin();
  OverlayModeManager::instance().begin();
  touchFullscreen = TouchModeManager::instance().loadFullscreen();

  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);

  // Hintergrundbeleuchtung wird manuell per ledc-PWM gesteuert (nicht über TFT_eSPIs TFT_BL-Flag),
  // damit sie für den Nachtmodus über haustuer/helligkeit gedimmt werden kann, statt nur komplett
  // ein-/ausschaltbar zu sein.
  ledcSetup(TFT_BACKLIGHT_LEDC_CHANNEL, TFT_BACKLIGHT_LEDC_FREQ_HZ, TFT_BACKLIGHT_LEDC_RESOLUTION_BITS);
  ledcAttachPin(TFT_BACKLIGHT_PIN, TFT_BACKLIGHT_LEDC_CHANNEL);
  applyBrightness(BRIGHTNESS_DEFAULT);

  // Touch am globalen Arduino-SPI-Objekt (native VSPI-Pins) - echte, von TFT_eSPIs eigener,
  // HSPI-basierter Display-Instanz getrennte Hardware (siehe platformio.ini).
  ts.begin();
  ts.setRotation(TFT_ROTATION);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tjpgOutput);

  KlingelButton::instance().begin();
  BellOutput::instance().begin();

  // Vorgabe aus dem Lastenheft: sofort ein gecachtes Bild (oder mindestens eine Status-Zeile)
  // zeigen - noch bevor network_task's WiFiManager überhaupt startet, da dessen Captive Portal
  // Core 0 lange blockieren kann. ImageCache::instance().begin() (LittleFS-Mount) lief bereits
  // vorher in main.cpp.
  if (ImageCache::instance().hasCachedImage()) {
    renderCachedImage();
  } else {
    StatusScreen::draw("Starte...");
  }
}

void displayTaskLoop() {
  // Von Core 0 (Netzwerk-Task) eingereihte Nachrichten abarbeiten - siehe ipc/messages.h.
  ToDisplayMsg msg;
  while (xQueueReceive(g_toDisplayQueue, &msg, 0) == pdTRUE) {
    switch (msg.type) {
      case ToDisplayMsgType::NewImageAvailable:
        renderCachedImage();
        break;
      case ToDisplayMsgType::StartCalibration:
        TouchCalibrationRoutine::instance().runNow();
        renderCachedImage();
        break;
      case ToDisplayMsgType::SetBrightness:
        applyBrightness(msg.brightness);
        break;
      case ToDisplayMsgType::StatusText:
        if (!imageEverShown) {
          StatusScreen::draw(msg.text);
        }
        break;
      case ToDisplayMsgType::SetButtonStyle:
        KlingelButton::instance().setStyle(msg.buttonColor, static_cast<TextWeight>(msg.buttonWeight), msg.buttonX, msg.buttonY);
        renderCachedImage();
        break;
      case ToDisplayMsgType::SetBellDuration:
        BellOutput::instance().setDurationMs(msg.bellDurationMs);
        break;
      case ToDisplayMsgType::SetTouchMode:
        touchFullscreen = msg.touchFullscreen;
        TouchModeManager::instance().saveFullscreen(touchFullscreen);
        break;
      case ToDisplayMsgType::SetOverlayVisible:
        KlingelButton::instance().setHidden(msg.overlayHidden);
        renderCachedImage();
        break;
    }
  }

  bool rawTouchedNow = ts.touched();

  int16_t tx = 0, ty = 0;
  bool touchedNow = getCalibratedTouch(tx, ty);
  bool risingEdge = touchedNow && !lastButtonTouched;
  lastButtonTouched = touchedNow;

  bool touchTriggersRing = touchFullscreen || KlingelButton::instance().isInside(tx, ty);
  if (risingEdge && imageEverShown && touchTriggersRing) {
    KlingelButton::instance().trigger();
    // Eigene, unabhängige Zustandsmaschine mit eigener Dauer - startet zeitgleich mit der
    // visuellen Animation (derselbe Touch-Event), läuft aber unabhängig davon ab.
    BellOutput::instance().trigger();
  }

  if (KlingelButton::instance().update()) {
    // 500ms-Invertierungsperiode ist gerade zu Ende - Ruhezustand-Button + zugrunde liegendes Bild wiederherstellen.
    renderCachedImage();
  }
  BellOutput::instance().update();

  // Lokaler Kalibrier-Fallback: unabhängig von Koordinaten-Kalibrierung/WLAN, immer aktiv.
  if (TouchCalibrationRoutine::instance().pollLocalTrigger(rawTouchedNow)) {
    renderCachedImage();
  }
}
