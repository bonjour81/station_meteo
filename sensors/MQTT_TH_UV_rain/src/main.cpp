//V1.1
//History:
// added OTA
// added AM2315


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PCF8583.h>
#include <Adafruit_INA219.h>
//#include "VEML6075.h"
#include <SparkFun_VEML6075_Arduino_Library.h>
#include <ClosedCube_HDC1080.h>
#include <Adafruit_AM2315.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ArduinoOTA.h>

// wifi & mqtt credentials
#include "passwords.h"

// I2C Setup    SCL: D1/GPIO5    SDA: D2/GPIO4  /////////////////////////////////////////////
// setup of sensors
// counter for rain gauge tipping bucket  I2C address 0xA0
PCF8583 rtc(0xA0);
// INA219 current and voltage sensors: to monitor solar panel & battery
Adafruit_INA219 ina219_solar(0x44);   // I2C address 0x44   !default is 0x40, conflict with hdc1080
Adafruit_INA219 ina219_battery(0x45); // I2C address 0x45   !default is 0x40, conflict with hdc1080
// UV sensor
//VEML6075 veml6075 = VEML6075();
VEML6075 uv; // sparkfun lib sensor declaration
// Temp and humidity sensor.
ClosedCube_HDC1080 hdc1080;  // default address is 0x40
Adafruit_AM2315 am2315;    // default address is 0x05C (!cannot be changed)

// WiFi connexion informations //////////////////////////////////////////////////////////////
IPAddress ip(192, 168, 1, 191);      // hard coded IP address (make the wifi connexion faster (save battery), no need for DHCP)
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0);  // set subnet mask to match your network

// MQTT server informations /////////////////////////////////////////////////////////////////
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish rain_pub = Adafruit_MQTT_Publish(&mqtt, "tweewx/rain", 0);  // QoS 0 because QoS 1 may send several time rain (could be counted several times by weewx
Adafruit_MQTT_Publish outTemp_pub = Adafruit_MQTT_Publish(&mqtt, "tweewx/outTemp", 1);
Adafruit_MQTT_Publish outHumidity_pub = Adafruit_MQTT_Publish(&mqtt, "tweewx/outHumidity", 1);
Adafruit_MQTT_Publish UV_pub = Adafruit_MQTT_Publish(&mqtt, "tweewx/UV", 1);
Adafruit_MQTT_Publish Vsolar_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/Vsolar", 1);
Adafruit_MQTT_Publish Isolar_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/Isolar", 1);
Adafruit_MQTT_Publish Vbat_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/Vbat", 1);
Adafruit_MQTT_Publish Status = Adafruit_MQTT_Publish(&mqtt, "THrain/Status", 1);

Adafruit_MQTT_Subscribe OTA_flag = Adafruit_MQTT_Subscribe(&mqtt, "THrain/OTA");// flag to enable OTA (wait for OTA before go to sleep), use retained messages on publisher!
Adafruit_MQTT_Subscribe user_sleep_time = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/THrain/Sleep_time");// set different time to wait before next wakeup (nice to speed up, for debug), use retained messages on publisher!


// variables ////////////////////////////////////////////////////////////////////////
float rain = -1;
float temp = -100;
float humi = -1;
float UVindex = -1;
float UVA = -1;
float UVB = -1;
float solar_voltage = -1;
float solar_current = -1;  // to check if battery is charging well.
float battery_voltage = -1;
int sleep_duration = 30;  // deep sleep duration in seconds


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup_wifi() : connexion to wifi hotspot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Connexion au réseau WiFi
void setup_wifi() {
   //config static IP
   WiFi.mode(WIFI_STA);
   WiFi.config(ip, gateway, subnet);
   if (WiFi.status() == WL_CONNECTED) {
      return;
   }
// try 1st SSID (if you have 2 hotspots)
   WiFi.begin(ssid, password);
   uint8_t timeout_wifi = 30;
   while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0)) {
      delay(1000);
      timeout_wifi--;
   }
// if connexion is successful, let's go to next, no need for SSID2
   if (WiFi.status() == WL_CONNECTED) {
      return;
   }
// let's try SSID2 (if ssid1 did not worked)
   WiFi.begin(ssid2, password);
   timeout_wifi = 30;
   while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0)) {
      delay(1000);
      timeout_wifi--;
      }

   if (WiFi.status() == WL_CONNECTED) {
     return;     // if connexion is successful, let's go to next, no need for SSID2
   }
   else {
     ESP.deepSleep(sleep_duration * 1000000);  // if no connexion, deepsleep some time, after will restart & retry (= reset)
   }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  setup_mqtt() : connexion to mosquitto server
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup_mqtt() {
   int8_t ret;
   // Stop if already connected.
   if (mqtt.connected()) {
      return;
   }
   mqtt.subscribe(&OTA_flag);
   mqtt.subscribe(&user_sleep_time);

   //Serial.print("Connecting to MQTT... ");
   uint8_t retries = 3;
   while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
      //Serial.println(mqtt.connectErrorString(ret));
      //Serial.println("Retrying MQTT connection in 1 seconds...");
      mqtt.disconnect();
      delay(1000);  // wait 1 seconds
      retries--;
      if (retries == 0) {
         ESP.deepSleep(sleep_duration * 1000000);  // if no connexion, deepsleep for 20sec, after will restart (= reset)
      }
   }
   Status.publish("Go!");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Start of main programm !
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
   Serial.begin(115200);
   //Serial.println(' ');
   // check if POR occured ( @POR, register 0x00 of PCF8583 is set to 0 )
   if ( rtc.getRegister(0) == 0 ) {
      // POR occured, let's clear the SRAM of PCF8583
      for (uint8_t i = 0x10; i < 0x20; i++) { // PCF8583 SRAM start @0x10 to 0xFF, let's clear a few bytes only.
         rtc.setRegister(i, 0);
      }
      rtc.setMode(MODE_EVENT_COUNTER);  // will set non zero value in register 0x00, so if no POR occured at next loop, register will not be cleared
      rtc.setCount(0);  // reset rain counter
   }
   // read sensors
   ina219_solar.begin();
   ina219_battery.begin();
   hdc1080.begin(0x40);
   if(hdc1080.readManufacturerId() == 0x5449) {  // as "begin" does not provide boolean answer, check id to confirm HDC1080 is connected
     temp = hdc1080.readTemperature();
     humi = hdc1080.readHumidity();
   };
   if (am2315.begin()) {
     temp = am2315.readTemperature();
     humi = am2315.readHumidity();
   }
   rain = 0.2 * float(rtc.getCount());
   solar_voltage = ina219_solar.getBusVoltage_V();
   solar_current = ina219_solar.getCurrent_mA();
   battery_voltage = ina219_battery.getBusVoltage_V();

   if (uv.begin()) {  // power ON veml6075 early to let it wakeup
     uv.powerOn();
   }

// measurements done, time to send them all !
   if (rain > 0 && rain <500 ) {
      setup_wifi();
      setup_mqtt();
      if (rain_pub.publish(rain)) {
        rtc.setCount(0);  //  reset rain counter only if it was able to send the date to the mqtt broker
      };
      delay(15);
   }
   if ( humi >= 0 && humi <= 100 ) {
      setup_wifi();
      setup_mqtt();
      outHumidity_pub.publish(humi);
      delay(15);
   }
   if ((temp > -40) && (temp < 80)) {
      setup_wifi();
      setup_mqtt();
      outTemp_pub.publish(temp);
      delay(15);
   }
   if ((solar_voltage >= 0) && (solar_voltage < 20.0)) {
      setup_wifi();
      setup_mqtt();
      Vsolar_pub.publish(solar_voltage);
      delay(15);
   }
   if ((solar_current >= 0) && (solar_current < 1000.0)) {
      setup_wifi();
      setup_mqtt();
      Isolar_pub.publish(solar_current);
      delay(15);
   }
   if ((battery_voltage >= 0) && (battery_voltage < 20.0) ) {
      setup_wifi();
      setup_mqtt();
      Vbat_pub.publish(battery_voltage);
      delay(15);
   }
   // measure UV index and publish
   if (uv.begin()) {
     UVindex = uv.index();
     uv.shutdown();
   }
   if ((UVindex > 0) && (UVindex < 25)) {
       setup_wifi();
       setup_mqtt();
       UV_pub.publish(UVindex);
   }
  // add subscription handling (OTA and user_sleep_time)
  Status.publish("Sensors published!");



  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
       Status.publish("check sub");
    if (subscription == &OTA_flag) {
       Status.publish("OTA flag detected");
      if (strcmp((char *)OTA_flag.lastread, "ON") == 0) {
       Status.publish("OTA flag ON");
      }
    }
  }

  // if (subscription == &user_sleep_time) {
  //     sleep_duration = atoi((char *)user_sleep_time.lastread); // may be used for debug to shorten sleep.
  // }

//////////////////////////////////// OTA management
  setup_wifi();
  Status.publish("OK");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("ESP_THrain"); // on donne une petit nom a notre module
  ArduinoOTA.onStart([]() {
     Status.publish("Start updating");// + type);
     String type;
     if (ArduinoOTA.getCommand() == U_FLASH)
       type = "sketch";
     else // U_SPIFFS
       type = "filesystem";
         // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
  });
  ArduinoOTA.onEnd([]() {
     Status.publish("End");// + type);
     //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
     Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Status.publish("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Status.publish("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Status.publish("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Status.publish("Receive Failed");
    else if (error == OTA_END_ERROR) Status.publish("End Failed");
  });
  mqtt.disconnect();
  ArduinoOTA.begin(); // initialisation de l'OTA
  uint8_t timeout_OTA = 30;
  while (timeout_OTA > 0) {
     ArduinoOTA.handle();
     delay(100);
     timeout_OTA--;
  }

//////////////////////////////////// end of OTA management

// job is done, let's disconnect


   delay(50);
   WiFi.disconnect();
   //Serial.println("Sleep");
   delay(50);
   ESP.deepSleep(sleep_duration * 1000000);
}

void loop() {
   // not used due to deepsleep
}
