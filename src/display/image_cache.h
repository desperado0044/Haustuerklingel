#pragma once
#include <Arduino.h>

// Verwaltet den Ein-Bild-LittleFS-Cache (CURRENT_IMAGE_PATH + CURRENT_IMAGE_FLAG_PATH). Wird vom
// Core-0-Netzwerk-Task aufgerufen (dort findet der HTTP-Download statt); die resultierende Datei
// wird später vom Core-1-Display-Task per TJpg_Decoder gerendert, angestoßen über g_toDisplayQueue.
class ImageCache {
public:
  enum class UpdateResult {
    NoChange,  // angeforderter Dateiname stimmt bereits mit dem gecachten überein - nichts getan.
    Updated,   // Download erfolgreich, /current.jpg und /current.txt wurden beide überschrieben.
    Failed,    // Download fehlgeschlagen (404, unvollständig, etc.) - altes /current.jpg unangetastet.
  };

  static ImageCache& instance();

  // Mountet LittleFS. Muss vor jeder anderen Methode aufgerufen werden, und vor der Display-Init,
  // damit die Boot-Sequenz ein gecachtes Bild zeigen kann, bevor WLAN überhaupt zu verbinden beginnt.
  bool begin();

  bool hasCachedImage();

  // Vergleicht filename mit der gecachten Flag-Datei; bei Unterschied wird per HTTP von
  // CredentialsManagers HA-Basis-URL + HA_IMAGE_PATH_PREFIX + filename heruntergeladen, der
  // Download wird gegen Content-Length geprüft bevor er übernommen wird. errorOut wird bei
  // Failed gesetzt.
  UpdateResult updateImage(const String& filename, String& errorOut);
};
