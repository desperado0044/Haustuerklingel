#include "credentials.h"
#include "../config.h"

CredentialsManager& CredentialsManager::instance() {
  static CredentialsManager manager;
  return manager;
}

bool CredentialsManager::begin() {
  return prefs.begin(NVS_NAMESPACE, false);
}

String CredentialsManager::getWifiSsid() {
  return prefs.getString("wifi_ssid", "");
}

String CredentialsManager::getWifiPass() {
  return prefs.getString("wifi_pass", "");
}

String CredentialsManager::getMqttBroker() {
  return prefs.getString("mqtt_broker", MQTT_DEFAULT_BROKER);
}

int CredentialsManager::getMqttPort() {
  return prefs.getInt("mqtt_port", MQTT_DEFAULT_PORT);
}

String CredentialsManager::getMqttUser() {
  return prefs.getString("mqtt_user", MQTT_DEFAULT_USER);
}

String CredentialsManager::getMqttPass() {
  return prefs.getString("mqtt_pass", MQTT_DEFAULT_PASS);
}

String CredentialsManager::getHaBaseUrl() {
  return prefs.getString("ha_base_url", HA_IMAGE_BASE_URL_DEFAULT);
}

void CredentialsManager::saveWifi(const String& ssid, const String& pass) {
  prefs.putString("wifi_ssid", ssid);
  prefs.putString("wifi_pass", pass);
}

void CredentialsManager::saveMqtt(const String& broker, int port, const String& user, const String& pass) {
  prefs.putString("mqtt_broker", broker);
  prefs.putInt("mqtt_port", port);
  prefs.putString("mqtt_user", user);
  prefs.putString("mqtt_pass", pass);
}

void CredentialsManager::saveHaBaseUrl(const String& baseUrl) {
  prefs.putString("ha_base_url", baseUrl);
}
