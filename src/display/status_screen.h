#pragma once
#include <Arduino.h>

// Rendert eine Status-Zeile auf schlichtem schwarzem Hintergrund - genutzt während Boot/AP-
// Provisionierung, bevor je ein Bild gecacht wurde, statt den Bildschirm leer zu lassen
// (Vorgabe aus dem Lastenheft).
namespace StatusScreen {
void draw(const String& text);
}
