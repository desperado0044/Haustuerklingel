#pragma once
#include <Arduino.h>

// FreeRTOS-Task-Einstiegspunkt, von main.cpp's xTaskCreatePinnedToCore() auf Core 0 gepinnt.
// Verantwortlich für WiFiManager-Provisionierung (inkl. Reconnect-Watchdog, der nach erfolgter
// Verbindung nie wieder ins blockierende AP-Portal zurückfällt), MQTT, HTTP-Bilddownloads (über
// MqttManager -> ImageCache) und den OTA-Webserver - also alles aus der "Core 0"-Spalte des
// Lastenhefts.
void networkTaskEntry(void* pvParameters);
