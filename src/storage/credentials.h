#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Persistiert WLAN- + MQTT-Broker-Zugangsdaten in NVS, damit sie Neustarts/OTA-Updates überstehen.
// WLAN wird vom erfolgreichen autoConnect()-Pfad des WiFiManager geschrieben; MQTT-Broker/Port/
// User/Passwort werden als WiFiManager-Custom-Parameter auf derselben Captive-Portal-Seite erfasst
// (siehe net/network_task.cpp) - es gibt keine separate, dedizierte MQTT-Konfig-UI.
class CredentialsManager {
public:
  static CredentialsManager& instance();
  bool begin();

  String getWifiSsid();
  String getWifiPass();
  String getMqttBroker();
  int getMqttPort();
  String getMqttUser();
  String getMqttPass();

  // Basis-URL, von der Home Assistants Bilder geladen werden, z.B. "http://192.168.1.10:8123" -
  // das Suffix "/local/klingel/<dateiname>" (HA_IMAGE_PATH_PREFIX) wird von image_cache.cpp
  // angehängt. Nicht im Voraus erratbar (hängt von der HA-Instanz des Nutzers ab), daher auf
  // derselben WiFiManager-Captive-Portal-Seite wie der MQTT-Broker erfasst statt hartkodiert.
  String getHaBaseUrl();

  void saveWifi(const String& ssid, const String& pass);
  void saveMqtt(const String& broker, int port, const String& user, const String& pass);
  void saveHaBaseUrl(const String& baseUrl);

private:
  CredentialsManager() = default;
  Preferences prefs;
};
