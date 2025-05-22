#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "R200.h"

// ======= CONFIGURAÇÕES ========
const char* SSID = "nersec";
const char* SENHA = "gremio123";

const char* BROKER_MQTT = "172.22.48.70";  // IP do servidor
const int PORTA_MQTT = 1883;
const char* CLIENT_ID = "ESP32_RFID_Client";
const char* TOPICO_MQTT = "/rfid/leituras";

// ======= OBJETOS GLOBAIS ========
WiFiClient espClient;
PubSubClient mqttClient(espClient);
R200 rfid;

unsigned long lastResetTime = 0;

void conectarWiFi() {
  // WiFi.config(
  //   IPAddress(172,22,48,73),
  //   IPAddress(172,22,48,1),
  //   IPAddress(255,255,255,0),

  //   IPAddress(172,22,48,1)
  // ); // Só se usar domiio no lugar de ip
  WiFi.begin(SSID, SENHA);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado! IP: " + WiFi.localIP().toString());
}

void conectarMQTT() {
  mqttClient.setServer(BROKER_MQTT, PORTA_MQTT);
  Serial.print("Conectando ao broker MQTT");
  while (!mqttClient.connected()) {
    Serial.print(".");
    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("\nConectado ao MQTT!");
    } else {
      Serial.print(" Falha, rc=");
      Serial.print(mqttClient.state());
      delay(1000);
    }
  }
}

void publicarMQTT(const String& epc) {
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  JsonDocument doc;
  doc["epc"] = epc;
  doc["timestamp"] = String(millis());
  doc["mqttId"] = CLIENT_ID;

  String json;
  serializeJson(doc, json);

  mqttClient.publish(TOPICO_MQTT, json.c_str());
  Serial.println("Publicado via MQTT: " + json);
}

void setup() {
  Serial.begin(115200);
  Serial.println(__FILE__ __DATE__);

  rfid.begin(&Serial2, 115200, 16, 17);
  rfid.dumpModuleInfo();
  rfid.setTransmissionPower(0x1A);   //37dbm potência máxima permitida
  rfid.acquireTransmitPower(); 

  conectarWiFi();
  conectarMQTT();

  Serial.println("Sistema iniciado.");
}

void loop() {
  mqttClient.loop();   // Mantém conexão ativa
  rfid.loop();         // Atualiza RFID

  if (rfid.epc.length() > 0) {
    publicarMQTT(rfid.epc);
    rfid.epc = "";
  }

  if (millis() - lastResetTime > 1000) {
    rfid.poll();
    lastResetTime = millis();
  }

  delay(1000);
}
