#include "upload_route.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include "../config.h"
#include "../ipc/messages.h"
#include "../storage/button_style.h"
#include "../storage/bell_settings.h"
#include "../storage/touch_mode.h"
#include "../storage/overlay_mode.h"
#include "../display/button_presets.h"

namespace {

const char* UPLOAD_TMP_PATH = "/upload.tmp";
// Großzügig genug für ein SCREEN_WIDTH x SCREEN_HEIGHT-Baseline-JPEG in guter Qualität; verhindert,
// dass eine versehentlich falsche (riesige) Datei den Flash vollschreibt.
const size_t MAX_UPLOAD_BYTES = 200 * 1024;

// Wandelt RGB565 in einen CSS-rgb()-String für die Farbvorschau um (5/6/5 Bit auf 8 Bit
// hochskaliert) - Farbliste selbst kommt aus display/button_presets.h (einzige Quelle, auch von
// der MQTT-Discovery in net/mqtt_manager.cpp genutzt), hier also keine doppelte Pflege nötig.
String rgb565ToCss(uint16_t c) {
  uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (c & 0x1F) * 255 / 31;
  return "rgb(" + String(r) + "," + String(g) + "," + String(b) + ")";
}

String buildColorPickerHtml() {
  String html;
  for (int i = 0; i < COLOR_OPTION_COUNT; i++) {
    const ColorOption& c = COLOR_OPTIONS[i];
    html += "<label style=\"display:inline-flex;align-items:center;gap:4px;margin:2px 10px 2px 0;\">";
    html += "<input type=\"radio\" name=\"farbe\" value=\"" + String(c.rgb565) + "\">";
    html += "<span style=\"width:18px;height:18px;background:" + rgb565ToCss(c.rgb565) +
            ";border:1px solid #888;display:inline-block;\"></span>";
    html += String(c.name);
    html += "</label>";
  }
  return html;
}

String buildPositionGridHtml() {
  String html = "<div style=\"display:inline-block;\">";
  for (int i = 1; i <= POSITION_COUNT; i++) {
    GridPos p = gridPositionByIndex(i);
    html += "<label style=\"display:inline-block;width:52px;text-align:center;border:1px solid #888;"
            "padding:8px 0;margin:2px;cursor:pointer;\">";
    html += "<input type=\"radio\" name=\"pos\" value=\"" + String(p.x) + "," + String(p.y) + "\"><br>";
    html += String(i);
    html += "</label>";
    if (i % POSITION_GRID_COLS == 0) {
      html += "<br>";
    }
  }
  html += "</div>";
  return html;
}

// Zuschnitt/Skalierung auf exakt %WIDTH%x%HEIGHT% passiert per <canvas> im Browser, VOR dem
// eigentlichen Upload - beliebige Fotos (auch deutlich größer/anders proportioniert) werden so
// automatisch passend gemacht, ohne den ESP32 mit Bildverarbeitung zu belasten (zu wenig RAM, um
// z.B. ein Handyfoto in voller Auflösung zu dekodieren). canvas.toBlob('image/jpeg', ...) liefert
// dabei immer Baseline-JPEG, nie progressive - erfüllt die Formatvorgabe also nebenbei mit.
// Serverseitig (handleUploadChunk()) wird die Größe trotzdem nochmal geprüft, als Absicherung für
// Uploads ohne JS (z.B. per curl) - die müssen weiterhin exakt passend vorliegen.
const char UPLOAD_FORM[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Haustuerklingel - Bild hochladen</title></head>
<body style="font-family:sans-serif;max-width:480px;margin:2em auto;">
<h1>Haust&uuml;rklingeldisplay</h1>
<h2>Bild hochladen</h2>
<p>Beliebiges Foto wählen - wird automatisch zentriert auf %WIDTH%x%HEIGHT%px eingepasst (ganzes Bild sichtbar, noetigenfalls schwarze Balken).</p>
<input type="file" id="datei" accept="image/*"><br><br>
<button type="button" onclick="hochladen()">Hochladen</button>
<p id="status"></p>

<h2>Klingel-Button</h2>
<p>Farbe (Rahmen + Beschriftung):</p>
<div>%FARBAUSWAHL%</div>
<p>Schriftst&auml;rke:</p>
<p>
<label style="margin-right:12px;"><input type="radio" name="staerke" value="0"> Normal</label>
<label style="margin-right:12px;"><input type="radio" name="staerke" value="1"> Fett</label>
<label style="margin-right:12px;"><input type="radio" name="staerke" value="2"> Extra Fett</label>
<label><input type="radio" name="staerke" value="3"> Ultra Fett</label>
</p>
<p>Position:</p>
%POSITIONSRASTER%
<br>
<button type="button" onclick="stilUebernehmen()">&Uuml;bernehmen</button>
<p id="stilStatus"></p>

<h2>Touch-Bereich</h2>
<p>
<label><input type="checkbox" id="touchVollflaechig" onchange="touchModusAendern()"> Vollfl&auml;chig (jede Ber&uuml;hrung klingelt, nicht nur der Button)</label>
</p>
<p id="touchStatus"></p>

<h2>Klingel-Overlay</h2>
<p>
<label><input type="checkbox" id="overlayAusblenden" onchange="overlayAendern()"> Ausblenden (Foto ohne Rahmen/Beschriftung, Touch-Bereich bleibt aktiv)</label>
</p>
<p id="overlayStatus"></p>

<h2>Physischer Klingelton (AO3400)</h2>
<p>Unabh&auml;ngig von der Bildschirm-Animation - eigene Dauer f&uuml;r den MOSFET-Impuls:</p>
<p>
<label>Dauer (ms): <input type="number" id="klingeltonDauer" min="0" max="%KLINGELTON_MAX%" style="width:80px;"></label>
</p>
<button type="button" onclick="klingeltonUebernehmen()">&Uuml;bernehmen</button>
<p id="klingeltonStatus"></p>

<p><a href="/einstellungen">Einstellungen (WLAN/MQTT/Home Assistant)</a></p>
<script>
const zielW = %WIDTH%, zielH = %HEIGHT%;
function hochladen() {
  const eingabe = document.getElementById('datei');
  const status = document.getElementById('status');
  if (!eingabe.files.length) { status.textContent = 'Bitte zuerst ein Bild waehlen.'; return; }
  status.textContent = 'Verarbeite...';

  const bild = new Image();
  const reader = new FileReader();
  reader.onload = function(e) { bild.src = e.target.result; };
  reader.onerror = function() { status.textContent = 'Fehler beim Lesen der Datei.'; };
  bild.onload = function() {
    const canvas = document.createElement('canvas');
    canvas.width = zielW;
    canvas.height = zielH;
    const ctx = canvas.getContext('2d');
    // "contain": ganzes Bild bleibt sichtbar (nichts wird abgeschnitten), dafuer noetigenfalls
    // schwarze Balken oben/unten oder links/rechts - Bild wird dabei zentriert.
    ctx.fillStyle = '#000000';
    ctx.fillRect(0, 0, zielW, zielH);
    const skala = Math.min(zielW / bild.width, zielH / bild.height);
    const dw = bild.width * skala, dh = bild.height * skala;
    const dx = (zielW - dw) / 2, dy = (zielH - dh) / 2;
    ctx.drawImage(bild, 0, 0, bild.width, bild.height, dx, dy, dw, dh);
    canvas.toBlob(function(blob) {
      const daten = new FormData();
      daten.append('bild', blob, 'upload.jpg');
      status.textContent = 'Laedt hoch...';
      fetch('/upload', { method: 'POST', body: daten })
        .then(function(r) { return r.text(); })
        .then(function(t) { status.textContent = t; })
        .catch(function(err) { status.textContent = 'Fehler: ' + err; });
    }, 'image/jpeg', 0.85);
  };
  bild.onerror = function() { status.textContent = 'Datei ist kein unterstuetztes Bildformat.'; };
  reader.readAsDataURL(eingabe.files[0]);
}

const aktuelleFarbe = %AKTUELLE_FARBE%;
const aktuelleStaerke = %AKTUELLE_STAERKE%;
const aktuellePos = "%AKTUELLE_POS%";
document.querySelectorAll('input[name="farbe"]').forEach(function(r) {
  if (parseInt(r.value) === aktuelleFarbe) { r.checked = true; }
});
document.querySelectorAll('input[name="staerke"]').forEach(function(r) {
  if (parseInt(r.value) === aktuelleStaerke) { r.checked = true; }
});
document.querySelectorAll('input[name="pos"]').forEach(function(r) {
  if (r.value === aktuellePos) { r.checked = true; }
});

function stilUebernehmen() {
  const farbeEl = document.querySelector('input[name="farbe"]:checked');
  const staerkeEl = document.querySelector('input[name="staerke"]:checked');
  const posEl = document.querySelector('input[name="pos"]:checked');
  const status = document.getElementById('stilStatus');
  if (!farbeEl || !staerkeEl || !posEl) { status.textContent = 'Bitte Farbe, Schriftstaerke und Position waehlen.'; return; }
  const teile = posEl.value.split(',');
  status.textContent = 'Wird angewendet...';
  const params = new URLSearchParams({ farbe: farbeEl.value, staerke: staerkeEl.value, x: teile[0], y: teile[1] });
  fetch('/klingelstil?' + params.toString(), { method: 'POST' })
    .then(function(r) { return r.text(); })
    .then(function(t) { status.textContent = t; })
    .catch(function(err) { status.textContent = 'Fehler: ' + err; });
}

document.getElementById('touchVollflaechig').checked = %AKTUELL_TOUCH_VOLLFLAECHIG%;

function touchModusAendern() {
  const an = document.getElementById('touchVollflaechig').checked;
  const status = document.getElementById('touchStatus');
  status.textContent = 'Wird angewendet...';
  fetch('/touchmodus?vollflaechig=' + (an ? '1' : '0'), { method: 'POST' })
    .then(function(r) { return r.text(); })
    .then(function(t) { status.textContent = t; })
    .catch(function(err) { status.textContent = 'Fehler: ' + err; });
}

document.getElementById('overlayAusblenden').checked = %AKTUELL_OVERLAY_AUSGEBLENDET%;

function overlayAendern() {
  const an = document.getElementById('overlayAusblenden').checked;
  const status = document.getElementById('overlayStatus');
  status.textContent = 'Wird angewendet...';
  fetch('/overlay?ausgeblendet=' + (an ? '1' : '0'), { method: 'POST' })
    .then(function(r) { return r.text(); })
    .then(function(t) { status.textContent = t; })
    .catch(function(err) { status.textContent = 'Fehler: ' + err; });
}

document.getElementById('klingeltonDauer').value = %AKTUELLE_KLINGELTON_DAUER%;

function klingeltonUebernehmen() {
  const dauer = document.getElementById('klingeltonDauer').value;
  const status = document.getElementById('klingeltonStatus');
  status.textContent = 'Wird angewendet...';
  fetch('/klingelton?dauer=' + encodeURIComponent(dauer), { method: 'POST' })
    .then(function(r) { return r.text(); })
    .then(function(t) { status.textContent = t; })
    .catch(function(err) { status.textContent = 'Fehler: ' + err; });
}
</script>
</body></html>
)HTML";

// Zustand des gerade laufenden Uploads - AsyncWebServer ruft den Upload-Callback mehrfach (einmal
// pro Chunk) auf, der Request-Callback erst danach einmalig zum Senden der Antwort.
bool g_uploadFailed = false;
String g_uploadError;
String g_uploadFilename;
size_t g_uploadBytesWritten = 0;
File g_uploadFile;

void handleUploadChunk(AsyncWebServerRequest* /*request*/, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    g_uploadFailed = false;
    g_uploadError = "";
    g_uploadBytesWritten = 0;
    g_uploadFilename = filename;
    LittleFS.remove(UPLOAD_TMP_PATH);
    g_uploadFile = LittleFS.open(UPLOAD_TMP_PATH, "w");
    if (!g_uploadFile) {
      g_uploadFailed = true;
      g_uploadError = "LittleFS-Schreibfehler";
    }
  }

  if (!g_uploadFailed && g_uploadFile && len > 0) {
    if (g_uploadBytesWritten + len > MAX_UPLOAD_BYTES) {
      g_uploadFailed = true;
      g_uploadError = "Datei zu gross (max. " + String(MAX_UPLOAD_BYTES / 1024) + "KB)";
    } else {
      g_uploadFile.write(data, len);
      g_uploadBytesWritten += len;
    }
  }

  if (!final) {
    return;
  }

  if (g_uploadFile) {
    g_uploadFile.close();
  }
  if (g_uploadFailed) {
    LittleFS.remove(UPLOAD_TMP_PATH);
    return;
  }

  uint16_t w = 0, h = 0;
  if (TJpgDec.getFsJpgSize(&w, &h, UPLOAD_TMP_PATH, LittleFS) != JDR_OK || w != SCREEN_WIDTH || h != SCREEN_HEIGHT) {
    g_uploadFailed = true;
    g_uploadError = "Bild muss exakt " + String(SCREEN_WIDTH) + "x" + String(SCREEN_HEIGHT) +
                     "px sein (erkannt: " + String(w) + "x" + String(h) + ")";
    LittleFS.remove(UPLOAD_TMP_PATH);
    return;
  }

  // Gleiches atomares Muster wie ImageCache::updateImage(): erst nach bestätigt erfolgreicher
  // Validierung umbenennen, Flag-Datei erst danach schreiben.
  LittleFS.remove(CURRENT_IMAGE_PATH);
  LittleFS.rename(UPLOAD_TMP_PATH, CURRENT_IMAGE_PATH);
  File flag = LittleFS.open(CURRENT_IMAGE_FLAG_PATH, "w");
  if (flag) {
    flag.print(g_uploadFilename.length() ? g_uploadFilename : "upload.jpg");
    flag.close();
  }

  ToDisplayMsg msg{};
  msg.type = ToDisplayMsgType::NewImageAvailable;
  xQueueSend(g_toDisplayQueue, &msg, 0);
}

}  // namespace

void registerUploadRoute(AsyncWebServer& server) {
  auto serveForm = [](AsyncWebServerRequest* request) {
    String html = UPLOAD_FORM;
    html.replace("%WIDTH%", String(SCREEN_WIDTH));
    html.replace("%HEIGHT%", String(SCREEN_HEIGHT));
    html.replace("%FARBAUSWAHL%", buildColorPickerHtml());
    html.replace("%POSITIONSRASTER%", buildPositionGridHtml());
    html.replace("%KLINGELTON_MAX%", String(BELL_MAX_DURATION_MS));
    // Aktuellen Stil vorausfüllen - reiner NVS-Lesezugriff, siehe storage/button_style.h bzw.
    // storage/bell_settings.h.
    ButtonStyle style = ButtonStyleManager::instance().load();
    html.replace("%AKTUELLE_FARBE%", String(style.color));
    html.replace("%AKTUELLE_STAERKE%", String(static_cast<int>(style.weight)));
    html.replace("%AKTUELLE_POS%", String(style.x) + "," + String(style.y));
    html.replace("%AKTUELLE_KLINGELTON_DAUER%", String(BellSettingsManager::instance().loadDurationMs()));
    html.replace("%AKTUELL_TOUCH_VOLLFLAECHIG%", TouchModeManager::instance().loadFullscreen() ? "true" : "false");
    html.replace("%AKTUELL_OVERLAY_AUSGEBLENDET%", OverlayModeManager::instance().loadHidden() ? "true" : "false");
    request->send(200, "text/html", html);
  };
  // Zusätzlich zu /upload auch als Startseite (/) erreichbar, damit man beim Aufruf der reinen
  // IP/Hostname direkt auf dem Upload-Formular landet statt auf einem 404.
  server.on("/", HTTP_GET, serveForm);
  server.on("/upload", HTTP_GET, serveForm);

  server.on(
      "/upload", HTTP_POST,
      [](AsyncWebServerRequest* request) {
        // Läuft erst NACH dem letzten Chunk in handleUploadChunk() - Ergebnis dort ausgewertet.
        if (g_uploadFailed) {
          request->send(400, "text/plain", "Fehler: " + g_uploadError);
        } else {
          request->send(200, "text/plain", "OK: Bild aktualisiert (" + String(g_uploadBytesWritten) + " Bytes)");
        }
      },
      handleUploadChunk);

  server.on("/klingelstil", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("farbe") || !request->hasParam("staerke") || !request->hasParam("x") || !request->hasParam("y")) {
      request->send(400, "text/plain", "Parameter fehlt (farbe/staerke/x/y)");
      return;
    }
    uint16_t color = static_cast<uint16_t>(request->getParam("farbe")->value().toInt());
    uint8_t weight = static_cast<uint8_t>(constrain(request->getParam("staerke")->value().toInt(), 0, 3));
    int16_t x = static_cast<int16_t>(request->getParam("x")->value().toInt());
    int16_t y = static_cast<int16_t>(request->getParam("y")->value().toInt());

    ToDisplayMsg msg{};
    msg.type = ToDisplayMsgType::SetButtonStyle;
    msg.buttonColor = color;
    msg.buttonWeight = weight;
    msg.buttonX = x;
    msg.buttonY = y;
    xQueueSend(g_toDisplayQueue, &msg, 0);

    request->send(200, "text/plain", "OK: Klingel-Button aktualisiert");
  });

  server.on("/klingelton", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("dauer")) {
      request->send(400, "text/plain", "Parameter fehlt (dauer)");
      return;
    }
    uint16_t ms = static_cast<uint16_t>(constrain(request->getParam("dauer")->value().toInt(), 0, BELL_MAX_DURATION_MS));

    ToDisplayMsg msg{};
    msg.type = ToDisplayMsgType::SetBellDuration;
    msg.bellDurationMs = ms;
    xQueueSend(g_toDisplayQueue, &msg, 0);

    request->send(200, "text/plain", "OK: Klingelton-Dauer aktualisiert (" + String(ms) + "ms)");
  });

  server.on("/touchmodus", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("vollflaechig")) {
      request->send(400, "text/plain", "Parameter fehlt (vollflaechig)");
      return;
    }
    bool fullscreen = request->getParam("vollflaechig")->value() == "1";

    ToDisplayMsg msg{};
    msg.type = ToDisplayMsgType::SetTouchMode;
    msg.touchFullscreen = fullscreen;
    xQueueSend(g_toDisplayQueue, &msg, 0);

    request->send(200, "text/plain", fullscreen ? "OK: Touch ist jetzt vollflaechig" : "OK: Touch ist jetzt auf den Button begrenzt");
  });

  server.on("/overlay", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("ausgeblendet")) {
      request->send(400, "text/plain", "Parameter fehlt (ausgeblendet)");
      return;
    }
    bool hidden = request->getParam("ausgeblendet")->value() == "1";

    ToDisplayMsg msg{};
    msg.type = ToDisplayMsgType::SetOverlayVisible;
    msg.overlayHidden = hidden;
    xQueueSend(g_toDisplayQueue, &msg, 0);

    request->send(200, "text/plain", hidden ? "OK: Overlay ist jetzt ausgeblendet" : "OK: Overlay ist jetzt sichtbar");
  });
}
