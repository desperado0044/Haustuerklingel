# Haustürklingel-Display (ESP32)

ESP32-Firmware für ein 2.8"-Touchdisplay an der Haustür: zeigt ein Bild an, überlagert mit einem
virtuellen Klingel-Button, und meldet Klingelereignisse per MQTT zurück.

Home Assistant ist der übliche Anwendungsfall (inkl. automatischer MQTT-Discovery), aber keine
Voraussetzung: Bilder lassen sich auch direkt über die Weboberfläche hochladen, und jeder andere
MQTT-Broker bzw. jede andere Automatisierung funktioniert genauso über dieselben Topics. Auch ganz
ohne MQTT (z.B. kein Broker konfiguriert oder gerade nicht erreichbar) bleibt das Gerät über die
Weboberfläche voll nutzbar - Bild-Upload, Klingel-Button-Aussehen/-Position, Touch-Bereich,
Overlay-Sichtbarkeit und Klingelton-Dauer lassen sich dort einstellen, und der Klingel-Button
selbst (Bildschirm-Animation + physischer Klingelton) funktioniert unabhängig davon.

## Hardware

> Nachbau und Verdrahtung auf eigene Gefahr - keine Garantie für Vollständigkeit oder Richtigkeit
> dieser Anleitung.

- ESP32 (Bare-Devkit, kein fertiges Board wie das "Cheap Yellow Display")
- 2.8" TFT SPI, 320×240, ILI9341-Treiber – andere Displaydiagonalen mit demselben Treiber-Chip
  und derselben 320×240-Auflösung sollten ebenso funktionieren
- Resistiver Touch, XPT2046-Controller
- Einbau **horizontal (Querformat)**

### Pinbelegung

Display und Touch laufen bewusst auf **getrennten SPI-Bussen**: Display auf HSPI (SPI2,
`TFT_eSPI`-eigene Instanz), Touch auf VSPI (SPI3, Arduino-globales `SPI`-Objekt –
`XPT2046_Touchscreen` erlaubt keine eigene SPI-Instanz).

| Signal | Pin | Bus |
|---|---|---|
| TFT_SCLK | 14 | HSPI (nativ) |
| TFT_MOSI | 4 | GPIO-Matrix (GPIO13 am Board nicht verbunden) |
| TFT_MISO | – (unbelegt) | – |
| TFT_CS | 17 | GPIO |
| TFT_DC | 21 | GPIO |
| TFT_RST | 16 | GPIO |
| Backlight (PWM) | 22 | GPIO (ledc) |
| TOUCH_SCLK | 18 | VSPI (nativ) |
| TOUCH_MOSI | 23 | VSPI (nativ) |
| TOUCH_MISO | 19 | VSPI (nativ) |
| TOUCH_CS | 27 | GPIO |
| TOUCH_IRQ | 33 | GPIO |
| BELL_MOSFET (Gate) | 25 | GPIO |

### Konfigurationshinweise

- **Flash-Größe/Partitionsschema:** Angenommen wird ein 4MB-ESP32-WROOM-32-Modul
  (`min_spiffs.csv`-Partitionstabelle). Bei mehr Flash ggf. `board_build.partitions` in
  `platformio.ini` anpassen.
- **`TFT_ROTATION`** (`src/config.h`): `1` (Querformat-Canvas 320×240) - das Modul wird physisch
  um 180° gedreht eingebaut (besserer Blickwinkel des TN-Panels), diese Rotation kompensiert das
  softwareseitig wieder. Touch nutzt dieselbe Konstante und zieht automatisch mit - **nach jeder
  Änderung hier muss die Touch-Kalibrierung neu durchgeführt werden**, da die gespeicherten
  Rohwerte an die vorherige Rotation gebunden sind.
- **Touch-Kalibrierung:** Werksseitig nur ein grober Fallback-Bereich hinterlegt – die 4-Ecken-
  Kalibrierung sollte nach dem ersten Flashen einmal durchgeführt werden (siehe unten).

## Bauen und Flashen

```
pio run                # bauen
pio run -t upload       # per USB flashen
pio device monitor      # seriellen Log ansehen
```

Für spätere Updates steht auch OTA über [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA)
bereit: `http://<ip-oder-hostname>/update` (HTTP-Basic-Auth, siehe `OTA_HOSTNAME`/`OTA_PASSWORD`
in `src/config.h`).

Für ein direktes `pio run -e esp32dev_ota -t upload` (kein manuelles Hochladen über die
`/update`-Seite im Browser nötig) in `platformio.ini`, Abschnitt `[env:esp32dev_ota]`, den
Platzhalter `custom_upload_url` durch die tatsächliche lokale IP oder den Hostnamen des Geräts im
eigenen WLAN ersetzen (z.B. `http://192.168.1.153/update` oder `http://haustuerklingel.local/update`).

## Erstinbetriebnahme

1. Beim ersten Boot (keine gespeicherten WLAN-Zugangsdaten) öffnet das Gerät einen Hotspot
   `Haustuerklingel-Setup` unter der festen IP `4.3.2.1`.
2. Im Captive Portal werden neben WLAN-SSID/Passwort auch folgende Felder abgefragt:
   - **MQTT Broker**, **MQTT Port**, **MQTT User**, **MQTT Passwort**
   - **Home Assistant Basis-URL**, z.B. `http://192.168.1.10:8123`
3. Nach dem Speichern verbindet sich das Gerät mit dem WLAN und dem MQTT-Broker. Bei künftigen
   Neustarts werden die gespeicherten Zugangsdaten verwendet; das Portal öffnet sich nur erneut,
   wenn die Erstverbindung fehlschlägt (ein späterer WLAN-Ausfall löst nur einen stillen
   Reconnect-Versuch aus, nie wieder das Portal).

### Zugangsdaten später ändern

Zwei getrennte Oberflächen, je nach Zustand des Geräts:

- **Im AP-/Captive-Portal-Modus** (Erstinbetriebnahme oder wenn die WLAN-Verbindung fehlschlägt):
  WiFiManagers Portal unter `4.3.2.1`, siehe oben - deckt WLAN, MQTT und HA-Basis-URL ab.
- **Im laufenden Betrieb (WLAN verbunden):** `http://<ip-oder-hostname>/einstellungen` - dieselben
  Felder, jederzeit direkt im Browser änderbar, ohne das Gerät erst von WLAN zu trennen. Leer
  gelassene Passwortfelder bleiben unverändert. Nach dem Speichern startet das Gerät automatisch
  neu, um die neuen Werte zu übernehmen.

## Bilder in Home Assistant

- Bilder müssen **exakt 320×240px**, **Baseline-JPEG** (kein Progressive-Format) sein und im
  HA-`www`-Ordner unter `www/klingel/<dateiname>.jpg` liegen.
- HA publiziert **retained** auf `haustuer/bild` entweder:
  - einen kurzen Dateinamen (inkl. `.jpg`), z.B. `willkommen.jpg` - wird zu
    `<Home-Assistant-Basis-URL>/local/klingel/<dateiname>.jpg` aufgelöst, oder
  - eine **komplette URL** (`http://...` oder `https://...`), z.B. um mal ein Bild von einer
    anderen Quelle im Netz zu zeigen - wird direkt verwendet, HA-Basis-URL spielt dann keine Rolle.
    HTTPS wird unterstützt, allerdings ohne Zertifikatsprüfung (`setInsecure()` - für beliebige
    Bildquellen ohne Cert-Pinning ausreichend, kein sicherheitskritischer Anwendungsfall).
- Das ESP32 cached das geladene Bild einmalig als `/current.jpg` auf LittleFS. Schlägt der
  Download fehl, bleibt das zuletzt gültige Bild erhalten.

### Bildanzeige manuell testen (ohne MQTT/HA)

Für schnelle Tests gibt es einen Debug-Endpunkt, der direkt den Bild-Download anstößt und dabei
MQTT/Home Assistant komplett umgeht:

```
http://<ip-oder-hostname>/testbild?datei=<dateiname>.jpg&basis=<basis-url>
```

- `datei` (Pflicht): Dateiname, wird an `basis` + `/local/klingel/` angehängt.
- `basis` (optional): überschreibt dauerhaft die gespeicherte Home-Assistant-Basis-URL, z.B.
  `http://192.168.1.10:8123`. Ohne Angabe wird die zuletzt gespeicherte URL verwendet.

Beispiel: `curl "http://haustuerklingel.local/testbild?datei=test.jpg&basis=http://192.168.1.50:8000"`

### Bild manuell hochladen (ohne HA/MQTT, ohne eigenen Server)

Alternative zum Debug-Endpunkt oben: Startseite (`http://<ip-oder-hostname>/`, auch unter
`/upload` erreichbar) im Browser öffnen zeigt ein Upload-Formular. **Beliebiges Foto** wählen -
JavaScript skaliert es im Browser per `<canvas>` automatisch zentriert auf exakt
`SCREEN_WIDTH`×`SCREEN_HEIGHT`px ein (ganzes Bild bleibt sichtbar, nötigenfalls schwarze Balken
oben/unten oder links/rechts - "contain", kein Beschnitt), bevor es hochgeladen wird; das erzeugt
dabei automatisch ein Baseline-JPEG. Bei Erfolg wird das Bild sofort übernommen und angezeigt,
genau wie ein erfolgreicher `haustuer/bild`-Download.

Per `curl` (ohne Browser/JS) hochgeladene Dateien werden **nicht** automatisch zugeschnitten -
serverseitig wird weiterhin die exakte Zielgröße verlangt, bei Abweichung mit Fehlermeldung
abgelehnt, ohne das aktuell angezeigte Bild anzutasten:

```
curl -F "bild=@pfad/zum/bild.jpg" http://haustuerklingel.local/upload
```

## MQTT-Topics

| Topic | Richtung | Payload | Retain |
|---|---|---|---|
| `haustuer/bild` | HA → ESP32 | Dateiname (`willkommen.jpg`) oder komplette URL | ja |
| `haustuer/klingel` | ESP32 → HA | `pressed` / `idle` (Impuls) | nein |
| `haustuer/status` | ESP32 → HA | `online` / `offline` (Last Will) | ja |
| `haustuer/kalibrieren` | HA → ESP32 | `start` | nein |
| `haustuer/helligkeit` | HA → ESP32 | PWM-Wert 0–255 | ja |
| `haustuer/klingelfarbe` | HA → ESP32 | Farbname (siehe unten) | ja |
| `haustuer/klingelstaerke` | HA → ESP32 | `Normal` / `Fett` / `Extra Fett` / `Ultra Fett` | ja |
| `haustuer/klingelposition` | HA → ESP32 | Positionsnummer `1`–`15` | ja |
| `haustuer/klingelton_dauer` | HA → ESP32 | Dauer in ms (0–`BELL_MAX_DURATION_MS`) | ja |
| `haustuer/touch_modus` | HA → ESP32 | `ON` (vollflächig) / `OFF` (nur Button) | ja |
| `haustuer/klingel_ausblenden` | HA → ESP32 | `ON` (Overlay ausgeblendet) / `OFF` (sichtbar) | ja |
| `homeassistant/.../config` | ESP32 → HA | MQTT-Discovery-Payloads | ja |

Beim ersten erfolgreichen MQTT-Connect nach dem Boot veröffentlicht das Gerät automatisch
Discovery-Configs für Home Assistant:

- Binary-Sensor "Klingel" (Klingel-Event)
- Binary-Sensor "Verbindung" (device_class `connectivity`, zeigt online/offline anhand des LWT-Topics)
- `text`-Entity "Bild" (Dateiname/URL auch manuell aus HA heraus setzbar, als Ergänzung zur üblichen Automation)
- Button "Touch kalibrieren" (löst die 4-Ecken-Kalibrierung aus)
- Number-Entity "Helligkeit"
- Drei `select`-Entities (echte Dropdown-Auswahl, keine rohen Zahlencodes nötig) für
  Klingel-Button-Farbe, -Schriftstärke und -Position
- Number-Entity "Klingelton Dauer" (physischer MOSFET-Impuls)
- `switch`-Entity "Touch vollflächig"
- `switch`-Entity "Klingel-Overlay ausblenden"

Dieselben Optionen wie auf der Weboberfläche unter `/klingelstil`, `/klingelton`, `/touchmodus`
bzw. `/overlay`. Jedes der drei Klingel-Button-Topics ändert nur sein eigenes Feld, die jeweils
anderen beiden bleiben unverändert.

Farboptionen (aus `src/display/button_presets.h`): `Weiss`, `Rot`, `Gruen`, `Blau`, `Gelb`,
`Orange`, `Cyan`, `Magenta`, `Violett`, `Rosa`, `Silber`, `Himmelblau`. Positionsnummern
entsprechen der Nummerierung im 3×5-Raster auf der Weboberfläche (Zeile für Zeile, links nach
rechts, 1–15).

## Physischer Klingelton

Ein logic-level N-Kanal-MOSFET (z.B. AO3400 oder IRF3708) an `BELL_MOSFET_PIN` (siehe
`src/config.h`) wird bei jedem Klingel-Tastendruck für eine konfigurierbare Dauer eingeschaltet -
**unabhängig** von der 0,5s-Bildschirm-Invertierung, mit eigener nicht-blockierender
Zustandsmaschine (`src/display/bell_output.h`). Default 500ms, änderbar über die Weboberfläche
(`/klingelton`-Abschnitt auf der Startseite) oder per MQTT (`haustuer/klingelton_dauer`, in NVS
persistiert). Der GPIO ist logic-level und steuert das MOSFET-Gate direkt mit 3,3V an - kein
Levelshifter nötig.

Bis zur eigentlichen Initialisierung liegt der Pin am internen Pulldown des ESP32 (siehe
`BellOutput::earlyInit()`, als erste Zeile in `setup()` aufgerufen), statt zu floaten - ohne das
kann das Gate je nach MOSFET (in der Praxis beim IRF3708 beobachtet) während der Boot-Phase
Undefiniertes tun und den MOSFET kurz leitend werden lassen.

## Touch-Bereich (vollflächig vs. Button)

Standardmäßig löst nur eine Berührung innerhalb der Klingel-Button-Trefferfläche das Klingeln
aus. Über die Checkbox "Vollflächig" auf der Startseite oder per MQTT (`haustuer/touch_modus`,
`ON`/`OFF`) lässt sich umstellen, sodass **jede** Berührung irgendwo auf dem Bildschirm klingelt -
z.B. sinnvoll bei sehr kleinen/schwer zu treffenden Buttons oder wenn ohnehin das ganze Display
als Klingelfläche dienen soll. Wirkt sofort (kein Neustart nötig), in NVS persistiert.

## Klingel-Overlay ausblenden

Über die Checkbox "Ausblenden" auf der Startseite oder per MQTT (`haustuer/klingel_ausblenden`,
`ON`/`OFF`) lässt sich Rahmen + "Klingel"-Beschriftung des Overlays komplett ausblenden, sodass nur
noch das Foto zu sehen ist - z.B. sinnvoll, wenn der Button optisch stören würde. Die
Touch-Trefferfläche bleibt davon unberührt aktiv (siehe Touch-Bereich oben); nur die sichtbare
Outline verschwindet. Wirkt sofort (kein Neustart nötig), in NVS persistiert.

## Touch-Kalibrierung

Die 4-Ecken-Fadenkreuz-Kalibrierung lässt sich auf zwei Wegen auslösen:

1. **Per MQTT:** `haustuer/kalibrieren` mit Payload `start` senden (braucht WLAN/MQTT).
2. **Lokal, immer verfügbar:** 10× auf das Display tippen innerhalb von 7 Sekunden – funktioniert
   unabhängig von WLAN-Status und aktueller Kalibrierung.

Die ermittelten Werte werden dauerhaft im NVS gespeichert.

## Architektur

Die Firmware läuft auf beiden ESP32-Kernen (FreeRTOS):

- **Core 0** (`src/net/`): WLAN-Provisionierung + Reconnect-Watchdog, MQTT, HTTP-Bilddownload,
  OTA-Webserver.
- **Core 1** (`src/display/`, Arduinos Standard-`loop()`): Display-Rendering, Touch-Polling,
  Klingel-Button-Overlay, Kalibrierung.

Beide Kerne kommunizieren ausschließlich über zwei FreeRTOS-Queues (`src/ipc/messages.h`), da
weder die SPI-Peripherie noch der MQTT-Client threadsicher zwischen den Kernen geteilt werden.

## Lizenz

[PolyForm Noncommercial License 1.0.0](LICENSE) - nichtkommerzielle Nutzung frei erlaubt,
kommerzielle Nutzung ausgeschlossen.
