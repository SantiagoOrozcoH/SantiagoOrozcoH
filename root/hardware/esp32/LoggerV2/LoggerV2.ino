// ------------------------------------------------------------------------------------------
// ---------------------------------- Logger Specifications ---------------------------------
// ------------------------------------------------------------------------------------------
/* 
  Logger: V2

  Developer: orozcoh

  MCU: ESP32 - C3F

  Time: NTP Server
  
  Sensors:
    - Light: ............. (VEML7700)
      - COMM: I2C 
        - SCL: PIN D9
        - SDA: PIN D8
    - Temperature: ....... (SHT30)
      - COMM: I2C 
        - SCL: PIN D9
        - SDA: PIN D8
    - Air Humidity: ...... (SHT30)
      - COMM: I2C 
        - SCL: PIN D9
        - SDA: PIN D8
    - Soil Humidity: ....  (XXX Analog)
      - COMM: Analog read 
        - PIN: D1
 */
// ------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
#include <esp_task_wdt.h>
#include "Adafruit_VEML7700.h"
#include <Arduino.h>
#include <Wire.h>
#include <ArtronShop_SHT3x.h>
#include <WiFi.h>
#include <HTTPClient.h> 
#include <time.h>
#include <ArduinoJson.h>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60 //60        /* Time ESP32 will go to sleep (in seconds) */

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

const int MEASURE_INTERVAL = 10; //1min
const int WDT_TIMEOUT = MEASURE_INTERVAL + (MEASURE_INTERVAL*1);

Adafruit_VEML7700 veml = Adafruit_VEML7700();

ArtronShop_SHT3x sht3x(0x44, &Wire); // ADDR: 0 => 0x44, ADDR: 1 => 0x45

//const int LIGHTSENSORPIN = 1;
const int SOIL_PIN = 1;

time_t currentTime;

// LIGHT
float light;

// ------------------------------------------------------------------------------------------
// -------------------------------------- Secrets -------------------------------------------
// ------------------------------------------------------------------------------------------
const char* SSID        = "__SSID__";
const char* PASSWORD    = "__PASSWORD__";
const String API_KEY    = "__API_KEY___";
const String DEVICE_ID  = "Logger-Hass";

// local API
const char* LOCAL_API   = "http://192.168.1.2:3000/dataLogger/aguacate";

// Raspberrry API
const char* RASPBERRY_API   = "http://192.168.1.200:3000/dataLogger/aguacate";

// cloud API
const char* CLOUD_API   = "http://api2.orozcoh.com/dataLogger/aguacate";


// ------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------
// -------------------------------------- Setup ---------------------------------------------
// ------------------------------------------------------------------------------------------

void setup() {
  delay(1000);

  //pinMode(LIGHTSENSORPIN, INPUT);
  pinMode(SOIL_PIN, INPUT);

  Serial.begin(115200);

  Serial.println("\n\n--------------------------------------------------------------");
  Serial.println("--------------------------- CONFIG ---------------------------");
  Serial.println("--------------------------------------------------------------");

  Wire.begin();
  while (!sht3x.begin()) {
    Serial.println("SHT3x not found !");
    delay(1000);
  }
  Serial.println("SHT 30 was found !");

  if (!veml.begin()) {
    Serial.println("VEML - Sensor not found");
    while (1);
  }
  Serial.println("VEML - Sensor found");

  veml.setGain(VEML7700_GAIN_1_8) ; // set gain = 1/8
  veml.setIntegrationTime(VEML7700_IT_100MS); // set 25ms exposure time


  WiFi.begin(SSID, PASSWORD);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Conectado a la red con la IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  while (time(nullptr) < 1617460172){ // minimum valid epoch
    Serial.println("Wainting NTP Server");
    delay(3000);
  }

  Serial.println("-------------------------------------------------------------");

  // Enable timer wakeup for 5 minutes  * 60s (300 seconds)
  esp_sleep_enable_timer_wakeup(0.1 * 60 * uS_TO_S_FACTOR);  // Multiply by 1000000 to convert seconds to microseconds

  // Activate the watchdog timer
  //activateWatchdog();
}

// ------------------------------------------------------------------------------------------
// ---------------------------------------- Loop --------------------------------------------
// ------------------------------------------------------------------------------------------

void loop() {
  // ------------------------------------------------------------------------------------------
  // -------------------------------------- Variables -----------------------------------------
  // ------------------------------------------------------------------------------------------
  // LIGHT
  light = -1;
  // SOIL HUMIDITY
  float soil_read;
  float soil_humidity;
  // TEMPERATURE
  float temp;
  // AIR HUMIDITY
  float air_humidity;

  String answer_post;

  DynamicJsonDocument bodyJson(300);
  String bodyString;

  Serial.println("-------------------- GETTING DATA ---------------------------");
  
    if(WiFi.status()== WL_CONNECTED ){ 
      // GET UNIX TIME
      currentTime = getCurrentTime();
      Serial.println("Check currentTime");

      int nowT = currentTime + MEASURE_INTERVAL; //measureInterval
      Serial.println("Check nowT");

      Serial.println(light);
      // GET LIGHT

      while (light == -1){
        light = veml.readALS();
        delay(100);
      }

/*       try {
        light = 777; //veml.readLux();
      } catch (const char* errorMessage) {
        // Catch the exception and handle it
        light = -1;
        Serial.println("Error occurred: ");
        Serial.println(errorMessage);
      } */

      Serial.println(light);
      Serial.println("Check light");
      //GET SOIL HUMIDITY
      soil_read = analogRead(SOIL_PIN);
      soil_humidity = 100 - ((soil_read / 4095) * 100);
      // SHT30 CHECK IF READ
      if (sht3x.measure()) {
        // GET TEMPERATURE
        temp = sht3x.temperature();
        // GET AIR HUMIDITY
        air_humidity = sht3x.humidity();
      } else {
        temp = -1;
        air_humidity = -1;
        Serial.println("SHT3x read error");
      }

      Serial.println("CONNECTED");
      Serial.print("WIFI STATUS: ");
      Serial.println(WiFi.status());
      //answer = getRequest(local_API);
      bodyJson["api_key"] = API_KEY;
      JsonObject data = bodyJson.createNestedObject("data");
      data["device_name"] = DEVICE_ID;
      data["unix_time"] = currentTime;
      data["light"] = String(light, 1);
      data["temp"] = String(temp, 2);
      data["air_humidity"] = String(air_humidity, 2);
      data["soil_humidity"] = String(soil_humidity, 2);

      serializeJson(bodyJson, bodyString);

      Serial.print("DATA:");
      Serial.println(bodyString);

      answer_post = Post(CLOUD_API, bodyString);
      Serial.print("answer:");
      Serial.println(answer_post);
      //count++;
      //bootCount++;
    } else {
      Serial.println("NOT CONNECTED");
      Serial.print("WIFI STATUS: ");
      Serial.println(WiFi.status());
    }
  Serial.println("-------------------------------------------------------------");

  // -------------------------------- SLEEP MODE SETUP --------------------------------------

  delay(5*60*1000);

/*   while (currentTime < nowT){
    delay(1000);
    currentTime = getCurrentTime();
  } */

  // Put the ESP32 into deep sleep mode
  //esp_deep_sleep_start();

  // Put the ESP32 into Light Sleep mode
  //esp_light_sleep_start();

  // Put the ESP32 into modem sleep mode
  //esp_modem_sleep_start();


  // ---------------------------------- RESET WATCHDOG --------------------------------------
  // Reset the watchdog timer
  //resetWatchdog();
}


// ------------------------------------------------------------------------------------------
// -------------------------------------- Functions -----------------------------------------
// ------------------------------------------------------------------------------------------

// ------------------------------------ getCurrentTime --------------------------------------
time_t getCurrentTime() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec;
}
// -------------------------------------- getRequest ---------------------------------------
String getRequest(const char* serverName) {
  HTTPClient http;    
  http.begin(serverName);
  
  // Enviamos peticion HTTP
  int httpResponseCode = http.GET();
  
  String payload = "..."; 
  
  if (httpResponseCode > 0) {
    Serial.print("\nHTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // liberamos
  http.end();

  return payload;
}
// -------------------------------------- Post -------------------------------------------
int Post(const char* serverName, String _body){
  HTTPClient http; 
  http.begin(serverName);
  
  // If you need an HTTP request with a content type: application/json, use the following:
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(_body);
 
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
    
  // Free resources
  http.end();

  return httpResponseCode;
}
// ---------------------------------- Activate Watchdog -----------------------------------

void activateWatchdog() {
  esp_task_wdt_init(WDT_TIMEOUT, true);  // Initialize the watchdog timer
  esp_task_wdt_add(NULL);  // Add the current task to the watchdog
}
// ---------------------------------- Reset Watchdog --------------------------------------
void resetWatchdog() {
  esp_task_wdt_reset();  // Reset the watchdog timer
}
