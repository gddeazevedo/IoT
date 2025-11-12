#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// Configurações do WiFi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Configurações do Sensor DHT22
#define DHTPIN 4       // Pino onde o DHT22 está conectado
#define DHTTYPE DHT22  // Define o tipo do sensor
DHT dht(DHTPIN, DHTTYPE);

// URL do Google Apps Script 
// Essa url é a da planilha da minha conta. O ideal é criar uma outra conta do google e gerar um novo link para botar aqui
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbycddfI6iDRxRRI0wtvtrKujGhEa_GJZOQvbtCdW6VA2D8optlGRFlo_7stcciZu2na/exec";


// Intervalo entre envios (em milissegundos)
long interval = 10000; // 30 segundos
unsigned long previousMillis = 0;

void setup() {
    Serial.begin(115200);
    dht.begin();

    // Conecta ao WiFi
    Serial.print("Conectando ao WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }
    Serial.println("\nConectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
}

void loop() {
    //Tô usando o milis pra n travar o sensor com delay
    unsigned long currentMillis = millis();

    // Verifica se o intervalo de tempo passou
    if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Lê os dados do sensor
    float h = dht.readHumidity();
    float t = dht.readTemperature(); // Lê em Celsius

    // Verifica se a leitura falhou
    if (isnan(h) || isnan(t)) {
        Serial.println("Falha ao ler o sensor DHT!");
        return;
    }

    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.print(" *C, Umidade: ");
    Serial.print(h);
    Serial.println(" %%");

    // Envia os dados para o Google Sheets
    sendDataToSheet(t, h);
    }
}

void sendDataToSheet(float temp, float humidity) {
    if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Constrói a URL com os parâmetros
    String url = GOOGLE_SCRIPT_URL;
    url += "?temperature=" + String(temp);
    url += "&humidity=" + String(humidity);

    Serial.println("Enviando dados para a planilha...");

    // Inicia a requisição HTTP GET
    http.begin(url); 
    int httpCode = http.GET();

    // O Google Apps Script costuma retornar 200 (OK) ou 302 (Redirecionamento)
    // quando funciona corretamente.
    if (httpCode == 200 || httpCode == 302 || httpCode == 408) {
        Serial.println("Dados gravados com sucesso.");
    } else {
        // Informa se houve um erro de conexão (código < 0) ou um erro do servidor (código > 0)
        Serial.print("Falha ao gravar dados. Erro: ");
        Serial.println(http.errorToString(httpCode).c_str()); 
    }

    http.end();
    } else {
    Serial.println("WiFi desconectado. Não foi possível enviar os dados.");
    }
}