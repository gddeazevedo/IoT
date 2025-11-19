#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <WiFi.h>
#include <HTTPClient.h>

// --- Configurações do LoRa ---
#define RF_FREQUENCY                915000000 // Hz
#define TX_OUTPUT_POWER             20        // dBm
#define LORA_BANDWIDTH              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR       7         // [SF7..SF12]
#define LORA_CODINGRATE             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false
#define RX_TIMEOUT_VALUE            1000
#define BUFFER_SIZE                 30 


const char* ssid = "";
const char* password = "";

// URL do Google Apps Script da conta do MicroProjeto
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbycddfI6iDRxRRI0wtvtrKujGhEa_GJZOQvbtCdW6VA2D8optlGRFlo_7stcciZu2na/exec";

// --- Variáveis Globais ---
char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
int16_t rssi, rxSize;
bool lora_idle = true;

// Flag para indicar ao loop que temos um novo pacote para mostrar/enviar
volatile bool pacoteRecebido = false; 

// Variáveis para armazenar os dados recebidos via LoRa
float temperaturaRecebida = 0.0;
float umidadeRecebida = 0.0;

// Objeto display (extern)
extern SSD1306Wire display; 

void setup() {
    Serial.begin(115200);
    
    // Inicializa o Hardware MCU e Vext (Energia do OLED/LoRa)
    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
    
    // Inicializa Vext manualmente para garantir energia na tela
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW); // Liga a energia
    delay(100);

    // Inicializa Display (Usando o objeto da biblioteca)
    display.init();
    display.setFont(ArialMT_Plain_10);
    display.clear();
    display.drawString(0, 0, "Iniciando Sistema...");
    display.display();

    // Conecta ao WiFi
    display.drawString(0, 15, "Conectando WiFi...");
    display.display();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        display.drawString(0, 30, "."); // Adiciona um ponto visual no display
        display.display();
    }
    Serial.println("\nConectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    display.clear();
    display.drawString(0, 0, "WiFi Conectado!");
    display.drawString(0, 15, "IP: " + WiFi.localIP().toString());
    display.display();
    delay(2000); // Mostra o IP por um breve momento

    // Inicializa LoRa
    rssi = 0;
    RadioEvents.RxDone = OnRxDone;
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                      0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
    
    Serial.println("Sistema Iniciado - Aguardando Pacotes...");
}

void loop() {
    // Máquina de estados do LoRa
    if (lora_idle) {
        lora_idle = false;
        Radio.Rx(0);
    }
    Radio.IrqProcess();

    // --- Lógica de Display e Envio de Dados no LOOP ---
    if (pacoteRecebido) {
        pacoteRecebido = false; // Reseta a flag

        // 1. Envia os dados para a planilha
        sendDataToSheet(temperaturaRecebida, umidadeRecebida);

        // 2. Atualiza o Display
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        
        String info = "RSSI: " + String(rssi) + " | Len: " + String(rxSize);
        display.drawString(0, 0, info);
        
        display.setFont(ArialMT_Plain_16);
        String textoTemp = "Temp: " + String(temperaturaRecebida, 1) + " *C";
        String textoUmid = "Umid: " + String(umidadeRecebida, 1) + " %%";
        display.drawString(0, 20, textoTemp);
        display.drawString(0, 40, textoUmid);

        display.display();

        // 3. Volta a ouvir
        lora_idle = true; 
    }
}

// Esta função roda dentro da Interrupção (IRQ).
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi_val, int8_t snr) {
    // Copia os dados
    rssi = rssi_val;
    rxSize = size;
    memcpy(rxpacket, payload, size);
    rxpacket[size] = '\0';
    
    Radio.Sleep();
    
    Serial.printf("\r\nPacote Recebido: \"%s\" com RSSI %d\r\n", rxpacket, rssi);

    // --- LÓGICA PARA EXTRAIR DADOS DO PACOTE ---
    String pacote = String(rxpacket);
    int tIndex = pacote.indexOf("T:");
    int hIndex = pacote.indexOf("H:");
    int commaIndex = pacote.indexOf(',');
    
    if (tIndex != -1 && hIndex != -1 && commaIndex != -1) {
        // Extrai Temperatura
        String tempStr = pacote.substring(tIndex + 2, commaIndex);
        temperaturaRecebida = tempStr.toFloat();
        
        // Extrai Umidade
        String humStr = pacote.substring(hIndex + 2);
        umidadeRecebida = humStr.toFloat();
        Serial.printf("Dados extraídos: T=%.1f, H=%.1f\r\n", temperaturaRecebida, umidadeRecebida);
    } else {
        Serial.println("Erro ao analisar pacote. Formato esperado: T:xx.x,H:yy.y");
    }

    // Apenas avisa o loop.
    pacoteRecebido = true; 
}

// --- FUNÇÃO DE ENVIO HTTP (Adaptada do seu código original) ---
void sendDataToSheet(float temp, float humidity) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // Constrói a URL com os parâmetros
        String url = GOOGLE_SCRIPT_URL;
        url += "?temperature=" + String(temp);
        url += "&humidity=" + String(humidity);

        Serial.println("Enviando dados para a planilha...");
        Serial.println(url);

        // Inicia a requisição HTTP GET
        http.begin(url); 
        int httpCode = http.GET();

        if (httpCode == 200 || httpCode == 302 || httpCode == 408) {
            Serial.println("Dados gravados com sucesso.");
        } else {
            Serial.print("Falha ao gravar dados. Erro: ");
            Serial.println(http.errorToString(httpCode).c_str()); 
        }

        http.end();
    } else {
        Serial.println("WiFi desconectado. Não foi possível enviar os dados.");
    }
}
