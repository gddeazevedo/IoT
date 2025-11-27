/* * Heltec/ESP32 LoRa Receiver (Gateway) - VERSÃO SEM DISPLAY
 * Função:
 * 1. Recebe JSON via LoRa.
 * 2. Envia para Google Sheets via WiFi.
 * 3. Status apenas via Serial Monitor.
 */

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "boards.h" // Arquivo com definição dos pinos (RADIO_CS_PIN, etc.)

// --- Configurações do LoRa ---
#define LORA_FREQUENCY      915E6  // 915 MHz
#define LORA_SPREADING_FACTOR 7    // Deve ser igual ao transmissor
#define LORA_BANDWIDTH      125E3  
#define TX_POWER            20

// --- Configurações WiFi e Google ---
const char* ssid = "AndroidA";
const char* password = "PedroVictor";

// URL do Google Apps Script
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbySpAERCOG6Q06EhBCSYvbr_ueC0DZX4PysnpwsYqsYGD3ir7B9GSrk__OtQC18hyw/exec";

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

  // Inicializa Hardware da Placa (definido no boards.h)
  initBoard(); 
  delay(1500);

  // --- Conecta ao WiFi ---
  Serial.print("Conectando WiFi");
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
    Serial.println("\nERRO: WiFi não conectou. O sistema rodará offline (sem Google Sheets).");
  }

  // --- Inicializa LoRa ---
  Serial.println("Iniciando LoRa...");
  
  // Define os pinos do módulo LoRa (Vêm do arquivo boards.h)
  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("ERRO CRÍTICO: Falha ao iniciar LoRa!");
    while (1); // Trava o sistema se não houver rádio
  }

  // Configurações de rádio
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  // LoRa.setCodingRate4(5); 
  
  Serial.println("LoRa Iniciado com sucesso! Aguardando pacotes...");
}

void loop() {
  // Tenta processar pacote recebido
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    onPacketReceived(packetSize);
  }
}

// --- Processa o Pacote LoRa ---
void onPacketReceived(int packetSize) {
  rxPacketString = "";
  
  // Ler todo o conteúdo do pacote
  while (LoRa.available()) {
    rxPacketString += (char)LoRa.read();
  }
  
  rssi = LoRa.packetRssi();
  Serial.println("------------------------------------------------");
  Serial.printf("Pacote Recebido (%d bytes) | RSSI: %d\n", packetSize, rssi);
  Serial.println("Payload: " + rxPacketString);

  // --- PARSE DO JSON ---
  // Formato esperado: {"id":1,"t":25.0,"h":60.0,"lat":-22.90,"lon":-43.17}
  int n = sscanf(rxPacketString.c_str(), "{\"id\":%d,\"t\":%f,\"h\":%f,\"lat\":%lf,\"lon\":%lf}", 
                  &idRecebido, &temperaturaRecebida, &umidadeRecebida, &latRecebida, &lonRecebida);

  if (n >= 5) {
    Serial.println(">> JSON Válido! Enviando para nuvem...");
    sendDataToSheet(idRecebido, temperaturaRecebida, umidadeRecebida, latRecebida, lonRecebida);
  } else {
    Serial.println(">> Erro: Formato JSON inválido ou incompleto.");
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

        // Inicia conexão (HTTPS inseguro para evitar erro de certificado no ESP32)
        WiFiClientSecure client;
        client.setInsecure();
        
        Serial.print("Enviando HTTP GET... ");
        
        // A biblioteca HTTPClient gerencia a conexão baseada no objeto client
        http.begin(client, url); 
        
        int httpCode = http.GET();

        if (httpCode > 0) {
            Serial.printf("Sucesso! Código: %d\n", httpCode);
        } else {
            Serial.printf("Falha! Erro: %s\n", http.errorToString(httpCode).c_str()); 
        }

        http.end();
    } else {
        Serial.println("Erro: WiFi desconectado. Dados não enviados.");
    }
}