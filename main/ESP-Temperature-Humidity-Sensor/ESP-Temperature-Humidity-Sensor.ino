#include <WiFi.h>
#include "config.h"
#include <DHTesp.h>
#include <HTTPClient.h>

/* Declare Environment Variables */
DHTesp dht;

float temperature;
float humidity;

#define MS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 600

uint8_t espMacAddress[12];
char deviceId[20];


/*
  Print Wakeup Reason
  Prints a message for the ESP32 wakeup reason.
*/
void printWakeupReason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


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
  esp_efuse_mac_get_default(espMacAddress);

  /* Format as Human Readable */
  for(int m = 0; m<6; m++)
    sprintf(deviceId, "%02x:%02x:%02x:%02x:%02x:%02x", espMacAddress[0],espMacAddress[1],espMacAddress[2],espMacAddress[3], espMacAddress[4],espMacAddress[5]);
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

  @return bool
    true when temperature and values provided
    false when sensor data could not be read
*/
bool readSensorData(void * parameter) {

  /* Query for Sensor Data */
  Serial.println("Reading sensor data");
  TempAndHumidity lastValues = dht.getTempAndHumidity();

  /* Check If Sensor Read Was Successful */
  if (dht.getStatus() != 0) {
    Serial.println("DHT error status: " + String(dht.getStatusString()));
    return false;
  }

  /* Store Values */
  temperature = lastValues.temperature;
  humidity = lastValues.humidity;
  Serial.println("Sensor values updated");

  return true;
}


/*
  Log Sensor Reading
  POSTs sensor data to cloud database.
*/
void logSensorReading() {

  /* Check WiFi is Connected */
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

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
}


void setup() {
  /* Stream to 115200 */
  Serial.begin(115200);

  /* Finish Setup */
  setupDHTSensor();
  connectToWiFi();
  assignDeviceId();

  /* Print Wakeup Reason */
  printWakeupReason();

  /* Configure Wakeup Timer */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * MS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  /* Measure & Log Temperature and Humidity */
  int reattemptCount = 0;
  bool isReadingSuccess = false;
  do {
    /* Wait for Sensor */
    delay(1000);
    
    /* Read Sensor Data */
    isReadingSuccess = readSensorData(NULL);

    /* Increment Count */
    reattemptCount += 1;
  } while (reattemptCount < 3 && !isReadingSuccess);

  /* Log Data If Sensor Read Success */
  if (isReadingSuccess) {
    logSensorReading();
  }
  else {
    Serial.println("Data could not be read from sensor. Initiate sleep without logging data.");
  }

  /* Enter Deep Sleep */
  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush(); 
  esp_deep_sleep_start();
}

void loop() {
  /* Loop Not Executed in Deep Sleep */
}
