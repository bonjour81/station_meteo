//V1.28
//History:
// added average (on 2 samples) of UV, T & H measurement.
//V1.27
//History:
// switched to AM2315 for temp & humidity sensor
//V1.26
//History:
// added OTA via http server
// added debug mode for serial monitoring


//#define DEBUGMODE   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef  DEBUGMODE    //Macros are usually in all capital letters.
  #define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DPRINT(...)     //now defines a blank line
  #define DPRINTLN(...)   //now defines a blank line
#endif



const int FW_VERSION = 128;
const char* fwImageURL = "http://192.168.1.180/fota/THrain/firmware.bin"; // update with your link to the new firmware bin file.
const char* fwVersionURL = "http://192.168.1.180/fota/THrain/firmware.version"; // update with your link to a text file with new version (just a single line with a number)
// version is used to do OTA only one time, even if you let the firmware file available on the server.
// flashing will occur only if a greater number is available in the "firmware.version" text file.
// take care the number in text file is compared to "FW_VERSION" in the code => this const shall be incremented at each update.

#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PCF8583.h>
#include <Adafruit_INA219.h>
#include <SparkFun_VEML6075_Arduino_Library.h>
//#include <ClosedCube_HDC1080.h>
#include <Adafruit_AM2315.h>   // => Modified library from https://github.com/switchdoclabs/SDL_ESP8266_AM2315   I have switched pin 4&5 in cpp file wire.begin
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


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
//ClosedCube_HDC1080 hdc1080;  // default address is 0x40
Adafruit_AM2315 am2315;    // default address is 0x05C (!cannot be changed)

// WiFi connexion informations //////////////////////////////////////////////////////////////
IPAddress ip(192, 168, 1, 191);      // hard coded IP address (make the wifi connexion faster (save battery), no need for DHCP)
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0);  // set subnet mask to match your network

// MQTT server informations /////////////////////////////////////////////////////////////////
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, "ESP_THrain", AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish rain_pub        = Adafruit_MQTT_Publish(&mqtt, "weewx/rain", 0);  // QoS 0 because QoS 1 may send several time rain (could be counted several times by weewx
Adafruit_MQTT_Publish outTemp_pub     = Adafruit_MQTT_Publish(&mqtt, "weewx/outTemp", 1);
Adafruit_MQTT_Publish outHumidity_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/outHumidity", 1);
Adafruit_MQTT_Publish UV_pub          = Adafruit_MQTT_Publish(&mqtt, "weewx/UV", 1);
Adafruit_MQTT_Publish Vsolar_pub      = Adafruit_MQTT_Publish(&mqtt, "weewx/Vsolar", 1);
Adafruit_MQTT_Publish Isolar_pub      = Adafruit_MQTT_Publish(&mqtt, "weewx/Isolar", 1);
Adafruit_MQTT_Publish Vbat_pub        = Adafruit_MQTT_Publish(&mqtt, "weewx/Vbat", 1);

Adafruit_MQTT_Publish Status_pub      = Adafruit_MQTT_Publish(&mqtt, "THrain/Status", 1);
Adafruit_MQTT_Publish Version_pub     = Adafruit_MQTT_Publish(&mqtt, "THrain/Version", 1);
Adafruit_MQTT_Publish Debug_pub       = Adafruit_MQTT_Publish(&mqtt, "THrain/Debug", 1);


// variables ////////////////////////////////////////////////////////////////////////
float rain = -1;
float temp = -100;
float humi = -1;
float temp_buffer = -100;
float humi_buffer = -1;
float UVindex = -1;
float UVA = -1;
float UVB = -1;
float solar_voltage = -1;
float solar_current = -1;  // to check if battery is charging well.
float battery_voltage = -1;

// set faster emission in debug mode (take care battery will empty faster!)
#ifdef DEBUGMODE
  int sleep_duration = 5;  // deep sleep duration in seconds
#else
  int sleep_duration = 150;  // deep sleep duration in seconds
#endif

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
      DPRINTLN(" ");

      DPRINTLN("connected");
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
   Status_pub.publish("Online!");
   delay(5);
   Version_pub.publish(FW_VERSION);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  check_OTA() : check for some available new firmware on server?
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void check_OTA() {
// setup_wifi(); must be called before check_OTA();

DPRINT("Firmware:<");DPRINT(FW_VERSION);DPRINTLN(">");
DPRINTLN("Check for OTA");

HTTPClient http;
if (http.begin(client, fwVersionURL)) {
    DPRINTLN("http begin");
    int httpCode = http.GET();
    DPRINT("httpCode:");DPRINTLN(httpCode);
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String newFWVersion = http.getString();
          DPRINT("newFWVersion: ");DPRINTLN(newFWVersion);
          int newVersion = newFWVersion.toInt();
          if( newVersion > FW_VERSION ) {
            DPRINTLN("start OTA !");
            delay(100);
            // place OTA command
            ESPhttpUpdate.update( fwImageURL );
          } else {
            DPRINTLN("no new version available");
          }
    }
  } // end if http.begin
} // end check_OTA()


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Start of main programm !
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
   #ifdef DEBUGMODE
   Serial.begin(115200);
   #endif
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
   /*hdc1080.begin(0x40);
   if(hdc1080.readManufacturerId() == 0x5449) {  // as "begin" does not provide boolean answer, check id to confirm HDC1080 is connected
     temp = hdc1080.readTemperature();
     humi = hdc1080.readHumidity();
   };*/
   if (am2315.begin()) {

     if (am2315.readTemperatureAndHumidity(temp_buffer,humi_buffer)) {
       temp = temp_buffer;  // if function return false, make sure the value are considered wrong and not emitted
       humi = humi_buffer;
     } else {
       temp = -100;  // if function return false, make sure the value are considered wrong and not emitted
       humi = -1;
     }
     DPRINTLN("am2315 detected");
     DPRINT("T:");DPRINTLN(temp);
     DPRINT("H:");DPRINTLN(humi);
   }
   rain = 0.2 * float(rtc.getCount());
   DPRINT("rain :"); DPRINTLN(rain);
   solar_voltage = ina219_solar.getBusVoltage_V();    DPRINT("solar_voltage:"); DPRINTLN(solar_voltage);
   solar_current = ina219_solar.getCurrent_mA();      DPRINT("solar_current:"); DPRINTLN(solar_current);
   battery_voltage = ina219_battery.getBusVoltage_V();DPRINT("battery_voltage:"); DPRINTLN(battery_voltage);

   if (uv.begin()) {  // power ON veml6075 early to let it wakeup
     uv.powerOn();
   }

  setup_wifi();
  check_OTA();  // check for new firmware available on server, if check OTA with occur
  // OTA must be checked before connecting to MQTT (or after disconnecting MQTT)
  // measurements done, time to send them all !


  // I wise to average T & H on a few samples, but am2315 does not allow fast reading (min 2sec recommanded according to adafruit)
  // I use the wifi connexion as a delay (a few seconds)
  if (am2315.readTemperatureAndHumidity(temp_buffer,humi_buffer)) {
    temp = (temp + temp_buffer) / 2;  // if function return false, make sure the value are considered wrong and not emitted
    humi = (humi + humi_buffer) / 2;
  } // no else here, if we were lucky at 1st measurement, we have a least a value, if not, let's forget T or H this time.

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
     delay(2);
     UVindex = UVindex + uv.index();
     UVindex = UVindex + uv.index();
     UVindex = UVindex / 3;
     uv.shutdown();
   }
   if ((UVindex > 0) && (UVindex < 25)) {
       setup_wifi();
       setup_mqtt();
       UV_pub.publish(UVindex);
   }

  // job is done, let's disconnect
   DPRINTLN("End of measurements & MQTT publish");
   Status_pub.publish("Offline!");
   delay(15);
   mqtt.disconnect();
   delay(15);
   WiFi.disconnect();
   //Serial.println("Sleep");
   DPRINTLN("Go to sleep");
   delay(15);
   ESP.deepSleep(sleep_duration * 1000000);
}

void loop() {
   // not used due to deepsleep
}
