#include <WiFi.h>
#include "config.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <DHTesp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp32-hal-cpu.h"

/* Declare Environment Variables */
DHTesp dht;

float temperature;
float humidity;

#define MS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 1800 /* 30 Min */

uint8_t espMacAddress[12];
char deviceId[20];

StaticJsonDocument<200> requestBody;

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

  @return bool
    true when WiFi is connected
    false when unable to connect to WiFi
*/
bool connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(wifiSSID);

  /* Settle Voltage Before WiFi Pull */
  delay(5000);
  
  WiFi.begin(wifiSSID, wifiPassword);

  /* Attempt to Connect to WiFi */
  int reattemptCount = 0;
  do {
    delay(500);
    Serial.print(".");

    /* Increment Count */
    reattemptCount += 1;
  } while (reattemptCount < 10 && WiFi.status() != WL_CONNECTED);

  
  /* Return */
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  else {
    Serial.println("Failed to connect to WiFi.");
    return false;
  }
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
  int dhtPin = 14;

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
bool readSensorData() {

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
  Serial.println("Sensor values set");
  Serial.println(lastValues.temperature);
  Serial.println(lastValues.humidity);

  return true;
}


/*
  Log Sensor Reading
  POSTs sensor data to cloud database.
*/
void logSensorReading(String reading) {

  /* Check WiFi is Connected */
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      String jsonStr;
      int httpResponseCode;

      /* Open HTTP Session */
      if (reading == "temperature") {
        http.begin(databaseTemperature);
        requestBody["value"] = temperature;
      }
      else {
        http.begin(databaseHumidity);
        requestBody["value"] = humidity;
      }
      

      /* Build Request */
      http.addHeader("Content-Type", "application/json");
      requestBody["device_mac_address"] = deviceId;
      serializeJson(requestBody, jsonStr);
      serializeJson(requestBody, Serial);

      httpResponseCode = http.POST(jsonStr);
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      /* Free resources */
      requestBody.clear();
      httpResponseCode = 0;
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
}


void setup() {
  /* Lower CPU Clock to 80 Mhz */
  setCpuFrequencyMhz(80);
  
  /* Stream to 115200 */
  Serial.begin(115200);

  bool isWiFiSuccess = false;

  /* Print Wakeup Reason */
  printWakeupReason();

  /* Finish Setup */
  setupDHTSensor();
  assignDeviceId();

  /* Measure & Log Temperature and Humidity */
  int reattemptCount = 0;
  bool isReadingSuccess = false;
  do {
    /* Wait for Sensor */
    delay(1000);
    
    /* Read Sensor Data */
    isReadingSuccess = readSensorData();

    /* Increment Count */
    reattemptCount += 1;
  } while (reattemptCount < 3 && !isReadingSuccess);

  /* Log Data If Sensor Read Success */
  if (isReadingSuccess) {
    /* Attempt to connect to WiFi after a successful read */
    uint32_t brown_reg_temp = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG); //save WatchDog register
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
    isWiFiSuccess = connectToWiFi();
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, brown_reg_temp); //enable brownout detector
    if (isWiFiSuccess) {
      logSensorReading("temperature");
      logSensorReading("humidity");
    }
    else {
      Serial.println("WiFi not established after sensor read. Initiate sleep without logging data.");
    }
  }
  else {
    Serial.println("Data could not be read from sensor. Initiate sleep without logging data.");
  }

  /* Enter Deep Sleep */
  /* Configure Wakeup Timer */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * MS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep now");
  delay(500);
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {
  /* Loop Not Executed in Deep Sleep */
}
