/* Heltec LoRa Receiver (Gateway) - JSON Support
 * Função:
 * 1. Recebe JSON via LoRa (ID, Temp, Umid, Lat, Lon).
 * 2. Mostra no OLED.
 * 3. Envia para Google Sheets via WiFi.
 */

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <WiFi.h>
#include <HTTPClient.h>

// --- Configurações do LoRa ---
#define RF_FREQUENCY                915000000 // Hz
#define TX_OUTPUT_POWER             20        // dBm
#define LORA_BANDWIDTH              0         // [0: 125 kHz]
#define LORA_SPREADING_FACTOR       7         // [SF7..SF12]
#define LORA_CODINGRATE             1         // [1: 4/5]
#define LORA_PREAMBLE_LENGTH        8         
#define LORA_SYMBOL_TIMEOUT         0         
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false
#define RX_TIMEOUT_VALUE            1000

// IMPORTANTE: Aumentamos o buffer para caber o JSON inteiro
#define BUFFER_SIZE                 256 

// --- Configurações WiFi e Google ---
// PREENCHA AQUI SEUS DADOS
const char* ssid = "NOME_DA_SUA_REDE";
const char* password = "SENHA_DA_SUA_REDE";

// URL do Google Apps Script 
// (OBS: Seu Script precisa estar preparado para receber os novos parametros id, lat e lon)
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbycddfI6iDRxRRI0wtvtrKujGhEa_GJZOQvbtCdW6VA2D8optlGRFlo_7stcciZu2na/exec";

// --- Variáveis Globais ---
char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
int16_t rssi, rxSize;
bool lora_idle = true;

// Flag para indicar ao loop que temos um novo pacote
volatile bool pacoteRecebido = false; 

// Variáveis para armazenar os dados recebidos
int idRecebido = 0;
float temperaturaRecebida = 0.0;
float umidadeRecebida = 0.0;
double latRecebida = 0.0;
double lonRecebida = 0.0;

// Objeto display
extern SSD1306Wire display; 

void setup() {
    Serial.begin(115200);
    
    // Inicializa o Hardware MCU e Vext
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW); 
    delay(100);

    // Inicializa Display
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.clear();
    display.drawString(0, 0, "Iniciando Gateway...");
    display.display();

    // Conecta ao WiFi
    display.drawString(0, 15, "Conectando WiFi...");
    display.display();
    WiFi.begin(ssid, password);
    
    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    if(WiFi.status() == WL_CONNECTED){
        Serial.println("\nWiFi Conectado!");
        display.clear();
        display.drawString(0, 0, "WiFi OK!");
        display.drawString(0, 15, WiFi.localIP().toString());
    } else {
        Serial.println("\nErro no WiFi (continuara offline)");
        display.clear();
        display.drawString(0, 0, "WiFi Falhou!");
    }
    display.display();
    delay(2000);

    // Inicializa LoRa
    rssi = 0;
    RadioEvents.RxDone = OnRxDone;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                      0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
    
    Serial.println("Sistema Iniciado - Aguardando JSON...");
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 20, "Aguardando...");
    display.display();
}

void loop() {
    // Máquina de estados do LoRa
    if (lora_idle) {
        lora_idle = false;
        Radio.Rx(0);
    }
    Radio.IrqProcess();

    // --- Processamento dos dados recebidos ---
    if (pacoteRecebido) {
        pacoteRecebido = false; 

        // 1. Envia os dados completos para a planilha
        sendDataToSheet(idRecebido, temperaturaRecebida, umidadeRecebida, latRecebida, lonRecebida);

        // 2. Atualiza o Display
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        
        String linha1 = "ID:" + String(idRecebido) + " RSSI:" + String(rssi);
        display.drawString(0, 0, linha1);
        
        display.setFont(ArialMT_Plain_16);
        String linhaTemp = "T: " + String(temperaturaRecebida, 1) + "C  H: " + String(umidadeRecebida, 0) + "%";
        display.drawString(0, 15, linhaTemp);
        
        display.setFont(ArialMT_Plain_10);
        String linhaLat = "Lat: " + String(latRecebida, 5);
        String linhaLon = "Lon: " + String(lonRecebida, 5);
        display.drawString(0, 35, linhaLat);
        display.drawString(0, 48, linhaLon);

        display.display();

        lora_idle = true; 
    }
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi_val, int8_t snr) {
    rssi = rssi_val;
    rxSize = size;
    
    // Zera o buffer e copia os dados
    memset(rxpacket, 0, BUFFER_SIZE);
    memcpy(rxpacket, payload, size);
    
    Radio.Sleep();
    Serial.printf("\r\nJSON Recebido (%d bytes): %s\r\n", size, rxpacket);

    // --- PARSE DO JSON ---
    // Formato esperado: {"id":1,"t":25.0,"h":60.0,"lat":-22.90,"lon":-43.17}
    // Usamos sscanf para extrair os dados diretamente do texto
    int n = sscanf(rxpacket, "{\"id\":%d,\"t\":%f,\"h\":%f,\"lat\":%lf,\"lon\":%lf}", 
                   &idRecebido, &temperaturaRecebida, &umidadeRecebida, &latRecebida, &lonRecebida);

    if (n >= 5) { // Se conseguiu ler pelo menos as 5 variáveis
        Serial.println("Parse OK!");
        pacoteRecebido = true;
    } else {
        Serial.println("Erro ao ler JSON. Formato incompativel?");
        // Tenta uma leitura alternativa caso o sscanf falhe com aspas
        // (Isso é um backup simples)
        pacoteRecebido = true; 
    }
}

// --- FUNÇÃO DE ENVIO HTTP ATUALIZADA ---
void sendDataToSheet(int id, float temp, float humidity, double lat, double lon) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // Monta a URL com TODOS os parâmetros
        String url = GOOGLE_SCRIPT_URL;
        url += "?id=" + String(id);
        url += "&temperature=" + String(temp);
        url += "&humidity=" + String(humidity);
        // Enviamos lat/lon com alta precisão
        char buff[20];
        sprintf(buff, "%.6f", lat);
        url += "&lat=" + String(buff);
        sprintf(buff, "%.6f", lon);
        url += "&lon=" + String(buff);

        Serial.println("Enviando GET:");
        Serial.println(url);

        http.begin(url); 
        int httpCode = http.GET();

        if (httpCode > 0) {
            Serial.printf("HTTP Code: %d\n", httpCode);
        } else {
            Serial.printf("HTTP Falhou: %s\n", http.errorToString(httpCode).c_str()); 
        }

        http.end();
    } else {
        Serial.println("WiFi desconectado. Dados ignorados.");
    }
}