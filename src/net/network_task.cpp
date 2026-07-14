#include "network_task.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>
#include "../config.h"
#include "../storage/credentials.h"
#include "mqtt_manager.h"
#include "ota_manager.h"
#include "upload_route.h"
#include "settings_route.h"
#include "../ipc/messages.h"
#include "../display/image_cache.h"

namespace {

AsyncWebServer g_server(80);

void sendStatus(const String& text) {
  ToDisplayMsg msg{};
  msg.type = ToDisplayMsgType::StatusText;
  strncpy(msg.text, text.c_str(), sizeof(msg.text) - 1);
  msg.text[sizeof(msg.text) - 1] = '\0';
  xQueueSend(g_toDisplayQueue, &msg, 0);
}

// Blockierend (läuft nur auf Core 0 - Core 1's Display-Task rendert währenddessen weiter das
// gecachte Bild/den Statustext, siehe display/display_task.cpp). Versucht zuerst gespeicherte
// Zugangsdaten; fällt nur bei einem echten ersten Boot oder vollständig fehlgeschlagenem
// Reconnect auf das Captive Portal des WiFiManager zurück.
void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(OTA_HOSTNAME);

  String savedSsid = CredentialsManager::instance().getWifiSsid();
  String savedPass = CredentialsManager::instance().getWifiPass();

  bool connected = false;
  if (savedSsid.length() > 0) {
    sendStatus("Verbinde mit " + savedSsid + " ...");
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);
    }
    connected = (WiFi.status() == WL_CONNECTED);
  }

  if (!connected) {
    sendStatus("Hotspot geoeffnet: " WIFI_AP_NAME " (4.3.2.1)");

    WiFiManager wm;
    wm.setDebugOutput(false);
    wm.setConfigPortalBlocking(true);
    wm.setHostname(OTA_HOSTNAME);
    wm.setAPStaticIPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0));

    // MQTT-Broker + Home-Assistant-Basis-URL werden auf derselben Captive-Portal-Seite wie die
    // WLAN-Zugangsdaten erfasst, da keins von beiden im Voraus erratbar ist und es in diesem
    // Projekt keine separate Provisionierungs-UI gibt.
    String mqttPortStr = String(CredentialsManager::instance().getMqttPort());
    WiFiManagerParameter mqttBrokerParam("mqtt_broker", "MQTT Broker", CredentialsManager::instance().getMqttBroker().c_str(), 64);
    WiFiManagerParameter mqttPortParam("mqtt_port", "MQTT Port", mqttPortStr.c_str(), 6);
    WiFiManagerParameter mqttUserParam("mqtt_user", "MQTT User", CredentialsManager::instance().getMqttUser().c_str(), 32);
    WiFiManagerParameter mqttPassParam("mqtt_pass", "MQTT Passwort", CredentialsManager::instance().getMqttPass().c_str(), 32);
    WiFiManagerParameter haUrlParam("ha_url", "Home Assistant Basis-URL (z.B. http://192.168.1.10:8123)", CredentialsManager::instance().getHaBaseUrl().c_str(), 64);
    wm.addParameter(&mqttBrokerParam);
    wm.addParameter(&mqttPortParam);
    wm.addParameter(&mqttUserParam);
    wm.addParameter(&mqttPassParam);
    wm.addParameter(&haUrlParam);

    if (strlen(WIFI_AP_PASSWORD) > 0) {
      connected = wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);
    } else {
      connected = wm.autoConnect(WIFI_AP_NAME);
    }

    if (connected) {
      CredentialsManager::instance().saveWifi(WiFi.SSID(), WiFi.psk());
      CredentialsManager::instance().saveMqtt(mqttBrokerParam.getValue(), String(mqttPortParam.getValue()).toInt(),
                                               mqttUserParam.getValue(), mqttPassParam.getValue());
      CredentialsManager::instance().saveHaBaseUrl(haUrlParam.getValue());
    }
  }

  if (connected) {
    String ip = WiFi.localIP().toString();
    sendStatus("WLAN verbunden: " + ip);
    Serial.print("IP-Adresse: ");
    Serial.println(ip);
    if (MDNS.begin(OTA_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("Erreichbar als: " OTA_HOSTNAME ".local");
    }
  } else {
    sendStatus("WLAN nicht verbunden");
  }
}

// Manueller Test-/Debug-Endpunkt, der ImageCache direkt anstößt und dabei komplett an MQTT und
// Home Assistant vorbeigeht - praktisch, um die Bildanzeige isoliert zu testen. Aufruf z.B.:
// http://<ip-oder-hostname>/testbild?datei=test.jpg&basis=http://192.168.2.40:8123
// "basis" ist optional und überschreibt (persistent) die gespeicherte HA-Basis-URL.
void registerDebugRoutes() {
  g_server.on("/testbild", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("datei")) {
      request->send(400, "text/plain", "Parameter 'datei' fehlt, z.B. ?datei=test.jpg");
      return;
    }
    String filename = request->getParam("datei")->value();
    if (request->hasParam("basis")) {
      CredentialsManager::instance().saveHaBaseUrl(request->getParam("basis")->value());
    }

    String error;
    ImageCache::UpdateResult result = ImageCache::instance().updateImage(filename, error);
    switch (result) {
      case ImageCache::UpdateResult::Updated: {
        ToDisplayMsg msg{};
        msg.type = ToDisplayMsgType::NewImageAvailable;
        xQueueSend(g_toDisplayQueue, &msg, 0);
        request->send(200, "text/plain", "OK: Bild aktualisiert (" + filename + ")");
        break;
      }
      case ImageCache::UpdateResult::NoChange:
        request->send(200, "text/plain", "Kein Wechsel - Dateiname stimmt bereits mit dem Cache ueberein");
        break;
      case ImageCache::UpdateResult::Failed:
        request->send(500, "text/plain", "Fehler: " + error);
        break;
    }
  });
}

}  // namespace

void networkTaskEntry(void* /*pvParameters*/) {
  CredentialsManager::instance().begin();
  connectWifi();  // Kann für die Dauer des interaktiven WiFiManager-Captive-Portals blockieren -
                   // absichtlich noch NICHT beim Watchdog angemeldet, sonst reißt der Timeout
                   // während der Nutzer-Provisionierung ab und der Node bootet in einer Schleife.

  esp_task_wdt_add(NULL);  // ab hier überwacht der Watchdog nur noch den Steady-State-Betrieb.

  MqttManager::instance().begin();
  OtaManager::instance().begin(g_server);
  registerDebugRoutes();
  registerUploadRoute(g_server);
  registerSettingsRoute(g_server);
  g_server.begin();

  unsigned long lastWifiCheck = 0;

  for (;;) {
    esp_task_wdt_reset();
    ElegantOTA.loop();

    if (WiFi.status() != WL_CONNECTED) {
      // Reconnect-Watchdog: versucht still per Timer erneut, fällt nie zurück ins blockierende
      // AP-Portal, sobald einmal erfolgreich verbunden (Vorgabe aus dem Lastenheft) - ein
      // erneutes Portal würde Aus-/Einstecken oder Neuflashen erfordern, was im laufenden
      // Betrieb nicht akzeptabel ist.
      if (millis() - lastWifiCheck > WIFI_RECONNECT_CHECK_MS) {
        WiFi.reconnect();
        lastWifiCheck = millis();
      }
    } else {
      MqttManager::instance().loop();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
