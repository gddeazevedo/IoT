/* Heltec Automation send communication test example
 *
 * Função: 
 * 1. Lê a temperatura e umidade do DHT11.
 * 2. Formata os dados no padrão: "T:xx.x,H:yy.y"
 * 3. Envia o pacote via LoRa.
 *
 * Baseado no código original fornecido.
 * */

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "DHT.h"

// --- Configuração do Sensor DHT11 ---
#define DHTPIN 4     // Pino Digital conectado ao sensor DHT (AJUSTE SE NECESSÁRIO)
#define DHTTYPE DHT11  // Sensor DHT 11
DHT dht(DHTPIN, DHTTYPE);


// --- Configurações do LoRa (Mantidas) ---
#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             5        // dBm
#define LORA_BANDWIDTH                              0         
#define LORA_SPREADING_FACTOR                       7         
#define LORA_CODINGRATE                             1         
#define LORA_PREAMBLE_LENGTH                        8         
#define LORA_SYMBOL_TIMEOUT                         0         
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define BUFFER_SIZE                                 30 // Define o tamanho do pacote

char txpacket[BUFFER_SIZE];
// char rxpacket[BUFFER_SIZE]; // Não é necessário no Sender
// double txNumber; // Não é necessário mais, usaremos T e H

bool lora_idle=true;

static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );
// void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr ); // Não é necessário no Sender

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
	
    //txNumber=0; // Removido

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                       LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                       LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                       true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
                       
    Serial.println(F("LoRa Sender Iniciado com DHT11!"));

    dht.begin();
}


void loop() {
	if(lora_idle == true) {
        
        // --- 1. LEITURA DO SENSOR ---
        delay(2000); // Adiciona um pequeno delay, DHT11 precisa de tempo entre leituras
        
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        
        if (isnan(h) || isnan(t)) {
            Serial.println(F("Falha na leitura do sensor DHT!"));
            // O Sender permanecerá lora_idle=true e tentará novamente no próximo loop
            return;
        }

        Serial.print(F("Lido: Umidade: "));
        Serial.print(h);
        Serial.print(F("%  Temperatura: "));
        Serial.print(t);
        Serial.println(F("°C"));
        
        // --- 2. PREPARAÇÃO DO PACOTE (CHAVE) ---
        // Usamos snprintf para formatar a string no padrão "T:xx.x,H:yy.y"
        // O %.1f garante 1 casa decimal
        int len = snprintf(txpacket, BUFFER_SIZE, "T:%.1f,H:%.1f", t, h);
        
        if (len >= BUFFER_SIZE || len < 0) {
            Serial.println(F("ERRO: Pacote muito grande para o buffer!"));
            return;
        }
		
        Serial.printf("\r\nEnviando pacote LoRa: \"%s\" , length %d\r\n",txpacket, strlen(txpacket));

        // --- 3. ENVIO VIA LoRa ---
        Radio.Send( (uint8_t *)txpacket, strlen(txpacket) );
        lora_idle = false;
	}
    
    // Processa a interrupção do rádio
    Radio.IrqProcess( );
}

void OnTxDone( void ) {
	Serial.println("TX concluído com sucesso!");
	lora_idle = true; // Permite o próximo envio
}

void OnTxTimeout( void ) {
    Radio.Sleep( );
    Serial.println("ERRO: Timeout no envio TX!");
    lora_idle = true; // Permite o próximo envio
}
