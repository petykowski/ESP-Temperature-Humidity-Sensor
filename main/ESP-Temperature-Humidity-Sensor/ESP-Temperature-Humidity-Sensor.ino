#include <WiFi.h>
#include "config.h"
#include <DHTesp.h>
#include <HTTPClient.h>

/* Declare Environment Variables */
DHTesp dht;

float temperature;
float humidity;

unsigned long lastTime = 0;
unsigned long timerDelay = 600000; /* 10 minutes in milliseconds */

uint8_t esp_mac[12];
char deviceId[20];


/*
  Connect to WiFi
  Attempts to establish a wireless connection using the 
  SSID and Password provided in the config.h file.
*/
void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(wifiSSID);

  WiFi.begin(wifiSSID, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
}


/*
  Assign Device ID
  Sets the ESP's default MAC address as the device ID. 
*/
void assignDeviceId() {
  /* Assign MAC Address as Device ID */
  esp_efuse_mac_get_default(esp_mac);

  /* Format as Human Readable */
  for(int m = 0; m<6; m++)
    sprintf(deviceId, "%02x:%02x:%02x:%02x:%02x:%02x", esp_mac[0],esp_mac[1],esp_mac[2],esp_mac[3], esp_mac[4],esp_mac[5]);
    printf("Device ID: %s\n", deviceId);
}

/*
  Setup DHT Sensor
  Configures DHT22 Sensor
*/
void setupDHTSensor() {
  // Pin
  int dhtPin = 4;

  // Initialize Sensor
  dht.setup(dhtPin, DHTesp::DHT22);
  Serial.println("DHT22 sensor configured");
}


/*
  Read Sensor Data
  Stores the current Temperature and Humidity values into
  their respective environment variables.
*/
void readSensorData(void * parameter) {
  Serial.println("Reading sensor data");
  TempAndHumidity lastValues = dht.getTempAndHumidity();  
  temperature = lastValues.temperature;
  humidity = lastValues.humidity;
  Serial.println("Sensor values updated");
}


void setup() {
  /* Stream to 115200 */
  Serial.begin(115200);

  /* Finish Setup */
  setupDHTSensor();
  connectToWiFi();
  assignDeviceId();
}

void loop() {
  /* Send an HTTP POST request in timerDelay length intervals */
  if ((millis() - lastTime) > timerDelay) {

    /* Check WiFi is Connected */
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

      readSensorData(NULL);

      /* Open HTTP Session */
      http.begin(database);

      /* Build Request */
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{\"value\":\"" + String(temperature, 2) + "\",\"unit\":\"C\",\"type\":\"T\",\"deviceId\":\"" + deviceId + "\"}");
      http.POST("{\"value\":\"" + String(humidity, 2) + "\",\"unit\":\"R\",\"type\":\"H\",\"deviceId\":\"" + deviceId + "\"}");

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      /* Free resources */
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}
