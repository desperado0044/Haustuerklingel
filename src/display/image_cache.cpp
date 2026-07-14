#include "image_cache.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "../config.h"
#include "../storage/credentials.h"

namespace {
const char* TMP_IMAGE_PATH = "/current.jpg.tmp";
}

ImageCache& ImageCache::instance() {
  static ImageCache cache;
  return cache;
}

bool ImageCache::begin() {
  // format-on-fail=true: das ist ein Wegwerf-Cache (wird bei der nächsten MQTT-Nachricht oder
  // beim Reconnect neu von HA geladen), kein Datenspeicher, für den es sich lohnt den Boot bei
  // einem defekten Dateisystem scheitern zu lassen.
  return LittleFS.begin(true);
}

bool ImageCache::hasCachedImage() {
  return LittleFS.exists(CURRENT_IMAGE_PATH);
}

ImageCache::UpdateResult ImageCache::updateImage(const String& filename, String& errorOut) {
  String cachedName;
  File flagFile = LittleFS.open(CURRENT_IMAGE_FLAG_PATH, "r");
  if (flagFile) {
    cachedName = flagFile.readString();
    flagFile.close();
  }
  if (cachedName == filename) {
    return UpdateResult::NoChange;
  }

  // filename darf entweder ein kurzer Dateiname sein (weiterhin relativ zur HA-Basis-URL +
  // HA_IMAGE_PATH_PREFIX aufgelöst - der übliche Fall für Bilder aus Home Assistants www-Ordner)
  // oder bereits eine vollständige URL (z.B. ein Bild von einer anderen Quelle im Netz) - dann
  // wird sie unverändert verwendet und die HA-Basis-URL spielt keine Rolle.
  String url;
  if (filename.startsWith("http://") || filename.startsWith("https://")) {
    url = filename;
  } else {
    String baseUrl = CredentialsManager::instance().getHaBaseUrl();
    if (baseUrl.length() == 0) {
      errorOut = "HA-Basis-URL nicht konfiguriert";
      return UpdateResult::Failed;
    }
    url = baseUrl + HA_IMAGE_PATH_PREFIX + filename;
  }

  HTTPClient http;
  http.setTimeout(HTTP_DOWNLOAD_TIMEOUT_MS);

  // HTTPClient::begin(String) allein nutzt intern IMMER einen reinen (unverschlüsselten)
  // WiFiClient, auch für https-URLs - der TLS-Handshake würde also fehlschlagen. Für https
  // deshalb explizit WiFiClientSecure verwenden. setInsecure() = keine Zertifikatsprüfung; für
  // beliebige Bildquellen aus dem Internet ohne Cert-Pinning hier ausreichend (kein
  // sicherheitskritischer Anwendungsfall).
  WiFiClientSecure secureClient;
  bool began;
  if (url.startsWith("https://")) {
    secureClient.setInsecure();
    began = http.begin(secureClient, url);
  } else {
    began = http.begin(url);
  }
  if (!began) {
    errorOut = "HTTP-Verbindung fehlgeschlagen";
    return UpdateResult::Failed;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    errorOut = "HTTP " + String(httpCode) + ": " + filename;
    http.end();
    return UpdateResult::Failed;
  }

  int contentLength = http.getSize();  // -1, falls der Server kein Content-Length gesendet hat.
  WiFiClient* stream = http.getStreamPtr();

  // Erst in eine Temp-Datei heruntergeladen und nur bei vollem Erfolg eingewechselt, damit ein
  // fehlgeschlagener/unvollständiger Download nie das letzte gute /current.jpg beschädigen kann
  // (Vorgabe aus dem Lastenheft: altes Bild bleibt bei Fehler erhalten, Flag-Datei wird erst nach
  // bestätigt vollständigem Schreiben aktualisiert).
  File out = LittleFS.open(TMP_IMAGE_PATH, "w");
  if (!out) {
    errorOut = "LittleFS-Schreibfehler";
    http.end();
    return UpdateResult::Failed;
  }

  size_t written = 0;
  uint8_t buf[512];
  unsigned long idleDeadline = millis() + HTTP_DOWNLOAD_TIMEOUT_MS;
  while (http.connected() && (contentLength < 0 || written < (size_t)contentLength)) {
    size_t avail = stream->available();
    if (avail == 0) {
      if (millis() > idleDeadline) {
        break;
      }
      delay(1);
      continue;
    }
    size_t toRead = avail < sizeof(buf) ? avail : sizeof(buf);
    size_t n = stream->readBytes(buf, toRead);
    out.write(buf, n);
    written += n;
    idleDeadline = millis() + HTTP_DOWNLOAD_TIMEOUT_MS;
  }
  out.close();
  http.end();

  bool complete = written > 0 && (contentLength < 0 || written == (size_t)contentLength);
  if (!complete) {
    LittleFS.remove(TMP_IMAGE_PATH);
    errorOut = "Download unvollständig (" + String(written) + "/" + String(contentLength) + " Bytes)";
    return UpdateResult::Failed;
  }

  LittleFS.remove(CURRENT_IMAGE_PATH);
  LittleFS.rename(TMP_IMAGE_PATH, CURRENT_IMAGE_PATH);

  File flag = LittleFS.open(CURRENT_IMAGE_FLAG_PATH, "w");
  if (flag) {
    flag.print(filename);
    flag.close();
  }

  return UpdateResult::Updated;
}
