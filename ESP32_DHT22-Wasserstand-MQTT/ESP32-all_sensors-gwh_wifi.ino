
#include "DHT.h"
#include "Adafruit_Sensor.h"
#include "Wire.h"
#include <WiFi.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <math.h>

//zum Empfangen der Messages im Topic
// mosquitto_sub -h [BROKER_ADRESS] test.mosquitto.org -t [MQTT_CLIENT_ID] "franzzz-esp32-001/loopback"


const char WIFI_SSID[] = "gwh-wifi";     // CHANGE TO YOUR WIFI SSID
const char WIFI_PASSWORD[] = "chili-24";  // CHANGE TO YOUR WIFI PASSWORD

const char MQTT_BROKER_ADRRESS[] = "192.168.1.135";  // CHANGE TO MQTT BROKER'S ADDRESS // RASPBERRY-PI IP-ADRESS
const int MQTT_PORT = 1883;
const char MQTT_CLIENT_ID[] = "gwh-MQTT";  // CHANGE IT AS YOU DESIRE
const char MQTT_USERNAME[] = "";                        // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";                        // CHANGE IT IF REQUIRED, empty if not required

// The MQTT topics that ESP32 should publish/subscribe
const char PUBLISH_TOPIC[] = "gwh-data";    // CHANGE IT AS YOU DESIRE
const char SUBSCRIBE_TOPIC[] = "gwh-data";  // CHANGE IT AS YOU DESIRE

const int PUBLISH_INTERVAL = 5000;  // 5 seconds

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

unsigned long lastPublishTime = 0;


//define PINs and voltage reference
#define WATER_LEVEL_PIN 34
#define WATER_POWER_PIN 32
#define EMERGENCY_WATER_PIN 33  
#define DHTPIN 23
#define DHTTYPE DHT22
#define TDS_PIN 35
#define TDS_POWER_PIN 26
#define VREF 3.3


//DHT-Sensor Setup 
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);

  // set the ADC attenuation to 11 dB (up to ~3.3V input)
  //analogSetAttenuation(ADC_11db);

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
  pinMode(EMERGENCY_WATER_PIN, INPUT);
  digitalWrite(WATER_POWER_PIN, LOW);
  pinMode(TDS_POWER_PIN, OUTPUT);
  pinMode(TDS_PIN, INPUT);
  digitalWrite(TDS_POWER_PIN, LOW);
  dht.begin();
}



void loop() {
  // Read and send data to the MQTT-Broker (Raspberry-Pi)
  digitalWrite(WATER_POWER_PIN, HIGH);
  digitalWrite(TDS_POWER_PIN, HIGH);
  delay(100);
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int water_level = analogRead(WATER_LEVEL_PIN);
  int emergency_water = analogRead(EMERGENCY_WATER_PIN);
  float water_quality = analogRead(TDS_PIN);
  digitalWrite(WATER_POWER_PIN, LOW);
  digitalWrite(TDS_POWER_PIN, LOW);

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Fehler beim Auslesen des DHT22 Sensors!");
    return;
  }
  if (isnan(water_level) || isnan(emergency_water)) {
    Serial.println("Fehler beim Auslesen des Wasserstand Sensors!");
    return;
  }
  if (isnan(water_quality)) {
    Serial.println("Fehler beim Auslesen des TDS-Sensors!");
    return;
  }

  //The water temperature needs to be accounted for the TDS-measurement
  float temp_coeff = 1.0 + 0.2*(temperature-25.0);                      //This is supposed to be water-temperature (not air-temperature), but it results a very small error for temperature changes +/-10°C
  float compensation_voltage = water_quality * (float)VREF / 4096.0 / temp_coeff;
  water_quality = (133.42*compensation_voltage*compensation_voltage*compensation_voltage - 255.86*compensation_voltage*compensation_voltage + 857.39*compensation_voltage)*0.5;

  mqtt.loop();

  if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
    sendToMQTT(temperature, humidity, water_level, emergency_water, water_quality);
    lastPublishTime = millis();
  }


  Serial.print("Temperatur: ");
  Serial.println(temperature);
  Serial.print("Luftfeuchte: ");
  Serial.println(humidity);
  Serial.print("Wasserstand: ");
  Serial.println(water_level);
  Serial.print("Notfall-Wasser: ");
  Serial.println(emergency_water);
  Serial.print("Wasserqualität: ");
  Serial.print(water_quality);

  delay(5000);
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


//Sending the data via MQTT-protocol
void sendToMQTT(float temp, float humid, int water_lvl, int emergency_water, float water_quality) {
  StaticJsonDocument<200> message;
  message["timestamp"] = millis();
  message["CURRENT_TEMPERATURE"] = roundf(temp); 
  message["CURRENT_HUMIDITY"] = roundf(humid);
  message["WATER_STATE"] = water_lvl;
  message["EMERGENCY_WATER"] = emergency_water;
  message["WATER_QUALITY"] = water_quality;
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
