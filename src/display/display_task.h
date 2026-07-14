#pragma once
#include <Arduino.h>

// Kein echter FreeRTOS-Task - läuft als Arduinos Standard-Core-1-loop() via main.cpp, parallel
// zum separat gestarteten Core-0-Netzwerk-Task (siehe net/network_task.h). Verantwortlich für
// TFT-/Touch-Init, Touch-Polling mit Flankenerkennung, Bild-Rendering, Button-Overlay und Kalibrierung.
void displayTaskSetup();
void displayTaskLoop();
