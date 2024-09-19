
#include "DHT.h"
#include "Adafruit_Sensor.h"
#include "Wire.h"
#include <WiFi.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <math.h>

const char WIFI_SSID[] = "";     // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "";  // CHANGE TO YOUR WIFI PASSWORD

const char MQTT_BROKER_ADRRESS[] = "test.mosquitto.org";  // CHANGE TO MQTT BROKER'S ADDRESS
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "franzzz-esp32-001";  // CHANGE IT AS YOU DESIRE
const char MQTT_USERNAME[] = "";                        // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";                        // CHANGE IT IF REQUIRED, empty if not required

// The MQTT topics that ESP32 should publish/subscribe
const char PUBLISH_TOPIC[] = "franzzz-esp32-001/loopback";    // CHANGE IT AS YOU DESIRE
const char SUBSCRIBE_TOPIC[] = "franzzz-esp32-001/loopback";  // CHANGE IT AS YOU DESIRE

const int PUBLISH_INTERVAL = 5000;  // 5 seconds

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

unsigned long lastPublishTime = 0;



#define WATER_LEVEL_PIN 34
#define WATER_POWER_PIN 32
#define DHTPIN 23
#define DHTTYPE DHT22


//DHT-Sensor Setup 
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // put your setup code hee, to run once:
  Serial.begin(115200);


  // set the ADC attenuation to 11 dB (up to ~3.3V input)
  analogSetAttenuation(ADC_11db);

   WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("ESP32 - Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  connectToMQTT();


  analogReadResolution(10);
  pinMode(WATER_POWER_PIN, OUTPUT);
  pinMode(WATER_LEVEL_PIN, INPUT);
  digitalWrite(WATER_POWER_PIN, LOW);
  dht.begin();
}

void loop() {
  // Auslesen der Sensoren und Senden an den MQTT-Broker
  digitalWrite(WATER_POWER_PIN, HIGH);
  delay(100);
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int water_level = analogRead(WATER_LEVEL_PIN);
  digitalWrite(WATER_POWER_PIN, LOW);


  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Fehler beim Auslesen des DHT22 Sensors!");
    return;
  }
  if (isnan(water_level)) {
    Serial.println("Fehler beim Auslesen des Wasserstand Sensors!");
    return;
  }
  mqtt.loop();

  if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
    sendToMQTT(temperature, humidity, water_level);
    lastPublishTime = millis();
  }


  Serial.print("Temperatur: ");
  Serial.println(temperature);
  Serial.print("Luftfeuchte: ");
  Serial.println(humidity);
  Serial.print("Sensor value: ");
  Serial.println(water_level);

  delay(2000);
}




void connectToMQTT() {
  // Connect to the MQTT broker
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);

  // Create a handler for incoming messages
  mqtt.onMessage(messageHandler);

  Serial.print("ESP32 - Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("ESP32 - MQTT broker Timeout!");
    return;
  }

  // Subscribe to a topic, the incoming messages are processed by messageHandler() function
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("ESP32 - Subscribed to the topic: ");
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println("ESP32 - MQTT broker Connected!");
}

void sendToMQTT(float temp, float humid, int water_lvl) {
  StaticJsonDocument<200> message;
  message["timestamp"] = millis();
  message["Temperatur"] = roundf(temp);  // Sensordaten zum Senden
  message["Luftfeuchte"] = roundf(humid);
  message["Wasserstand"] = water_lvl;
  char messageBuffer[512];
  serializeJson(message, messageBuffer);

  mqtt.publish(PUBLISH_TOPIC, messageBuffer);

  Serial.println("ESP32 - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(PUBLISH_TOPIC);
  Serial.print("- payload:");
  Serial.println(messageBuffer);
}

void messageHandler(String &topic, String &payload) {
  Serial.println("ESP32 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);
}