#include <Arduino.h>


#include <ESP8266WiFi.h>
#include <ClosedCube_HDC1080.h>
#include <Adafruit_SI1145.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "PCF8583.h"
#include <Adafruit_INA219.h>
//#include "Adafruit_VEML6070.h"
#include "VEML6075.h"
// includes necessaires au fonctionnement de l'OTA :
//#include <WiFiUdp.h>
//#include <ArduinoOTA.h>


#include "passwords.h"

// I2C Setup    SCL: D1/GPIO5    SDA: D2/GPIO4  /////////////////////////////////////////////
PCF8583 rtc(0xA0);   // counter for rain gauge tipping bucket   address 0xA0
// PCF8583 memory mapping:
// @ 0x10  =  humidity status:  2: >=95%    1: between 5 & 95%    0:below 5%
// @ 0x11  = Invalid Temp counter, if heater was used, as long as counter > 0, do not use sensor T value.
// @ 0x12 / 13 / 14 / 15  : store last good temp value.

// INA219 current and voltage sensors: to monitor solar panel & battery
Adafruit_INA219 ina219_solar(0x44);   // I2C address 0x44   !default is 0x40, conflict with hdc1080
Adafruit_INA219 ina219_battery(0x45); // I2C address 0x45 !default is 0x40, conflict with hdc1080
float solar_voltage = 0;
float solar_current = 0;  // to check if battery is charging well.
float battery_voltage = 0;
// float battery_current = 0 // not really interesting because discharge current depends when we measure it (during wifi etc..)

VEML6075 veml6075 = VEML6075();

ClosedCube_HDC1080 hdc1080;   // default address is 0x40

//Adafruit_SI1145 uv = Adafruit_SI1145();  // default address is 0x60
//Adafruit_VEML6070 uv = Adafruit_VEML6070();

// WiFi connexion informations //////////////////////////////////////////////////////////////

IPAddress ip(192, 168, 1, 191);
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network

// MQTT server informations /////////////////////////////////////////////////////////////////

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish rain_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/rain", 1);
Adafruit_MQTT_Publish outTemp_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/outTemp", 1);
Adafruit_MQTT_Publish outHumidity_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/outHumidity", 1);
Adafruit_MQTT_Publish UV_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/UV", 1);
Adafruit_MQTT_Publish Vsolar_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/Vsolar", 1);
Adafruit_MQTT_Publish Isolar_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/Isolar", 1);
Adafruit_MQTT_Publish Vbat_pub = Adafruit_MQTT_Publish(&mqtt, "weewx/Vbat", 1);




// other variables ////////////////////////////////////////////////////////////////////////
float rain = 0;
float temp;
float humi;
union {
   float f;
   uint8_t b[4];
} last_temp;

float UVindex = -1;
float UVA = -1;
float UVB = -1;


const int sleep_duration = 150;  // deep sleep duration in seconds
const uint8_t invalid_temp_counter_init = 6; // should be 15min / sleep duration : used to disable Temp measurement after heater use.  Temp will be disable n * sleep duration, 15 min is recommanded
uint8_t invalid_temp_counter;

// uint8_t rtc_mode;

uint8_t test;

float value;




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup_wifi() : connexion to wifi hotspot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Connexion au réseau WiFi
void setup_wifi() {
   //Serial.println();
   //Serial.print("Connexion a ");
   //Serial.println(ssid);
   //config static IP
   WiFi.mode(WIFI_STA);
   WiFi.config(ip, gateway, subnet);
   if (WiFi.status() == WL_CONNECTED) {
      return;
   }

   WiFi.begin(ssid, password);

   uint8_t timeout_wifi = 30;
   while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      //Serial.print("["); Serial.print(WiFi.status()); Serial.print("]");
      timeout_wifi--;
      if (timeout_wifi == 0) {
         //Serial.println("connexion timeout!"); delay(100);
         ESP.deepSleep(sleep_duration * 1000000);  // deepsleep for 60sec, after will restart (= reset)
      }
   }

   if (WiFi.status() == WL_CONNECTED) {
      return;
   }

   WiFi.begin(ssid2, password);

   timeout_wifi = 30;
   while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      //Serial.print("["); Serial.print(WiFi.status()); Serial.print("]");
      timeout_wifi--;
      if (timeout_wifi == 0) {
         //Serial.println("connexion timeout!"); delay(100);
         ESP.deepSleep(sleep_duration * 1000000);  // deepsleep for 60sec, after will restart (= reset)
      }
   }
  // ArduinoOTA.setHostname("ESP_THrain");
  // ArduinoOTA.begin();
  // ArduinoOTA.handle();
   //Serial.println("");
   //Serial.println("Connexion WiFi etablie ");
   //Serial.print("=> Addresse IP : ");
   //Serial.println(WiFi.localIP());
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
   //Serial.print("Connecting to MQTT... ");
   uint8_t retries = 3;
   while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
      //Serial.println(mqtt.connectErrorString(ret));
      //Serial.println("Retrying MQTT connection in 1 seconds...");
      mqtt.disconnect();
      delay(1000);  // wait 1 seconds
      retries--;
      if (retries == 0) {
         ESP.deepSleep(sleep_duration * 1000000);  // deepsleep for 20sec, after will restart (= reset)
      }
   }
   //Serial.println("MQTT Connected!");
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Start of main programm !
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void setup() {
   Serial.begin(115200);
   //Serial.println(' ');
   //Serial.println("Boot");

   // check if POR occured ( @POR, register 0x00 of PCF8583 is set to 0 )
   if ( rtc.getRegister(0) == 0 ) {
      // POR occured, let's clear the SRAM of PCF8583
      for (uint8_t i = 0x10; i < 0x20; i++) { // PCF8583 SRAM start @0x10 to 0xFF, let's clear a few bytes only.
         rtc.setRegister(i, 0);
      }
      rtc.setMode(MODE_EVENT_COUNTER);
      rtc.setCount(0);
      //Serial.println("Power ON RESET !");
   }

   ina219_solar.begin();
   ina219_battery.begin();
   hdc1080.begin(0x40);

   /*if (! uv.begin()) {
      //Serial.println("Didn't find Si1145");
   }

   else {
      delay(5);
      UVindex = uv.readUV();
      UVindex /= 100.0;
      //Serial.print("UV: ");  Serial.println(UVindex);
      //   Serial.print("Vis: "); Serial.println(uv.readVisible());
      //   Serial.print("IR: "); Serial.println(uv.readIR());
   }*/
   //if (veml6075.begin()) {
  //   UVindex = veml6075.getUVIndex();
  //   }


   // read sensors
   rain = 0.2 * float(rtc.getCount());
   rtc.setCount(0);
   temp = hdc1080.readTemperature();
   humi = hdc1080.readHumidity();
   solar_voltage = ina219_solar.getBusVoltage_V();
   solar_current = ina219_solar.getCurrent_mA();;
   battery_voltage = ina219_battery.getBusVoltage_V();;

   // read last temp stored in SRAM
   last_temp.b[0] = rtc.getRegister(0x12);
   last_temp.b[1] = rtc.getRegister(0x13);
   last_temp.b[2] = rtc.getRegister(0x14);
   last_temp.b[3] = rtc.getRegister(0x15);

   if (humi >= 95) {
      rtc.setRegister(0x10, 2);
   }
   else if (humi > 5 & humi < 95) {
      rtc.setRegister(0x10, 1);
   }
   else  {  //(humi <= 0)
      // let's check if previous value was higher than 95%
      if (rtc.getRegister(0x10) == 2) {
         // condensation occured, let's try the heater
         //    Serial.println("heater launch!");
         while (hdc1080.readHumidity() <= 5) {
            hdc1080.heatUp(10);
         }
         //    Serial.println("heater end");
         rtc.setRegister(0x10, 0);
         rtc.setRegister(0x11, invalid_temp_counter_init ); // set counter for invalid T measurement
      }
   }
   invalid_temp_counter = rtc.getRegister(0x11);
   if (invalid_temp_counter > 0) {
      temp = last_temp.f;
      rtc.setRegister(0x11, invalid_temp_counter - 1 );
   }
   else {
      //  Serial.println("we store temp as last_temp");
      last_temp.f = temp;
      rtc.setRegister(0x12, last_temp.b[0]);
      rtc.setRegister(0x13, last_temp.b[1]);
      rtc.setRegister(0x14, last_temp.b[2]);
      rtc.setRegister(0x15, last_temp.b[3]);
   }



     if (!veml6075.begin()) {
         Serial.println(F("VEML6075 not found!"));
       } else {
         Serial.println(F("VEML6075 found!"));;
       }
      // veml6075.begin();
      veml6075.sleep(false); // leave before wifi connection, and measure after wifi connexion  => Use wifi delay to allow restart of UV sensor after sleep
      //delay(10);
      //veml6075.poll();
  //    delay(1200);

   setup_wifi();
   setup_mqtt();
   delay(10);
   veml6075.poll();
//   UVA = veml6075.getUVA();
//   UVB = veml6075.getUVB();
   UVindex = 0.8 * veml6075.getUVIndex();
   veml6075.sleep(true);  // UV sensor set to sleep


   if (rain > 0 ) {
      rain_pub.publish(rain);
      delay(5);
   }
   if ( humi >= 0 && humi <= 100 ) {
      outHumidity_pub.publish(humi);
      setup_wifi();
      setup_mqtt();
      delay(10);
   }
   if ((temp > -40) && (temp < 80)) {
      outTemp_pub.publish(temp);
      setup_wifi();
      setup_mqtt();
      delay(10);
   }
   if ((UVindex > 0) && (UVindex < 25)) {
      UV_pub.publish(UVindex);
      setup_wifi();
      setup_mqtt();
      delay(10);
   }
   if ((solar_voltage >= 0) && (solar_voltage < 20.0)) {
      Vsolar_pub.publish(solar_voltage);
      setup_wifi();
      setup_mqtt();
      delay(10);
   }
   if ((solar_current >= 0) && (solar_current < 1000.0)) {
      Isolar_pub.publish(solar_current);
      setup_wifi();
      setup_mqtt();
      delay(10);
   }
   if ((battery_voltage >= 0) && (battery_voltage < 20.0) ) {
      Vbat_pub.publish(battery_voltage);
   }

   mqtt.disconnect();
   delay(50);
   WiFi.disconnect();
   //Serial.println("Sleep");
   delay(50);
   ESP.deepSleep(sleep_duration * 1000000);
}

void loop() {
   // not used due to deepsleep
}
