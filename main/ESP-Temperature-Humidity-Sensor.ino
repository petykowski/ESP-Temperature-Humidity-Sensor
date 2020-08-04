#include <WiFi.h>
#include "config.h"
#include <DHTesp.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Congifure Web Server for Port 80
WebServer server(80);

/** Initialize DHT sensor */
DHTesp dht;
/** Pin number for DHT11 data pin */
int dhtPin = 4;

// JSON data buffer
StaticJsonDocument<250> jsonDocument;
char buffer[250];

// env variable
float temperature;
float humidity;
 
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

void setup_routing() {
  server.on("/getCurrentTemperature", getCurrentTemperature);
  server.on("/getCurrentRelativeHumidity", getCurrentRelativeHumidity);

  // start server
  server.begin();
}

void create_json(float value) {
  jsonDocument.clear();
  jsonDocument["response"] = value;
  serializeJson(jsonDocument, buffer);
}

void read_sensor_data(void * parameter) {
  TempAndHumidity lastValues = dht.getTempAndHumidity();  
  temperature = lastValues.temperature;
  humidity = lastValues.humidity;
  Serial.println("Read sensor data");
}

void getCurrentTemperature() {
  Serial.println("Get temperature");
  read_sensor_data(NULL);
  create_json(temperature);
  server.send(200, "application/json", buffer);
}

void getCurrentRelativeHumidity() {
  Serial.println("Get humidity");
  read_sensor_data(NULL);
  create_json(humidity);
  server.send(200, "application/json", buffer);
}

void handlePost() {
  if (server.hasArg("plain") == false) {
    //handle error here
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  // Respond to the client
  server.send(200, "application/json", "{}");
}
//
//void setup_task() {
//  xTaskCreate(
//  read_sensor_data,
//  "Read sensor data",
//  1000,
//  NULL,
//  1,
//  NULL
//  );
//}

void setup() {
  Serial.begin(115200);

  // Initialize temperature sensor
  dht.setup(dhtPin, DHTesp::DHT22);

  connectToWiFi();
//  setup_task();
  setup_routing();
}

void loop() {
  server.handleClient();
}
