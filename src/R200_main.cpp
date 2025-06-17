#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "R200.h" // Sua biblioteca R200

// ======= CONFIGURAÇÕES DE REDE E MQTT (Mantenha as suas) ========
const char* SSID = "nersec";
const char* SENHA = "gremio123";
<<<<<<< HEAD
const char* BROKER_MQTT = "172.22.48.33";
=======
const char* BROKER_MQTT = "172.22.48.70";
>>>>>>> origin/main
const int PORTA_MQTT = 1883;

// ===================================================================================
// ======= IMPORTANTE: CLIENT_ID DEVE SER ÚNICO PARA CADA ESP32/MÓDULO! =======
const char* CLIENT_ID = "ESP32_RFID_Client_1"; // <-- MUDE PARA CADA MÓDULO! (ex: ESP32_RFID_Client_01)
// ===================================================================================

const char* TOPICO_MQTT = "/rfid/leituras";

// ======= CONFIGURAÇÕES DE TEMPO PARA O SEQUENCIAMENTO (Adaptado para rfid.poll()) ========
const unsigned long T_JANELA_ATIVA_MODULO = 150;       // Duração da "janela de oportunidade" de leitura deste módulo (ms)
const unsigned long T_INTERVALO_POLL_NA_JANELA = 50; // A cada quantos ms chamar rfid.poll() DENTRO da janela ativa
const unsigned long T_PAUSA_CURTA_POS_JANELA = 5;    // Pequena pausa após a janela ativa deste módulo (ms)

const unsigned long T_SLOT_COMPLETO_MODULO = T_JANELA_ATIVA_MODULO + T_PAUSA_CURTA_POS_JANELA; // Ex: 150 + 5 = 155 ms
const unsigned long T_ESPERA_OUTROS_MODULOS = T_SLOT_COMPLETO_MODULO * 2; // Espera pelos outros 2 módulos (Ex: 155 * 2 = 310 ms)

// ======= OBJETOS GLOBAIS ========
WiFiClient espClient;
PubSubClient mqttClient(espClient);
R200 rfid;

// ======= VARIÁVEIS DE CONTROLE DE TEMPO ========
unsigned long ultimoPollNaJanela = 0;

// ======= FUNÇÕES DE CONEXÃO (Mantenha as suas) ========
void conectarWiFi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(SSID, SENHA);
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

// ======= FUNÇÃO DE PUBLICAÇÃO MQTT (Mantenha a sua) ========
void publicarMQTT(const String& epc) {
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  JsonDocument doc;
  doc["epc"] = epc;
  doc["timestamp"] = String(millis());
  doc["mqttId"] = CLIENT_ID;

  String jsonString;
  serializeJson(doc, jsonString);

  mqttClient.publish(TOPICO_MQTT, jsonString.c_str());
  Serial.println("Publicado via MQTT: " + jsonString);
}

// ======= SETUP ========
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando: " __FILE__ " " __DATE__);

  rfid.begin(&Serial2, 115200, 16, 17);
  rfid.dumpModuleInfo();
<<<<<<< HEAD
  rfid.setTransmissionPower(100);
=======
  rfid.setTransmissionPower(0x1A);
>>>>>>> origin/main
  rfid.acquireTransmitPower();

  conectarWiFi();
  conectarMQTT();

  // ===================================================================================
  // ======= DELAY INICIAL PARA ESCALONAMENTO (MUDAR PARA CADA MÓDULO!) ========
  // ===================================================================================
  // MÓDULO 1: delay(0); // (Ou simplesmente não coloque esta linha)
  // MÓDULO 2: delay(T_SLOT_COMPLETO_MODULO * 1); // Ex: delay(155);
  // MÓDULO 3: delay(T_SLOT_COMPLETO_MODULO * 2); // Ex: delay(310);
  // ===================================================================================
  // Exemplo para o MÓDULO 1 (sem delay inicial):
  // delay(0); 

  // Exemplo para o MÓDULO 2 (descomente e use este no Módulo 2):
  // delay(T_SLOT_COMPLETO_MODULO * 1); 

  // Exemplo para o MÓDULO 3 (descomente e use este no Módulo 3):
  // delay(T_SLOT_COMPLETO_MODULO * 2); 

  Serial.println("Sistema iniciado e escalonado (usando rfid.poll() repetido).");
}

// ======= LOOP ========
void loop() {
  mqttClient.loop(); // Manter MQTT funcionando independentemente da fase

  // --- FASE ATIVA DE LEITURA PARA ESTE MÓDULO ---
  // Serial.println("LOOP: Iniciando Janela Ativa de Leitura..."); // Debug
  unsigned long inicioJanelaAtiva = millis();
  // Força o primeiro poll a acontecer imediatamente dentro da janela
  ultimoPollNaJanela = inicioJanelaAtiva - T_INTERVALO_POLL_NA_JANELA; 

  while (millis() - inicioJanelaAtiva < T_JANELA_ATIVA_MODULO) {
    mqttClient.loop(); // Manter MQTT funcionando
    
    // A função rfid.loop() da biblioteca R200 é chamada para processar respostas
    // da serial. É importante chamá-la regularmente.
    rfid.loop(); 

    // Chamar rfid.poll() em intervalos dentro da janela ativa
    if (millis() - ultimoPollNaJanela >= T_INTERVALO_POLL_NA_JANELA) {
      // Serial.println("  Janela Ativa: Chamando rfid.poll()"); // Debug
      rfid.poll(); // Envia CMD_SinglePollInstruction
      ultimoPollNaJanela = millis();
    }

    // Após chamar rfid.poll(), chame rfid.loop() novamente para processar
    // a resposta do single poll e potencialmente preencher rfid.epc
    rfid.loop(); 
    if (rfid.epc.length() > 0) {
      // Serial.print("  Janela Ativa: TAG LIDA: "); Serial.println(rfid.epc); // Debug
      publicarMQTT(rfid.epc);
      rfid.epc = ""; // Limpa após publicar para não reenviar a mesma tag da mesma leitura
    }
    
    delay(1); // Pequeno delay para ceder tempo ao processador, essencial se T_INTERVALO_POLL_NA_JANELA for curto
  }
  // Serial.println("LOOP: Fim da Janela Ativa de Leitura."); // Debug
  // --- FIM DA FASE ATIVA ---

  // Pequena pausa após a janela ativa deste módulo (para garantir transição limpa)
  delay(T_PAUSA_CURTA_POS_JANELA);

  // --- FASE INATIVA (ESPERA PELOS OUTROS MÓDULOS) ---
  // Serial.println("LOOP: Iniciando Fase de Espera..."); // Debug
  delay(T_ESPERA_OUTROS_MODULOS);
  // Serial.println("LOOP: Fim da Fase de Espera."); // Debug
}