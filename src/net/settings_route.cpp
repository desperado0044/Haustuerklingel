#include "settings_route.h"
#include "../storage/credentials.h"

namespace {

const char SETTINGS_FORM[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Haustuerklingel - Einstellungen</title></head>
<body style="font-family:sans-serif;max-width:480px;margin:2em auto;">
<h1>Haust&uuml;rklingeldisplay</h1>
<h2>Einstellungen</h2>
<form method="POST" action="/einstellungen">
<fieldset>
<legend>WLAN</legend>
<label>SSID<br><input type="text" name="wifi_ssid" value="%WIFI_SSID%"></label><br><br>
<label>Passwort (leer lassen = unver&auml;ndert)<br><input type="password" name="wifi_pass" placeholder="unveraendert"></label>
</fieldset><br>
<fieldset>
<legend>MQTT</legend>
<label>Broker<br><input type="text" name="mqtt_broker" value="%MQTT_BROKER%"></label><br><br>
<label>Port<br><input type="number" name="mqtt_port" value="%MQTT_PORT%"></label><br><br>
<label>User<br><input type="text" name="mqtt_user" value="%MQTT_USER%"></label><br><br>
<label>Passwort (leer lassen = unver&auml;ndert)<br><input type="password" name="mqtt_pass" placeholder="unveraendert"></label>
</fieldset><br>
<fieldset>
<legend>Home Assistant</legend>
<label>Basis-URL<br><input type="text" name="ha_url" value="%HA_URL%" placeholder="http://192.168.1.10:8123"></label>
</fieldset><br><br>
<input type="submit" value="Speichern &amp; neu starten">
</form>
<p><a href="/">Zur&uuml;ck zum Bild-Upload</a></p>
</body></html>
)HTML";

const char SETTINGS_SAVED[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<title>Gespeichert</title></head>
<body style="font-family:sans-serif;max-width:480px;margin:2em auto;">
<h1>Gespeichert</h1>
<p>Einstellungen wurden gespeichert. Das Ger&auml;t startet jetzt neu ...</p>
</body></html>
)HTML";

String paramOr(AsyncWebServerRequest* request, const char* name, const String& fallback) {
  return request->hasParam(name, true) ? request->getParam(name, true)->value() : fallback;
}

}  // namespace

void registerSettingsRoute(AsyncWebServer& server) {
  server.on("/einstellungen", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = SETTINGS_FORM;
    html.replace("%WIFI_SSID%", CredentialsManager::instance().getWifiSsid());
    html.replace("%MQTT_BROKER%", CredentialsManager::instance().getMqttBroker());
    html.replace("%MQTT_PORT%", String(CredentialsManager::instance().getMqttPort()));
    html.replace("%MQTT_USER%", CredentialsManager::instance().getMqttUser());
    html.replace("%HA_URL%", CredentialsManager::instance().getHaBaseUrl());
    request->send(200, "text/html", html);
  });

  server.on("/einstellungen", HTTP_POST, [](AsyncWebServerRequest* request) {
    // Passwortfelder nur überschreiben, wenn tatsächlich ein neuer Wert eingegeben wurde - leer
    // gelassen heißt "unverändert", damit man nicht bei jeder Änderung alle Passwörter erneut
    // eintippen muss (und sie nicht im Klartext im Formular vorausgefüllt anzeigen muss).
    String ssid = paramOr(request, "wifi_ssid", CredentialsManager::instance().getWifiSsid());
    String wifiPassIn = paramOr(request, "wifi_pass", "");
    String wifiPass = wifiPassIn.length() > 0 ? wifiPassIn : CredentialsManager::instance().getWifiPass();

    String broker = paramOr(request, "mqtt_broker", CredentialsManager::instance().getMqttBroker());
    int port = paramOr(request, "mqtt_port", String(CredentialsManager::instance().getMqttPort())).toInt();
    String user = paramOr(request, "mqtt_user", CredentialsManager::instance().getMqttUser());
    String mqttPassIn = paramOr(request, "mqtt_pass", "");
    String mqttPass = mqttPassIn.length() > 0 ? mqttPassIn : CredentialsManager::instance().getMqttPass();

    String haUrl = paramOr(request, "ha_url", CredentialsManager::instance().getHaBaseUrl());

    CredentialsManager::instance().saveWifi(ssid, wifiPass);
    CredentialsManager::instance().saveMqtt(broker, port, user, mqttPass);
    CredentialsManager::instance().saveHaBaseUrl(haUrl);

    request->send(200, "text/html", SETTINGS_SAVED);

    // Kurz verzögert, damit die Antwort noch beim Client ankommt, bevor die Verbindung durch den
    // Neustart abreißt - WLAN-Änderungen greifen ohnehin erst nach einem Neuverbinden.
    delay(500);
    ESP.restart();
  });
}
