#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"

// --- Configurações do LoRa ---
#define RF_FREQUENCY                915000000 // Hz
#define TX_OUTPUT_POWER             20        // dBm
#define LORA_BANDWIDTH              0         // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR       7         // [SF7..SF12]
#define LORA_CODINGRATE             1         // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false
#define RX_TIMEOUT_VALUE            1000
#define BUFFER_SIZE                 30 

// --- Variáveis Globais ---
char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
int16_t rssi, rxSize;
bool lora_idle = true;

// Flag para indicar ao loop que temos um novo pacote para mostrar
volatile bool pacoteRecebido = false; 

// --- CORREÇÃO AQUI ---
// Removemos a linha "SSD1306Wire display(...)"
// Adicionamos "extern" para dizer ao código: "Use o display que a biblioteca LoRaWan_APP já criou"
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
    display.drawString(0, 0, "Iniciando LoRa...");
    display.display();

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
        Serial.println("Entrando em modo RX");
        Radio.Rx(0);
    }
    Radio.IrqProcess();

    // --- Lógica de Display no LOOP (Seguro contra Core Panic) ---
    if (pacoteRecebido) {
        pacoteRecebido = false; // Reseta a flag

        // Atualiza o Display
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        
        String info = "RSSI: " + String(rssi) + " | Len: " + String(rxSize);
        display.drawString(0, 0, info);
        
        display.setFont(ArialMT_Plain_16);
        // Limita o tamanho do texto para não quebrar a tela se for lixo de memória
        String textoPacket = String(rxpacket);
        if(textoPacket.length() > 15) textoPacket = textoPacket.substring(0, 15);
        display.drawString(0, 20, textoPacket);
        
        display.setFont(ArialMT_Plain_10);
        display.drawString(0, 50, "Time: " + String(millis()));
        
        display.display();

        // Volta a ouvir
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

    // Apenas avisa o loop. NÃO DESENHE NO DISPLAY AQUI.
    pacoteRecebido = true; 
}