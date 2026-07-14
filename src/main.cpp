#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "ipc/messages.h"
#include "display/image_cache.h"
#include "display/display_task.h"
#include "net/network_task.h"

QueueHandle_t g_toDisplayQueue = nullptr;
QueueHandle_t g_toNetworkQueue = nullptr;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Haustuerklingel-Firmware gestartet");

  g_toDisplayQueue = xQueueCreate(8, sizeof(ToDisplayMsg));
  g_toNetworkQueue = xQueueCreate(8, sizeof(ToNetworkMsg));

  esp_task_wdt_init(TASK_WATCHDOG_TIMEOUT_SEC, true);

  // LittleFS-Mount + erstes Rendern müssen erfolgen, bevor der Core-0-Netzwerk-Task startet
  // (dessen WiFiManager-Captive-Portal kann lange blockieren) - Vorgabe aus dem Lastenheft: das
  // gecachte Foto (oder eine Status-Zeile, falls noch keins existiert) muss unabhängig vom
  // WLAN-Status sofort beim Boot erscheinen.
  ImageCache::instance().begin();
  displayTaskSetup();

  xTaskCreatePinnedToCore(networkTaskEntry, "network_task", 8192, nullptr, 1, nullptr, 0);

  esp_task_wdt_add(NULL);  // registriert Arduinos loopTask (Core 1, dieser Task) beim Watchdog.
}

void loop() {
  esp_task_wdt_reset();
  displayTaskLoop();
  delay(10);
}
