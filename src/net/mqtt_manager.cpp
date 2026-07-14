#include "mqtt_manager.h"
#include <WiFi.h>
#include "../storage/credentials.h"
#include "../storage/button_style.h"
#include "../display/image_cache.h"
#include "../display/button_presets.h"
#include "../ipc/messages.h"

namespace {

void sendToDisplay(ToDisplayMsgType type, uint8_t brightness = 0, const String& text = "") {
  ToDisplayMsg msg{};
  msg.type = type;
  msg.brightness = brightness;
  strncpy(msg.text, text.c_str(), sizeof(msg.text) - 1);
  msg.text[sizeof(msg.text) - 1] = '\0';
  // Nicht-blockierend: falls der Display-Task gerade im Rückstand ist, verwerfen statt MQTT auszubremsen.
  xQueueSend(g_toDisplayQueue, &msg, 0);
}

void sendButtonStyle(uint16_t color, TextWeight weight, int16_t x, int16_t y) {
  ToDisplayMsg msg{};
  msg.type = ToDisplayMsgType::SetButtonStyle;
  msg.buttonColor = color;
  msg.buttonWeight = static_cast<uint8_t>(weight);
  msg.buttonX = x;
  msg.buttonY = y;
  xQueueSend(g_toDisplayQueue, &msg, 0);
}

void sendBellDuration(uint16_t ms) {
  ToDisplayMsg msg{};
  msg.type = ToDisplayMsgType::SetBellDuration;
  msg.bellDurationMs = ms;
  xQueueSend(g_toDisplayQueue, &msg, 0);
}

void sendTouchMode(bool fullscreen) {
  ToDisplayMsg msg{};
  msg.type = ToDisplayMsgType::SetTouchMode;
  msg.touchFullscreen = fullscreen;
  xQueueSend(g_toDisplayQueue, &msg, 0);
}

void sendOverlayHidden(bool hidden) {
  ToDisplayMsg msg{};
  msg.type = ToDisplayMsgType::SetOverlayVisible;
  msg.overlayHidden = hidden;
  xQueueSend(g_toDisplayQueue, &msg, 0);
}

// Baut ein JSON-String-Array für HA-Discovery-"options" (select-Entity).
String jsonStringArray(const char* const* items, int count) {
  String result = "[";
  for (int i = 0; i < count; i++) {
    if (i > 0) {
      result += ",";
    }
    result += "\"" + String(items[i]) + "\"";
  }
  result += "]";
  return result;
}

}  // namespace

MqttManager& MqttManager::instance() {
  static MqttManager manager;
  return manager;
}

bool MqttManager::begin() {
  client.setClient(wifiClient);
  // PubSubClients Standardpuffer von 256 Bytes ist zu klein für die HA-Discovery-JSON-Payloads
  // (Device-Block + Attribute) - publish() kürzt/verwirft alles über dem Limit stillschweigend.
  client.setBufferSize(1024);
  client.setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->handleMessage(topic, payload, length);
  });
  return true;
}

void MqttManager::loop() {
  if (!client.connected()) {
    reconnect();
  } else {
    client.loop();
  }
  handleKlingelPressed();
  serviceKlingelReset();
}

bool MqttManager::isConnected() const {
  return const_cast<PubSubClient&>(client).connected();
}

// client.connect() blockiert kurz (TCP-Handshake + optionale Auth), daher werden Reconnect-
// Versuche auf höchstens einen pro MQTT_RECONNECT_COOLDOWN_MS gedrosselt statt bei jedem Loop-Tick.
void MqttManager::reconnect() {
  unsigned long now = millis();
  if (now - lastReconnectAttempt < MQTT_RECONNECT_COOLDOWN_MS) {
    return;
  }
  lastReconnectAttempt = now;

  String broker = CredentialsManager::instance().getMqttBroker();
  if (broker.length() == 0) {
    return;
  }
  int port = CredentialsManager::instance().getMqttPort();
  String user = CredentialsManager::instance().getMqttUser();
  String pass = CredentialsManager::instance().getMqttPass();
  String clientId = String(MQTT_CLIENT_ID_PREFIX) + WiFi.macAddress().substring(12);

  client.setServer(broker.c_str(), port);
  bool connected = client.connect(clientId.c_str(), user.c_str(), pass.c_str(),
                                   MQTT_TOPIC_STATUS, 0, true, "offline");
  if (!connected) {
    return;
  }

  client.publish(MQTT_TOPIC_STATUS, "online", true);
  // Explizites Neu-Abonnieren nach jedem (Re-)Connect - ein Broker-Neustart oder Netzwerk-Aussetzer
  // wirft Subscriptions raus, und PubSubClient stellt sie nicht automatisch wieder her.
  client.subscribe(MQTT_TOPIC_BILD);
  client.subscribe(MQTT_TOPIC_KALIBRIEREN);
  client.subscribe(MQTT_TOPIC_HELLIGKEIT);
  client.subscribe(MQTT_TOPIC_KLINGEL_FARBE);
  client.subscribe(MQTT_TOPIC_KLINGEL_STAERKE);
  client.subscribe(MQTT_TOPIC_KLINGEL_POSITION);
  client.subscribe(MQTT_TOPIC_KLINGELTON_DAUER);
  client.subscribe(MQTT_TOPIC_TOUCH_MODUS);
  client.subscribe(MQTT_TOPIC_KLINGEL_AUSBLENDEN);
  publishDiscovery();
}

void MqttManager::handleMessage(const char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; ++i) {
    msg += static_cast<char>(payload[i]);
  }
  String t(topic);

  if (t == MQTT_TOPIC_BILD) {
    String error;
    ImageCache::UpdateResult result = ImageCache::instance().updateImage(msg, error);
    if (result == ImageCache::UpdateResult::Updated) {
      sendToDisplay(ToDisplayMsgType::NewImageAvailable);
    } else if (result == ImageCache::UpdateResult::Failed) {
      sendToDisplay(ToDisplayMsgType::StatusText, 0, error);
    }
  } else if (t == MQTT_TOPIC_KALIBRIEREN) {
    if (msg == "start") {
      sendToDisplay(ToDisplayMsgType::StartCalibration);
    }
  } else if (t == MQTT_TOPIC_HELLIGKEIT) {
    int value = msg.toInt();
    value = constrain(value, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    sendToDisplay(ToDisplayMsgType::SetBrightness, static_cast<uint8_t>(value));
  } else if (t == MQTT_TOPIC_KLINGEL_FARBE) {
    // Jedes der drei Klingel-Button-Topics ändert nur sein eigenes Feld - die aktuell
    // gespeicherten anderen beiden Werte werden mit übernommen, statt sie zurückzusetzen.
    const ColorOption* color = findColorByName(msg);
    if (color != nullptr) {
      ButtonStyle style = ButtonStyleManager::instance().load();
      sendButtonStyle(color->rgb565, style.weight, style.x, style.y);
    }
  } else if (t == MQTT_TOPIC_KLINGEL_STAERKE) {
    int weightIndex = findWeightIndexByName(msg);
    if (weightIndex >= 0) {
      ButtonStyle style = ButtonStyleManager::instance().load();
      sendButtonStyle(style.color, static_cast<TextWeight>(weightIndex), style.x, style.y);
    }
  } else if (t == MQTT_TOPIC_KLINGEL_POSITION) {
    int index = msg.toInt();
    if (index >= 1 && index <= POSITION_COUNT) {
      GridPos pos = gridPositionByIndex(index);
      ButtonStyle style = ButtonStyleManager::instance().load();
      sendButtonStyle(style.color, style.weight, pos.x, pos.y);
    }
  } else if (t == MQTT_TOPIC_KLINGELTON_DAUER) {
    // Physischer Klingelton (MOSFET) - eigene, von den drei Topics oben unabhängige Einstellung.
    int ms = constrain(msg.toInt(), 0, BELL_MAX_DURATION_MS);
    sendBellDuration(static_cast<uint16_t>(ms));
  } else if (t == MQTT_TOPIC_TOUCH_MODUS) {
    // Standard-MQTT-switch-Konvention: ON/OFF.
    if (msg == "ON") {
      sendTouchMode(true);
    } else if (msg == "OFF") {
      sendTouchMode(false);
    }
  } else if (t == MQTT_TOPIC_KLINGEL_AUSBLENDEN) {
    // Standard-MQTT-switch-Konvention: ON/OFF.
    if (msg == "ON") {
      sendOverlayHidden(true);
    } else if (msg == "OFF") {
      sendOverlayHidden(false);
    }
  }
}

// Arbeitet Klingel-Events von Core 1 ab (siehe ipc/messages.h) und publiziert den initialen
// "pressed"-Zustand; serviceKlingelReset() unten publiziert ~300ms später "idle", damit
// aufeinanderfolgende Klingelereignisse von HA jeweils als eigener pressed->idle->pressed-
// Übergang erkannt werden.
void MqttManager::handleKlingelPressed() {
  ToNetworkMsg msg;
  while (xQueueReceive(g_toNetworkQueue, &msg, 0) == pdTRUE) {
    if (msg.type == ToNetworkMsgType::KlingelPressed && client.connected()) {
      client.publish(MQTT_TOPIC_KLINGEL, "pressed", false);
      klingelResetPending = true;
      klingelResetAtMs = millis() + MQTT_KLINGEL_RESET_DELAY_MS;
    }
  }
}

void MqttManager::serviceKlingelReset() {
  if (klingelResetPending && millis() >= klingelResetAtMs) {
    klingelResetPending = false;
    if (client.connected()) {
      client.publish(MQTT_TOPIC_KLINGEL, "idle", false);
    }
  }
}

// Retained HA-MQTT-Discovery-Configs, einmalig pro Boot beim ersten erfolgreichen Connect
// publiziert (Lastenheft: "beim Boot einmalig") - werden nicht bei jedem Reconnect erneut
// gesendet, da sie retained sind und Broker-Neustarts dadurch von selbst überstehen.
void MqttManager::publishDiscovery() {
  if (discoveryPublished || !client.connected()) {
    return;
  }
  discoveryPublished = true;

  const char* device = "{\"identifiers\":[\"" HA_DEVICE_ID "\"],\"name\":\"" HA_DEVICE_NAME "\",\"manufacturer\":\"DIY\",\"model\":\"ESP32 Touchdisplay\"}";

  String availabilityBlock = String("\"availability_topic\":\"") + MQTT_TOPIC_STATUS + "\",";

  client.publish(
      "homeassistant/binary_sensor/" HA_DEVICE_ID "/klingel/config",
      String("{\"name\":\"Klingel\",\"unique_id\":\"" HA_DEVICE_ID "_klingel\"," + availabilityBlock +
             "\"state_topic\":\"" + MQTT_TOPIC_KLINGEL + "\",\"payload_on\":\"pressed\",\"payload_off\":\"idle\"," +
             "\"device\":" + device + "}")
          .c_str(),
      true);

  // Manuelles Setzen von Dateiname/URL direkt aus HA heraus, als Ergänzung zum üblichen Weg über
  // eine Automation, die haustuer/bild publiziert.
  client.publish(
      "homeassistant/text/" HA_DEVICE_ID "/bild/config",
      String("{\"name\":\"Bild\",\"unique_id\":\"" HA_DEVICE_ID "_bild\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_BILD + "\",\"device\":" + device + "}")
          .c_str(),
      true);

  // Verbindungsstatus als eigene, sichtbare Sensor-Entity - bewusst OHNE availabilityBlock
  // (anders als alle anderen Entities unten): das ist genau die Entity, die den LWT-Status selbst
  // anzeigen soll, sie darf also nicht ihrerseits als "nicht verfügbar" markiert werden, wenn das
  // Gerät offline ist - sonst zeigt HA statt "Aus" nur noch "nicht verfügbar" an.
  client.publish(
      "homeassistant/binary_sensor/" HA_DEVICE_ID "/status/config",
      (String("{\"name\":\"Verbindung\",\"unique_id\":\"" HA_DEVICE_ID "_status\",\"state_topic\":\"") +
       MQTT_TOPIC_STATUS + "\",\"payload_on\":\"online\",\"payload_off\":\"offline\"," +
       "\"device_class\":\"connectivity\",\"device\":" + device + "}")
          .c_str(),
      true);

  client.publish(
      "homeassistant/button/" HA_DEVICE_ID "/kalibrieren/config",
      String("{\"name\":\"Touch kalibrieren\",\"unique_id\":\"" HA_DEVICE_ID "_kalibrieren\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_KALIBRIEREN + "\",\"payload_press\":\"start\"," +
             "\"device\":" + device + "}")
          .c_str(),
      true);

  client.publish(
      "homeassistant/number/" HA_DEVICE_ID "/helligkeit/config",
      String("{\"name\":\"Helligkeit\",\"unique_id\":\"" HA_DEVICE_ID "_helligkeit\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_HELLIGKEIT + "\",\"min\":" + String(BRIGHTNESS_MIN) +
             ",\"max\":" + String(BRIGHTNESS_MAX) + ",\"device\":" + device + "}")
          .c_str(),
      true);

  // Klingel-Button-Aussehen/-Position als HA-"select"-Entities (echte Dropdown-Auswahl mit
  // Klarnamen statt roher Zahlencodes) - siehe display/button_presets.h für die Optionslisten.
  // Kein state_topic (wie schon bei "kalibrieren"): reine Command-Entities ohne Rückmeldung des
  // aktuellen Werts in HA, konsistent mit dem bisherigen, bewusst einfach gehaltenen Muster.
  String colorOptions = "[";
  for (int i = 0; i < COLOR_OPTION_COUNT; i++) {
    if (i > 0) {
      colorOptions += ",";
    }
    colorOptions += "\"" + String(COLOR_OPTIONS[i].name) + "\"";
  }
  colorOptions += "]";

  String positionOptions = "[";
  for (int i = 1; i <= POSITION_COUNT; i++) {
    if (i > 1) {
      positionOptions += ",";
    }
    positionOptions += "\"" + String(i) + "\"";
  }
  positionOptions += "]";

  client.publish(
      "homeassistant/select/" HA_DEVICE_ID "/klingelfarbe/config",
      String("{\"name\":\"Klingel Farbe\",\"unique_id\":\"" HA_DEVICE_ID "_klingelfarbe\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_KLINGEL_FARBE + "\",\"options\":" + colorOptions +
             ",\"device\":" + device + "}")
          .c_str(),
      true);

  client.publish(
      "homeassistant/select/" HA_DEVICE_ID "/klingelstaerke/config",
      String("{\"name\":\"Klingel Schriftst\\u00e4rke\",\"unique_id\":\"" HA_DEVICE_ID "_klingelstaerke\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_KLINGEL_STAERKE + "\",\"options\":" + jsonStringArray(WEIGHT_NAMES, WEIGHT_COUNT) +
             ",\"device\":" + device + "}")
          .c_str(),
      true);

  client.publish(
      "homeassistant/select/" HA_DEVICE_ID "/klingelposition/config",
      String("{\"name\":\"Klingel Position\",\"unique_id\":\"" HA_DEVICE_ID "_klingelposition\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_KLINGEL_POSITION + "\",\"options\":" + positionOptions +
             ",\"device\":" + device + "}")
          .c_str(),
      true);

  // Physischer Klingelton (MOSFET) - eigene number-Entity, unabhängig von den drei select-
  // Entities oben.
  client.publish(
      "homeassistant/number/" HA_DEVICE_ID "/klingelton_dauer/config",
      String("{\"name\":\"Klingelton Dauer\",\"unique_id\":\"" HA_DEVICE_ID "_klingelton_dauer\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_KLINGELTON_DAUER + "\",\"min\":0,\"max\":" + String(BELL_MAX_DURATION_MS) +
             ",\"unit_of_measurement\":\"ms\",\"device\":" + device + "}")
          .c_str(),
      true);

  // Touch-Modus als switch-Entity (echtes An/Aus in HA, Standard-Konvention payload ON/OFF) -
  // kein state_topic, konsistent mit dem bewusst einfach gehaltenen Muster der anderen
  // Klingel-Einstellungen (reine Command-Entities ohne Rückmeldung des aktuellen Werts in HA).
  client.publish(
      "homeassistant/switch/" HA_DEVICE_ID "/touch_modus/config",
      String("{\"name\":\"Touch vollfl\\u00e4chig\",\"unique_id\":\"" HA_DEVICE_ID "_touch_modus\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_TOUCH_MODUS + "\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"," +
             "\"device\":" + device + "}")
          .c_str(),
      true);

  // Overlay-Sichtbarkeit als switch-Entity, gleiches Muster wie "Touch vollflächig" oben.
  client.publish(
      "homeassistant/switch/" HA_DEVICE_ID "/klingel_ausblenden/config",
      String("{\"name\":\"Klingel-Overlay ausblenden\",\"unique_id\":\"" HA_DEVICE_ID "_klingel_ausblenden\"," + availabilityBlock +
             "\"command_topic\":\"" + MQTT_TOPIC_KLINGEL_AUSBLENDEN + "\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\"," +
             "\"device\":" + device + "}")
          .c_str(),
      true);
}
