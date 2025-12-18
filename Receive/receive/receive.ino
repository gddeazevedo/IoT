/* * Heltec/ESP32 LoRa Receiver (Gateway) -> MQTT & Google Sheets
 * Função:
 * 1. Recebe JSON via LoRa.
 * 2. Obtém Timestamp via NTP.
 * 3. Envia para Broker MQTT (Mosquitto) via WiFi.
 * 4. Envia para Google Sheets via WiFi (HTTPS).
 */

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include "boards.h" // Arquivo com definição dos pinos (RADIO_CS_PIN, etc.)
#include <PubSubClient.h> // Biblioteca necessária para MQTT
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// --- Configurações do LoRa ---
#define LORA_FREQUENCY      915E6  // 915 MHz
#define LORA_SPREADING_FACTOR 7    // Deve ser igual ao transmissor
#define LORA_BANDWIDTH      125E3  
#define TX_POWER            20

// --- Configurações WiFi ---
const char* ssid = "AndroidAP";
const char* password = "PedroVictor";

// --- Configurações MQTT ---
const char* mqtt_server = "10.66.121.105"; // IP do Servidor Mosquitto
const int mqtt_port = 1883;
const char* mqtt_topic = "cefet/iot";

// --- Configurações Google Sheets ---
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbyIok0fVuC6_7HITkZuXR9nF-sln68yNEuWZbuBvsz32kBbSwThIQrS7vezjF-v1_o/exec";

// --- NTP network time protocol related ---
const int UTC_OFFSET = -3*3600; // Brasilia UTC time offset
const long NTP_UPDATE_INTERVAL = 3600*1000;
WiFiUDP ntp_UDP;
NTPClient time_client(ntp_UDP, "south-america.pool.ntp.org", UTC_OFFSET, NTP_UPDATE_INTERVAL);

// --- Objetos WiFi e MQTT ---
WiFiClient espClient;
PubSubClient client(espClient);

// --- Variáveis Globais ---
String rxPacketString = "";
int rssi = 0;

// Variáveis para armazenar os dados recebidos
int idRecebido = 0;
float temperaturaRecebida = 0.0;
float umidadeRecebida = 0.0;
double latRecebida = 0.0;
double lonRecebida = 0.0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Inicializa Hardware da Placa
  initBoard(); 
  delay(1500);

  // --- Conecta ao WiFi ---
  setupWiFi();

  // --- Inicializa NTP ---
  time_client.begin();
  // Tenta forçar uma atualização inicial se o WiFi já estiver conectado
  if(WiFi.status() == WL_CONNECTED) {
      time_client.update();
  }

  // --- Configura MQTT ---
  client.setServer(mqtt_server, mqtt_port);

  // --- Inicializa LoRa ---
  Serial.println("Iniciando LoRa...");
  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("ERRO CRÍTICO: Falha ao iniciar LoRa!");
    while (1);
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  
  Serial.println("LoRa Iniciado com sucesso! Aguardando pacotes...");
}

void loop() {
  // Garante conexão MQTT
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();
    
    // Atualiza horário NTP
    time_client.update();
  }

  // Tenta processar pacote recebido via LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    onPacketReceived(packetSize);
  }
}

// --- Função Conexão WiFi ---
void setupWiFi() {
  delay(10);
  Serial.print("Conectando WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nWiFi Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nERRO: WiFi não conectou.");
  }
}

// --- Função Reconexão MQTT ---
void reconnectMQTT() {
  // Loop até conectar
  while ( client.connected()) {
    Serial.print("Tentando conexão MQTT no IP " + String(mqtt_server) + "... ");
    // Cria um ID de cliente aleatório
    String clientId = "ESP32LoRaGateway-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

// --- Processa o Pacote LoRa ---
void onPacketReceived(int packetSize) {
  rxPacketString = "";
  
  while (LoRa.available()) {
    rxPacketString += (char)LoRa.read();
  }
  
  rssi = LoRa.packetRssi();
  Serial.println("------------------------------------------------");
  Serial.printf("Pacote Recebido (%d bytes) | RSSI: %d\n", packetSize, rssi);
  Serial.println("Payload LoRa: " + rxPacketString);

  // --- PARSE DO JSON (Entrada) ---
  // Exemplo esperado: {"id":1,"t":25.0,"h":60.0,"lat":-22.90,"lon":-43.17}
  int n = sscanf(rxPacketString.c_str(), "{\"id\":%d,\"t\":%f,\"h\":%f,\"lat\":%lf,\"lon\":%lf}", 
                 &idRecebido, &temperaturaRecebida, &umidadeRecebida, &latRecebida, &lonRecebida);

  if (n >= 5) {
    Serial.println(">> Dados válidos! Enviando para MQTT e Google Sheets...");
    
    // Tenta enviar para MQTT (se falhar, apenas avisa no Serial e segue)
    sendDataToMQTT(idRecebido, temperaturaRecebida, umidadeRecebida, latRecebida, lonRecebida);
    
    // Envia para Google Sheets (independente do sucesso do MQTT)
    sendDataToSheet(idRecebido, temperaturaRecebida, umidadeRecebida, latRecebida, lonRecebida);
    
  } else {
    Serial.println(">> Erro: Formato JSON do LoRa inválido.");
  }
}

// --- Envia para Mosquitto MQTT ---
void sendDataToMQTT(int id, float temp, float humidity, double lat, double lon) {
    if (WiFi.status() == WL_CONNECTED && client.connected()) {
        
        // Formata Lat/Lon com precisão para String
        char latBuff[20];
        char lonBuff[20];
        sprintf(latBuff, "%.6f", lat);
        sprintf(lonBuff, "%.6f", lon);

        // Monta o JSON de Saída
        String jsonPayload = "{";
        jsonPayload += "\"id\":" + String(id) + ",";
        jsonPayload += "\"temperatura\":" + String(temp) + ",";
        jsonPayload += "\"umidade\":" + String(humidity) + ",";
        jsonPayload += "\"lat\":" + String(latBuff) + ",";
        jsonPayload += "\"long\":" + String(lonBuff) + ",";
        jsonPayload += "\"timestamp\": " + String(time_client.getEpochTime());
        jsonPayload += "}";

        // Publica no tópico
        Serial.print("Publicando em [");
        Serial.print(mqtt_topic);
        Serial.print("]: ");
        Serial.println(jsonPayload);

        if (client.publish(mqtt_topic, jsonPayload.c_str())) {
            Serial.println(">> Sucesso: Mensagem MQTT enviada.");
        } else {
            Serial.println(">> Erro: Falha no envio MQTT.");
        }

    } else {
        Serial.println("Erro: WiFi ou MQTT desconectado. Pulando MQTT...");
    }
}

// --- Envia para Google Sheets ---
void sendDataToSheet(int id, float temp, float humidity, double lat, double lon) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        // Monta a URL
        String url = GOOGLE_SCRIPT_URL;
        url += "?id=" + String(id);
        url += "&temperature=" + String(temp);
        url += "&humidity=" + String(humidity);
        
        // Formata Lat/Lon com precisão
        char buff[20];
        sprintf(buff, "%.6f", lat);
        url += "&lat=" + String(buff);
        sprintf(buff, "%.6f", lon);
        url += "&lon=" + String(buff);
        
        // --- MODIFICAÇÃO SOLICITADA: Adiciona Timestamp na URL ---
        url += "&timestamp=" + String(time_client.getEpochTime());

        // Inicia conexão (HTTPS inseguro para evitar erro de certificado no ESP32)
        WiFiClientSecure httpsClient; 
        httpsClient.setInsecure();
        
        Serial.print("Enviando HTTP GET para Google Sheets... ");
        
        // A biblioteca HTTPClient gerencia a conexão baseada no objeto client
        http.begin(httpsClient, url); 
        
        int httpCode = http.GET();

        if (httpCode > 0) {
            Serial.printf("Sucesso! Código: %d\n", httpCode);
        } else {
            Serial.printf("Falha! Erro: %s\n", http.errorToString(httpCode).c_str()); 
        }

        http.end();
    } else {
        Serial.println("Erro: WiFi desconectado. Dados não enviados para o Google.");
    }
}