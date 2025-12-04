/* Heltec Automation LoRa Sender + MQTT Publisher
 * Função: 
 * 1. Lê Temp/Umidade (DHT11).
 * 2. Envia via LoRa (P2P).
 * 3. Envia via MQTT (WiFi).
 */

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "DHT.h"
#include <WiFi.h>
#include <PubSubClient.h> // Necessário instalar essa biblioteca

// --- CONFIGURAÇÕES WIFI E MQTT (EDITE AQUI) ---
const char* ssid = "AndroidA";
const char* password = "PedroVictor";
const char* mqtt_server = "172.28.25.105"; // Exemplo de broker público
const int mqtt_port = 1883;
const char* mqtt_topic = "cefet/iot";

// --- CONSTANTES DO USUÁRIO ---
const int DEVICE_ID = 2;           
const double FIXED_LAT = -23.5505; 
const double FIXED_LON = -46.6333; 

// --- Configuração do Sensor DHT11 ---
#define DHTPIN 4     
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

// --- Configurações do LoRa ---
#define RF_FREQUENCY                                915000000 
#define TX_OUTPUT_POWER                             5        
#define LORA_BANDWIDTH                              0         
#define LORA_SPREADING_FACTOR                       7         
#define LORA_CODINGRATE                             1         
#define LORA_PREAMBLE_LENGTH                        8         
#define LORA_SYMBOL_TIMEOUT                         0         
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define BUFFER_SIZE                                 128 
char txpacket[BUFFER_SIZE];
bool lora_idle=true;

static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );

// --- Objetos WiFi e MQTT ---
WiFiClient espClient;
PubSubClient client(espClient);

// --- Função para conectar ao WiFi ---
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Conectando-se a rede: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi conectado");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

// --- Função para reconectar ao MQTT se cair ---
void reconnect() {
    // Loop até reconectar
    while (!client.connected()) {
        Serial.print("Tentando conexão MQTT...");
        // Cria um ID de cliente aleatório
        String clientId = "HeltecLoRaClient-";
        clientId += String(random(0xffff), HEX);
        
        // Tenta conectar
        if (client.connect(clientId.c_str())) {
            Serial.println("conectado!");
        } else {
            Serial.print("falhou, rc=");
            Serial.print(client.state());
            Serial.println(" tentando novamente em 5 segundos");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
    
    // Inicializa Sensor
    dht.begin();

    // Inicializa LoRa
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                       LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                       LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                       true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
    
    // Inicializa WiFi e MQTT
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    
    Serial.println(F("Sistema Híbrido (LoRa + MQTT) Iniciado!"));
}

void loop() {
    // Mantém a conexão MQTT ativa
    if (!client.connected()) {
        reconnect();
    }
    client.loop(); // Importante para processar mensagens recebidas e manter conexão

    if(lora_idle == true) {
        
        delay(2000); 
        
        float h = 10.0 //dht.readHumidity();
        float t = 23.5 //dht.readTemperature();
        
        if (isnan(h) || isnan(t)) {
            Serial.println(F("Falha na leitura do sensor DHT!"));
            return;
        }

        // --- Prepara o JSON ---
        int len = snprintf(txpacket, BUFFER_SIZE, 
                           "{\"id\":%d,\"t\":%.1f,\"h\":%.1f,\"lat\":%.6f,\"lon\":%.6f}", 
                           DEVICE_ID, t, h, FIXED_LAT, FIXED_LON);
        
        if (len >= BUFFER_SIZE || len < 0) {
            Serial.println(F("ERRO: JSON muito grande!"));
            return;
        }
        
        Serial.printf("\r\n[Dados] %s\r\n", txpacket);

        // --- 1. ENVIA VIA LORA ---
        Serial.print("Enviando LoRa... ");
        Radio.Send( (uint8_t *)txpacket, strlen(txpacket) );
        lora_idle = false;

        // --- 2. ENVIA VIA MQTT ---
        Serial.print("Publicando MQTT... ");
        if (client.publish(mqtt_topic, txpacket)) {
            Serial.println("Sucesso!");
        } else {
            Serial.println("Falha ao publicar.");
        }
    }
    
    Radio.IrqProcess( );
}

void OnTxDone( void ) {
    Serial.println("LoRa TX concluído!");
    lora_idle = true; 
}

void OnTxTimeout( void ) {
    Radio.Sleep( );
    Serial.println("ERRO: LoRa Timeout!");
    lora_idle = true; 
}
