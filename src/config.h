#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------------------------
// Display (TFT_eSPI, ILI9341, HSPI). TFT_MOSI/SCLK/CS/DC/RST werden über build_flags in
// platformio.ini gesetzt (TFT_eSPI liest sie von dort, nicht von hier) - nur als Kommentar
// dupliziert: TFT_MOSI=4 TFT_SCLK=14 TFT_CS=17 TFT_DC=21 TFT_RST=16 (TFT_MISO unbelegt/-1).
// TFT_MOSI=4 ist nicht HSPI's nativer MOSI-Pin (GPIO13 war am realen Board nicht verbunden),
// läuft also über die GPIO-Matrix - für ein statisches Display unkritisch.
// TODO: an der realen Verdrahtung verifizieren, sobald das Board vorliegt; keiner dieser Pins ist
// ein ESP32-Strapping-Pin (0/2/5/12/15), ein Verdrahtungsfehler hier sollte den Boot also nicht
// verhindern.
// ---------------------------------------------------------------------------------------------
// Querformat (320x240) ist die tatsächliche Einbaulage - am realen Gerät verifiziert, abweichend
// von der ursprünglich angenommenen Hochformat-Montage. Bilder aus Home Assistant müssen daher
// exakt 320x240px sein (siehe README), nicht 240x320.
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Rotation 3 zeigte Text zwar korrekt lesbar, das Modul wird aber physisch um 180° gedreht
// eingebaut, um einen besseren Blickwinkel des TN-Panels zu bekommen - daher hier wieder auf 1
// zurück, um das zu kompensieren (beide sind dieselbe Querformat-Canvas 320x240, siehe oben).
// ts.setRotation(TFT_ROTATION) in display_task.cpp hängt an derselben Konstante, zieht also
// automatisch mit. ACHTUNG: die gespeicherte Touch-Kalibrierung gilt pro Rotation - nach einer
// Änderung hier muss neu kalibriert werden (10x tippen oder haustuer/kalibrieren=start).
#define TFT_ROTATION 1

// Die Hintergrundbeleuchtung wird bewusst NICHT über TFT_eSPIs TFT_BL/TFT_BACKLIGHT_ON-Build-Flags
// gesteuert - wir steuern sie selbst per ledc-PWM (siehe display/display_task.cpp), damit die
// Helligkeit nachts gedimmt werden kann statt nur ein-/ausschaltbar zu sein.
#define TFT_BACKLIGHT_PIN 22
#define TFT_BACKLIGHT_LEDC_CHANNEL 0
#define TFT_BACKLIGHT_LEDC_FREQ_HZ 5000
#define TFT_BACKLIGHT_LEDC_RESOLUTION_BITS 8

// ---------------------------------------------------------------------------------------------
// Touch (XPT2046, resistiv). XPT2046_Touchscreen verwendet intern fest das globale Arduino-
// `SPI`-Objekt, läuft also auf VSPI (SPI3) mit DESSEN nativen Pins (SCLK=18, MOSI=23, MISO=19) -
// echte, vom HSPI-Bus des Displays getrennte Hardware (siehe USE_HSPI_PORT in platformio.ini).
// Nur CS/IRQ sind frei gewählte, einfache GPIOs; bewusst von jedem ESP32-Strapping-Pin
// (0/2/5/12/15) ferngehalten.
// TODO: an der realen Verdrahtung verifizieren.
// ---------------------------------------------------------------------------------------------
#define TOUCH_CS_PIN 27
#define TOUCH_IRQ_PIN 33

// Fallback-Bereich für die rohen ADC-Kalibrierwerte, nur solange gültig bis der Nutzer die
// 4-Ecken-Kalibrierroutine ausführt (siehe storage/calibration.h) und diese Werte per NVS ersetzt.
#define TOUCH_DEFAULT_MINX 300
#define TOUCH_DEFAULT_MAXX 3800
#define TOUCH_DEFAULT_MINY 300
#define TOUCH_DEFAULT_MAXY 3800

// Lokaler Kalibrier-Fallback: N Berührungen innerhalb von WINDOW_MS (rohes touched(), unabhängig
// von Koordinaten-Kalibrierung) starten die Kalibrierroutine, unabhängig vom WLAN-/MQTT-Status.
#define CALIBRATION_LOCAL_TRIGGER_COUNT 10
#define CALIBRATION_LOCAL_TRIGGER_WINDOW_MS 7000UL

// ---------------------------------------------------------------------------------------------
// WLAN / Provisionierung
// ---------------------------------------------------------------------------------------------
#define WIFI_AP_NAME "Haustuerklingel-Setup"
#define WIFI_AP_PASSWORD ""
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define WIFI_RECONNECT_CHECK_MS 5000UL

// NVS-Namespace (Preferences) für persistierte WLAN-/MQTT-Zugangsdaten und Touch-Kalibrierung.
#define NVS_NAMESPACE "haustuerklingel"

// ---------------------------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------------------------
#define MQTT_DEFAULT_BROKER ""
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_USER ""
#define MQTT_DEFAULT_PASS ""
#define MQTT_RECONNECT_COOLDOWN_MS 5000UL
#define MQTT_CLIENT_ID_PREFIX "haustuerklingel-"

#define MQTT_TOPIC_BILD "haustuer/bild"
#define MQTT_TOPIC_KLINGEL "haustuer/klingel"
#define MQTT_TOPIC_STATUS "haustuer/status"
#define MQTT_TOPIC_KALIBRIEREN "haustuer/kalibrieren"
#define MQTT_TOPIC_HELLIGKEIT "haustuer/helligkeit"

// Klingel-Button-Aussehen/-Position - Payload-Werte siehe src/display/button_presets.h
// (Farbname/Stärkename/Positionsnummer 1-15). Jedes Topic ändert nur sein eigenes Feld, die
// jeweils anderen beiden bleiben unverändert (siehe MqttManager::handleMessage()).
#define MQTT_TOPIC_KLINGEL_FARBE "haustuer/klingelfarbe"
#define MQTT_TOPIC_KLINGEL_STAERKE "haustuer/klingelstaerke"
#define MQTT_TOPIC_KLINGEL_POSITION "haustuer/klingelposition"
// Physischer Klingelton (MOSFET) - Dauer in ms, unabhängig von den drei Topics oben.
#define MQTT_TOPIC_KLINGELTON_DAUER "haustuer/klingelton_dauer"
// Touch-Modus: Payload "ON" = vollflächig (jede Berührung klingelt), "OFF" = nur innerhalb der
// Klingel-Button-Trefferfläche (Standard) - Standard-MQTT-switch-Konvention.
#define MQTT_TOPIC_TOUCH_MODUS "haustuer/touch_modus"
// Overlay-Sichtbarkeit: Payload "ON" = Klingel-Button-Overlay ausgeblendet (Foto ohne
// Rahmen/Beschriftung, Touch-Trefferfläche bleibt unberührt aktiv), "OFF" = sichtbar (Standard) -
// Standard-MQTT-switch-Konvention.
#define MQTT_TOPIC_KLINGEL_AUSBLENDEN "haustuer/klingel_ausblenden"

#define MQTT_KLINGEL_RESET_DELAY_MS 300UL

// Home-Assistant-Basis-URL, von der Bilder geladen werden: das Payload von MQTT_TOPIC_BILD wird
// als /local/klingel/<payload>.jpg angehängt - das Payload enthält laut Lastenheft bereits die
// .jpg-Endung (z.B. "willkommen.jpg"), hier steht also nur das feste Präfix.
#define HA_IMAGE_BASE_URL_DEFAULT ""
#define HA_IMAGE_PATH_PREFIX "/local/klingel/"

// ---------------------------------------------------------------------------------------------
// Home Assistant MQTT-Discovery
// ---------------------------------------------------------------------------------------------
#define HA_DEVICE_ID "haustuerklingel"
#define HA_DEVICE_NAME "Haustuerklingel"

// Helligkeits-Payload-Format: 0-255 (passt direkt auf den 8-Bit-ledc-PWM-Duty des ESP32, keine
// Skalierung nötig). Falls später Prozent bevorzugt wird, müssen nur das Discovery-Payload in
// mqtt_manager.cpp + der ledc-Write angepasst werden.
#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 255
#define BRIGHTNESS_DEFAULT 200

// ---------------------------------------------------------------------------------------------
// LittleFS-Bild-Cache
// ---------------------------------------------------------------------------------------------
#define CURRENT_IMAGE_PATH "/current.jpg"
#define CURRENT_IMAGE_FLAG_PATH "/current.txt"
#define HTTP_DOWNLOAD_TIMEOUT_MS 10000UL

// ---------------------------------------------------------------------------------------------
// OTA (ElegantOTA über den bestehenden AsyncWebServer)
// ---------------------------------------------------------------------------------------------
#define OTA_HOSTNAME "Haustuerklingel"
#define OTA_PASSWORD "1234567890"

// ---------------------------------------------------------------------------------------------
// Klingel-Button-Overlay
// ---------------------------------------------------------------------------------------------
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 80
#define BUTTON_CORNER_RADIUS 12
// Die Touch-Trefferfläche reicht auf jeder Seite um so viele px über die sichtbare Outline hinaus.
#define BUTTON_TOUCH_MARGIN 20
#define BUTTON_INVERT_DURATION_MS 500UL

// Standard: nur Berührungen innerhalb der Button-Trefferfläche lösen die Klingel aus. Über
// Web-UI/MQTT (haustuer/touch_modus) umschaltbar auf "vollflächig" (jede Bildschirmberührung
// klingelt) - siehe storage/touch_mode.h.
#define TOUCH_MODE_DEFAULT_FULLSCREEN false

// Standard: Overlay (Rahmen + "Klingel"-Beschriftung) sichtbar. Über Web-UI/MQTT
// (haustuer/klingel_ausblenden) ausblendbar - die Touch-Trefferfläche bleibt davon unberührt
// aktiv, siehe storage/overlay_mode.h.
#define OVERLAY_DEFAULT_HIDDEN false

// ---------------------------------------------------------------------------------------------
// Physischer Klingelton (N-Kanal-MOSFET, logic-level, z.B. AO3400/IRF3708, als Low-Side-Schalter,
// direkt per 3.3V-GPIO angesteuert, kein Levelshifter nötig). Gate hängt bis BellOutput::begin()
// am internen Pulldown des Pins (siehe BellOutput::earlyInit(), so früh wie möglich in main.cpp's
// setup() aufgerufen) statt frei zu floaten - ohne den war das Gate beim IRF3708 in der Praxis
// undefiniert genug, um den MOSFET während der Boot-Phase leiten zu lassen.
// ---------------------------------------------------------------------------------------------
#define BELL_MOSFET_PIN 25
// Eigene, von der visuellen Klingel-Animation unabhängige Dauer (siehe display/bell_output.h) -
// über Web-UI und MQTT (haustuer/klingelton_dauer) änderbar, in NVS persistiert.
#define BELL_DEFAULT_DURATION_MS 500
// Obergrenze gegen einen versehentlich riesigen MQTT-Wert, der den MOSFET dauerhaft offen hielte
// (z.B. Buzzer/Relais könnte das nicht vertragen).
#define BELL_MAX_DURATION_MS 10000

// Default-Position (links mittig, Lastenheft-Vorgabe), bis über /klingelstil ein Feld aus dem
// 3x5-Raster gewählt wird (siehe net/upload_route.cpp). Default-Aussehen: TFT_WHITE, normale Schriftstärke.
#define BUTTON_DEFAULT_X 8
#define BUTTON_DEFAULT_Y ((SCREEN_HEIGHT - BUTTON_HEIGHT) / 2)
#define BUTTON_DEFAULT_COLOR 0xFFFF
#define BUTTON_DEFAULT_WEIGHT TextWeight::Normal

// ---------------------------------------------------------------------------------------------
// Watchdog
// ---------------------------------------------------------------------------------------------
#define TASK_WATCHDOG_TIMEOUT_SEC 10
