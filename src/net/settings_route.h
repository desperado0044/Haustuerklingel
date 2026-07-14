#pragma once
#include <ESPAsyncWebServer.h>

// Registriert eine Einstellungsseite (GET/POST /einstellungen), über die WLAN-, MQTT- und
// HA-Basis-URL-Zugangsdaten im laufenden STA-Betrieb geändert werden können - das Pendant zu den
// WiFiManager-Custom-Parametern in net/network_task.cpp, die dieselben Felder nur im AP-/
// Captive-Portal-Modus abdecken. Bewusst zwei getrennte Oberflächen für die zwei Zustände, statt
// eines gemeinsamen Servers über beide Modi hinweg.
void registerSettingsRoute(AsyncWebServer& server);
