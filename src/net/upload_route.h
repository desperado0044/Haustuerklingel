#pragma once
#include <ESPAsyncWebServer.h>

// Registriert einen manuellen Bild-Upload auf dem bestehenden Webserver:
//   GET  /upload  - zeigt ein simples HTML-Formular
//   POST /upload  - nimmt eine JPEG-Datei entgegen, validiert Größe/Format und übernimmt sie
//                   als neues Anzeigebild (wie ein erfolgreicher haustuer/bild-Download, nur
//                   ohne MQTT/Home Assistant).
void registerUploadRoute(AsyncWebServer& server);
