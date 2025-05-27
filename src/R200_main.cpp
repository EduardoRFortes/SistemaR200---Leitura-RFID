#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "R200.h" // Inclui sua biblioteca R200

// ======= CONFIGURAÇÕES DE REDE E MQTT ========
const char* SSID = "nersec";
const char* SENHA = "gremio123";

const char* BROKER_MQTT = "172.22.48.33";  // IP do servidor MQTT
const int PORTA_MQTT = 1883;

// ===================================================================================
// ======= IMPORTANTE: CLIENT_ID DEVE SER ÚNICO PARA CADA ESP32/MÓDULO! =======
// Exemplo:
// Para o Módulo 1: const char* CLIENT_ID = "ESP32_RFID_Client_01";
// Para o M2:       const char* CLIENT_ID = "ESP32_RFID_Client_02";
// Para o M3:       const char* CLIENT_ID = "ESP32_RFID_Client_03";
const char* CLIENT_ID = "ESP32_RFID_Client_1"; // <-- MUDAR PARA CADA MÓDULO!
// ===================================================================================

const char* TOPICO_MQTT = "/rfid/leituras";

// ======= CONFIGURAÇÕES DE TEMPO PARA O SEQUENCIAMENTO ========
// Estes valores devem ser os MESMOS em TODOS os 3 módulos
const unsigned long T_DWELL_RF_ON = 150;      // Tempo que o RF fica LIGADO lendo tags (em ms)
const unsigned long T_PAUSA_CURTA_ENTRE_ANTENAS = 5; // Pequena pausa para transição (em ms)
const unsigned long T_SLOT_COMPLETO_MODULO = T_DWELL_RF_ON + T_PAUSA_CURTA_ENTRE_ANTENAS; // 155 ms
const unsigned long T_ESPERA_OUTROS_MODULOS = T_SLOT_COMPLETO_MODULO * 2; // Espera pelos outros 2 módulos (310 ms)

// ======= OBJETOS GLOBAIS ========
WiFiClient espClient;
PubSubClient mqttClient(espClient);
R200 rfid; // Instância da sua classe R200

// ======= VARIÁVEIS DE CONTROLE DE TEMPO (NÃO ALTERAR) ========
// unsigned long lastPollTime = 0; // Não será mais usado como antes

// ======= FUNÇÕES DE CONEXÃO ========
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
      delay(1000); // Pequeno delay antes de tentar reconectar
    }
  }
}

// ======= FUNÇÃO DE PUBLICAÇÃO MQTT ========
void publicarMQTT(const String& epc) {
  if (!mqttClient.connected()) {
    conectarMQTT(); // Tenta reconectar se a conexão caiu
  }

  JsonDocument doc; // Cria um objeto JSON
  doc["epc"] = epc;
  doc["timestamp"] = String(millis()); // Tempo desde o início do ESP32
  doc["mqttId"] = CLIENT_ID; // Identificador único deste módulo

  String jsonString;
  serializeJson(doc, jsonString); // Serializa o JSON para String

  mqttClient.publish(TOPICO_MQTT, jsonString.c_str()); // Publica no tópico MQTT
  Serial.println("Publicado via MQTT: " + jsonString);
}

// ======= SETUP: Executado uma única vez ao iniciar ========
void setup() {
  Serial.begin(115200); // Inicializa comunicação serial para debug
  Serial.println(__FILE__ " " __DATE__); // Imprime nome do arquivo e data de compilação

  // Inicializa o módulo RFID R200
  // Certifique-se de que os pinos 16 e 17 são os corretos para Serial2 no seu ESP32
  rfid.begin(&Serial2, 115200, 16, 17);
  rfid.dumpModuleInfo(); // Exibe informações do módulo RFID
  rfid.setTransmissionPower(0x1A); // Potência de transmissão (0x1A = 37dbm)
  rfid.acquireTransmitPower(); // Adquire a potência atual (para verificar)

  // Conecta ao WiFi e MQTT
  conectarWiFi();
  conectarMQTT();

  // ===================================================================================
  // ======= DELAY INICIAL PARA ESCALONAMENTO (MUDAR PARA CADA MÓDULO!) ========
  // ===================================================================================
  // Este delay garante que os módulos iniciem suas fases ativas em momentos diferentes.
  // Você DEVE definir um valor diferente para cada um dos 3 módulos:
  //
  // MÓDULO 1 (Antena 1):
  //   delay(0); // Ou simplesmente não coloque esta linha
  //
  // MÓDULO 2 (Antena 2):
  //   delay(T_SLOT_COMPLETO_MODULO * 1); // delay(155);
  //
  // MÓDULO 3 (Antena 3):
  //   delay(T_SLOT_COMPLETO_MODULO * 2); // delay(310);
  // ===================================================================================

  // Exemplo para o MÓDULO 1 (sem delay inicial):
  // delay(0); // Pode ser omitido

  // Exemplo para o MÓDULO 2 (descomente e use este no Módulo 2):
  // delay(T_SLOT_COMPLETO_MODULO * 1);

  // Exemplo para o MÓDULO 3 (descomente e use este no Módulo 3):
  // delay(T_SLOT_COMPLETO_MODULO * 2);

  Serial.println("Sistema iniciado e escalonado.");
}

// ======= LOOP: Executado repetidamente ========
void loop() {
  mqttClient.loop(); // Mantém a conexão MQTT ativa e processa mensagens pendentes
  // rfid.loop(); // Não chame aqui fora do bloco de leitura ativa, pois pode interferir no timing

  // --- FASE ATIVA DE LEITURA PARA ESTE MÓDULO ---
  Serial.println("Iniciando fase ativa de leitura...");
  // Liga o modo de polling múltiplo (leitura contínua)
  rfid.setMultiplePollingMode(true);

  unsigned long inicioFaseAtiva = millis();
  while (millis() - inicioFaseAtiva < T_DWELL_RF_ON) {
    // Durante a fase ativa, chame rfid.loop() para processar as respostas do módulo
    // e verificar se tags foram lidas.
    rfid.loop();
    if (rfid.epc.length() > 0) {
      publicarMQTT(rfid.epc);
      rfid.epc = ""; // Limpa o EPC para garantir que só publicamos novas leituras
    }
    mqttClient.loop(); // É importante chamar mqttClient.loop() aqui também para manter a conexão ativa
    delay(1); // Pequeno delay para permitir que o sistema operacional do ESP32 faça outras tarefas
  }

  // --- FIM DA FASE ATIVA ---
  // Desliga o modo de polling múltiplo
  rfid.setMultiplePollingMode(false);
  Serial.println("Fim da fase ativa de leitura.");

  // Garante que qualquer última tag lida seja processada após desligar o RF
  rfid.loop();
  if (rfid.epc.length() > 0) {
    publicarMQTT(rfid.epc);
    rfid.epc = "";
  }

  // --- FASE INATIVA (ESPERA PELOS OUTROS MÓDULOS) ---
  // Este delay faz com que este módulo espere enquanto os outros dois realizam suas leituras.
  Serial.println("Iniciando fase de espera...");
  delay(T_ESPERA_OUTROS_MODULOS);
  Serial.println("Fim da fase de espera.");
}