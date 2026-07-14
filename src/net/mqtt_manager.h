#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "../config.h"

// Einfacher (nicht-TLS) MQTT-Client, ausschließlich vom Core-0-Netzwerk-Task verwaltet. Kümmert
// sich um LWT-basierte Verfügbarkeit, einmalige Home-Assistant-Discovery, das Abonnieren der drei
// HA->ESP32-Kommando-Topics und das Publizieren von Klingel-Events (kommen von Core 1 über
// g_toNetworkQueue - siehe ipc/messages.h - da PubSubClient selbst nicht threadsicher von beiden
// Cores aus aufgerufen werden darf).
class MqttManager {
public:
  static MqttManager& instance();
  bool begin();

  // Nicht-blockierend: reconnected (mit Cooldown) bei Verbindungsverlust, bedient sonst den
  // Client, und arbeitet anstehende Klingel-Events aus g_toNetworkQueue ab.
  void loop();
  bool isConnected() const;

private:
  MqttManager() = default;
  WiFiClient wifiClient;
  PubSubClient client;
  bool discoveryPublished = false;
  unsigned long lastReconnectAttempt = 0;

  bool klingelResetPending = false;
  unsigned long klingelResetAtMs = 0;

  void reconnect();
  void publishDiscovery();
  void handleKlingelPressed();
  void serviceKlingelReset();

  // PubSubClient-Callback für haustuer/bild, haustuer/kalibrieren, haustuer/helligkeit.
  void handleMessage(const char* topic, byte* payload, unsigned int length);
};
