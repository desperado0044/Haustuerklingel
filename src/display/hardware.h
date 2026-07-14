#pragma once
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Gemeinsame Hardware-Instanzen, einmalig in display_task.cpp definiert und von button.cpp,
// touch_calibration.cpp und status_screen.cpp genutzt. Gehören ausschließlich Core 1 (dem
// Display-Task) - Core 0 fasst SPI/TFT/Touch nie direkt an, sondern tauscht nur Nachrichten
// über ipc/messages.h aus.
//
// tft nutzt TFT_eSPIs eigene, HSPI-basierte SPI-Instanz (USE_HSPI_PORT-Build-Flag). ts nutzt das
// globale Arduino-`SPI`-Objekt an dessen nativen VSPI-Pins - wie gefordert echte, vom Display-Bus
// getrennte Hardware (vollständige Erklärung siehe platformio.ini).
extern TFT_eSPI tft;
extern XPT2046_Touchscreen ts;
