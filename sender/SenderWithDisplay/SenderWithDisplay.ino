/* Heltec Automation LoRa Sender - JSON Update
 * * Função: 
 * 1. Lê Temp/Umidade (DHT11).
 * 2. Junta com ID, Latitude e Longitude (Constantes).
 * 3. Formata tudo em JSON: {"id":1,"t":25.0,"h":60.0,"lat":-22.90,"lon":-43.17}
 * 4. Envia via LoRa.
 */

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "DHT.h"

// --- CONSTANTES DO USUÁRIO (DEFINA AQUI) ---
const int DEVICE_ID = 2;           // ID único deste dispositivo
const double FIXED_LAT = -23.5505; // Exemplo: Latitude de SP
const double FIXED_LON = -46.6333; // Exemplo: Longitude de SP

// --- Configuração do Sensor DHT11 ---
#define DHTPIN 4     
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

// --- Configurações do LoRa ---
#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             5        // dBm
#define LORA_BANDWIDTH                              0         
#define LORA_SPREADING_FACTOR                       7         
#define LORA_CODINGRATE                             1         
#define LORA_PREAMBLE_LENGTH                        8         
#define LORA_SYMBOL_TIMEOUT                         0         
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

// AUMENTAMOS O BUFFER: Um JSON com coordenadas ocupa mais espaço que o texto anterior.
// Antes era 30, agora 128 bytes para garantir.
#define BUFFER_SIZE                                 128 

char txpacket[BUFFER_SIZE];
bool lora_idle=true;

static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
    
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                       LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                       LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                       true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
                       
    Serial.println(F("LoRa Sender Iniciado - Modo JSON!"));
    dht.begin();
}

void loop() {
    if(lora_idle == true) {
        
        delay(2000); 
        
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        
        if (isnan(h) || isnan(t)) {
            Serial.println(F("Falha na leitura do sensor DHT!"));
            return;
        }

        Serial.print(F("Leitura Sensor - T: "));
        Serial.print(t);
        Serial.print(F(" H: "));
        Serial.println(h);
        
        // --- CRIAÇÃO DO PACOTE JSON ---
        // Estrutura: {"id":1,"t":25.5,"h":60.0,"lat":-23.550500,"lon":-46.633300}
        // Explicação dos formatos:
        // %d   -> Inteiro (ID)
        // %.1f -> Float com 1 casa decimal (Temp/Umidade)
        // %.6f -> Float com 6 casas decimais (Lat/Lon precisam de precisão)
        
        int len = snprintf(txpacket, BUFFER_SIZE, 
                           "{\"id\":%d,\"t\":%.1f,\"h\":%.1f,\"lat\":%.6f,\"lon\":%.6f}", 
                           DEVICE_ID, t, h, FIXED_LAT, FIXED_LON);
        
        // Verifica se o texto coube no buffer
        if (len >= BUFFER_SIZE || len < 0) {
            Serial.println(F("ERRO: O JSON ficou maior que o BUFFER_SIZE!"));
            return;
        }
        
        Serial.printf("\r\nEnviando JSON (%d bytes): %s\r\n", strlen(txpacket), txpacket);

        // Envia o pacote convertido para bytes (uint8_t *)
        Radio.Send( (uint8_t *)txpacket, strlen(txpacket) );
        lora_idle = false;
    }
    
    Radio.IrqProcess( );
}

void OnTxDone( void ) {
    Serial.println("TX concluído!");
    lora_idle = true; 
}

void OnTxTimeout( void ) {
    Radio.Sleep( );
    Serial.println("ERRO: Timeout TX!");
    lora_idle = true; 
}