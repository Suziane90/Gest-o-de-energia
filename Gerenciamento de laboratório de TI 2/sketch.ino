#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Configuração Wi-Fi
const char* WIFI_NAME = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// Configuração ThingSpeak
const char* thingSpeakApiKey = "LXZ3HY7546P6KG5Z";
const char* server = "api.thingspeak.com";

// Configuração MQTT (HiveMQ Cloud)
const char* mqtt_server = "639526b830b64afe9214ef4328c64bb0.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "hivemq.webclient.1740703471593";
const char* mqtt_password = "Kg6!3j*N?8cJGOz9d&Ai";
const char* mqtt_topic = "energia/consumo";

// Cliente MQTT seguro
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Definição de pinos
#define PIR_PIN 4
#define RELAY_PIN 5
#define LED1_PIN 18
#define LED2_PIN 19
#define LED3_PIN 17
#define POT_PIN 34

float energiaConsumida = 0;
unsigned long ultimoTempo = 0;
float taxaConsumo;
const float limiteEnergia = 50.0;

// Conectar ao Wi-Fi
void conectarWiFi() {
    Serial.print("Conectando ao Wi-Fi ");
    WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
}

// Função de Callback MQTT
void callback(char* topic, byte* message, unsigned int length) {
    Serial.print("Mensagem recebida no tópico: ");
    Serial.println(topic);
}

// Conectar ao MQTT
void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Conectando ao MQTT...");
        if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
            Serial.println(" Conectado!");
            client.subscribe(mqtt_topic);
        } else {
            Serial.print(" Falha, rc=");
            Serial.print(client.state());
            Serial.println(" Tentando novamente em 5s...");
            delay(5000);
        }
    }
}

// Enviar dados para ThingSpeak
bool sendDataToThingSpeak(float energia) {
    WiFiClient client;
    if (!client.connect(server, 80)) {
        return false;
    }
    String data = "api_key=" + String(thingSpeakApiKey) + "&field1=" + String(energia, 1);
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: " + String(server) + "\n");
    client.print("Connection: close\n");
    client.print("LXZ3HY7546P6KG5Z: " + String(thingSpeakApiKey) + "\n");
    client.print("Content-Length: " + String(data.length()) + "\n\n");
    client.print(data);
    delay(1000);
    client.stop();
    return true;
}

void setup() {
    Serial.begin(115200);
    pinMode(PIR_PIN, INPUT);
    pinMode(POT_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    pinMode(LED3_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);
    digitalWrite(LED3_PIN, LOW);

    conectarWiFi();
    espClient.setInsecure(); 
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    reconnectMQTT();
    ultimoTempo = millis();
}

void loop() {
    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();

    bool presenceDetected = digitalRead(PIR_PIN);
    int valorPot = analogRead(POT_PIN);
    float percentual = valorPot / 4095.0; 
    taxaConsumo = percentual * 1000.0;

    unsigned long tempoAtual = millis();
    float tempoDecorrido = (tempoAtual - ultimoTempo) / 1000.0;
    energiaConsumida += (taxaConsumo / 3600.0) * tempoDecorrido; // Convertendo W para Wh
    ultimoTempo = tempoAtual;

    if (taxaConsumo > limiteEnergia) {
        digitalWrite(LED3_PIN, HIGH);
        Serial.println("!!! ALERTA, ALTO CONSUMO DE ENERGIA !!!");
    } else {
        digitalWrite(LED3_PIN, LOW);
    }

    if (!presenceDetected) {
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED1_PIN, LOW);
        digitalWrite(LED2_PIN, LOW);
        Serial.println("Presença não detectada. Dispositivos desligados.");
    } else {
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED1_PIN, HIGH);
        digitalWrite(LED2_PIN, HIGH);
        Serial.println("Presença detectada. Dispositivos ligados.");
    }

    Serial.print("Taxa de Consumo: ");
    Serial.print(taxaConsumo, 1);
    Serial.print(" kWh/s | Energia: ");
    Serial.print(energiaConsumida, 1);
    Serial.print(taxaConsumo, 1);
    Serial.println(" kWh");

    if (sendDataToThingSpeak(taxaConsumo)) {
        Serial.println("✅ ThingSpeak atualizado!");
    } else {
        Serial.println("Falha no envio ao ThingSpeak!");
    }

    String energiaStr = String(taxaConsumo, 1);
    client.publish(mqtt_topic, energiaStr.c_str());
    Serial.println("✅ Enviado ao MQTT!");

    delay(5000);
}
