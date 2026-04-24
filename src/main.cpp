//------------------------------------------------------------------------------------
// Gestion Piscine 2026
//
// Fonctionnement:
// - lecture des 3 sondes DS18B20 toutes les 10 secondes
// - publication MQTT a chaque lecture sur domos/piscine/valeurs
// - ecoute du topic domos/piscine/pompe pour piloter le relais electrolyse
//
// -- Mise à jour OTA via ElegantOTA (http://<IP_ADDRESS>/update)
// http://192.168.1.113/update
//------------------------------------------------------------------------------------

#include "config.h"
#include "display_manager.h"
#include <ArduinoJson.h>
#include <DallasTemperature.h>
#include <ElegantOTA.h>
#include <M5Unified.h>
#include <MODULE_2RELAY.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>

WiFiClient espClient;
PubSubClient client(espClient);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

MODULE_2RELAY relayModule;
WebServer server(80);
DisplayManager display;

float tempEau = -127.0f;
float tempAir = -127.0f;
float tempLocal = -127.0f;

float pompeTempEau = -127.0f;
int pompeRpm = 0;
bool pompeRunning = false;
bool pompeTempValid = false;
bool pompeRpmValid = false;
bool electrolyseDemandee = false;
bool stateElectrolyse = false;

unsigned long lastTempRead = 0;

void setupWiFi();
void handleWiFiConnection();
void reconnectMQTT();
void callbackMQTT(char *topic, byte *payload, unsigned int length);
void updateTemperatures();
void publishTemperatures();
void handlePumpMessage(const JsonDocument &doc);
void applyElectrolyseRule();
void updateDisplay();

static String formatTemperature(float value) { return String(value, 1); }

static bool readJsonBool(const JsonVariantConst &value, bool defaultValue = false) {
  if (value.is<bool>()) {
    return value.as<bool>();
  }
  if (value.is<int>()) {
    return value.as<int>() != 0;
  }
  if (value.is<const char *>()) {
    String text = value.as<const char *>();
    text.trim();
    text.toLowerCase();
    return text == "true" || text == "1" || text == "on";
  }
  return defaultValue;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  display.begin();

  Serial.begin(115200);
  Serial.println("Demarrage Gestion Piscine M5Stick Fire...");

  if (!relayModule.begin(&Wire, 21, 22)) {
    Serial.println("ERREUR: Module 2-Relay non trouve !");
  }
  relayModule.setAllRelay(false);

  sensors.begin();
  int deviceCount = sensors.getDeviceCount();
  Serial.printf("\n--- DIAGNOSTIC ONE-WIRE ---\n");
  Serial.printf("Nombre de sondes trouvees: %d\n", deviceCount);

  for (int i = 0; i < deviceCount; i++) {
    DeviceAddress tempAddr;
    if (sensors.getAddress(tempAddr, i)) {
      Serial.printf("Sonde #%d - Adresse: {", i);
      for (uint8_t j = 0; j < 8; j++) {
        Serial.printf("0x%02X%s", tempAddr[j], (j < 7) ? ", " : "");
      }
      Serial.println("};");
    }
  }
  Serial.println("---------------------------\n");

  setupWiFi();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setBufferSize(1024);
  client.setCallback(callbackMQTT);

  Serial.printf("MQTT buffer size: %u\n", client.getBufferSize());

  ElegantOTA.begin(&server);
  server.on("/", []() {
    server.send(200, "text/plain", "Gestion Piscine 2026 - M5Stick Fire");
  });
  server.begin();

  updateTemperatures();
  publishTemperatures();
  updateDisplay();
  display.updateActivity();
}

void loop() {
  M5.update();
  handleWiFiConnection();

  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    ElegantOTA.loop();
  }

  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    reconnectMQTT();
  }
  if (client.connected()) {
    client.loop();
  }

  static bool lastWifiStatus = true;
  bool currentWifiStatus = (WiFi.status() == WL_CONNECTED);
  if (currentWifiStatus != lastWifiStatus) {
    if (!currentWifiStatus) {
      Serial.println("ATTENTION: Connexion WiFi perdue ! tentative de reconnexion...");
    } else {
      Serial.println("INFO: WiFi de nouveau connecte !");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
    lastWifiStatus = currentWifiStatus;
  }

  if (millis() - lastTempRead >= TEMP_READ_INTERVAL) {
    updateTemperatures();
    publishTemperatures();
    updateDisplay();
    lastTempRead = millis();
  }

  display.manageBrightness(false);

  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
    display.updateActivity();
    updateDisplay();
  }
}

void setupWiFi() {
  Serial.print("Connexion WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connecte ! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nEchec WiFi. Redemarrage...");
    delay(2000);
    ESP.restart();
  }
}

void handleWiFiConnection() {
  static unsigned long lastReconnectAttempt = 0;

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (client.connected()) {
    client.disconnect();
  }

  if (millis() - lastReconnectAttempt < 10000) {
    return;
  }

  lastReconnectAttempt = millis();
  Serial.println("WiFi deconnecte, relance de la connexion...");
  WiFi.disconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void reconnectMQTT() {
  static unsigned long lastReconnectAttempt = 0;
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (millis() - lastReconnectAttempt <= 5000) {
    return;
  }

  lastReconnectAttempt = millis();
  String clientId = String(MQTT_CLIENT_ID) + "_" + String(random(0, 999));

  Serial.print("Connexion MQTT (id=");
  Serial.print(clientId);
  Serial.print(")... ");

  if (client.connect(clientId.c_str())) {
    Serial.println("Connecte!");
    client.subscribe(MQTT_TOPIC_POMPE);
    Serial.print("Subscribe: ");
    Serial.println(MQTT_TOPIC_POMPE);
    publishTemperatures();
  } else {
    Serial.print("Echec, rc=");
    Serial.print(client.state());
    Serial.println(" retry in 5s");
  }
}

void callbackMQTT(char *topic, byte *payload, unsigned int length) {
  String top = String(topic);
  if (top != MQTT_TOPIC_POMPE) {
    return;
  }

  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += static_cast<char>(payload[i]);
  }

  Serial.print("MQTT IN [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.print("JSON Error: ");
    Serial.println(error.c_str());
    return;
  }

  handlePumpMessage(doc);
  updateDisplay();
}

void updateTemperatures() {
  sensors.requestTemperatures();
  tempAir = sensors.getTempC(TempAirAddr);
  tempEau = sensors.getTempC(TempEauAddr);
  tempLocal = sensors.getTempC(TempLocalAddr);

  Serial.printf("LECTURE SONDES: Eau=%.1f Air=%.1f Local=%.1f\n", tempEau,
                tempAir, tempLocal);
}

void publishTemperatures() {
  if (!client.connected()) {
    Serial.println("MQTT indisponible, publication ignoree.");
    return;
  }

  JsonDocument doc;
  doc["temp_eau"] = formatTemperature(tempEau);
  doc["temp_air"] = formatTemperature(tempAir);
  doc["temp_local"] = formatTemperature(tempLocal);
  doc["electrolyse"] = stateElectrolyse;
  doc["ip"] = WiFi.localIP().toString();

  char buffer[224];
  serializeJson(doc, buffer, sizeof(buffer));
  client.publish(MQTT_TOPIC_VALEURS, buffer);

  Serial.print("MQTT PUBLISH [");
  Serial.print(MQTT_TOPIC_VALEURS);
  Serial.print("]: ");
  Serial.println(buffer);
}

void handlePumpMessage(const JsonDocument &doc) {
  pompeRpm = doc["rpm"] | 0;
  pompeRpmValid = doc["rpm_valid"] | false;
  pompeRunning = readJsonBool(doc["motor_running"]);
  pompeTempEau = doc["temp_eau"] | -127.0f;
  pompeTempValid = doc["temp_valid"] | false;
  electrolyseDemandee = doc["electrolyse"] | false;

  Serial.printf("POMPE: rpm=%d running=%d temp=%.2f temp_valid=%d electro=%d\n",
                pompeRpm, pompeRunning, pompeTempEau, pompeTempValid,
                electrolyseDemandee);

  applyElectrolyseRule();
}

void applyElectrolyseRule() {
  bool shouldEnable = pompeRunning && pompeRpmValid && pompeTempValid &&
                      electrolyseDemandee && (pompeRpm > ELECTROLYSE_MIN_RPM) &&
                      (pompeTempEau > 16.0f);

  stateElectrolyse = shouldEnable;
  relayModule.setRelay(ELECTROLYSE_RELAY_INDEX, stateElectrolyse);

  Serial.printf("RELAY ELECTROLYSE: %s\n", stateElectrolyse ? "ON" : "OFF");
}

void updateDisplay() {
  float displayedTempEau = pompeTempValid ? pompeTempEau : tempEau;
  display.displayInfo(displayedTempEau, pompeRunning, stateElectrolyse,
                      WiFi.localIP());
}
