const float FW_VERSION = 1.66;
//V1.66 : retry pubsubclient 2.8
//V1.65 : back to pubsub 2.7
//V1.64 : try pub sub client v2.8 instead of 2.7 => not compiling
//V1.63 : minor corrections
//V1.60 : replacement of wemos mini pro, new IP address
//V1.59 : some IP address update
//V1.58 : some IP address update
//V1.57 : bug correction
//V1.52 : change parameters /timings in case of wifi connexion failure to save battery.
//V1.50 : switched to SHT31 TH sensor, change INA219 address.
//V1.49 bug correction
//V1.48 add filter on T&H measurement: take 3 samples, keep 2 best ones (filter sporadic spikes)
//V1.30 - V1.46 debug, change  to pubsubclient lib instead of adafruit mqtt lib
//  added subscription to receive config message (not used yet)
//V1.29
//History:
// change Isolar unit in Amp instead of milliamp to match weewx default unit for current.
//v1.28
// added average (on 2 samples) of UV, T & H measurement.
//V1.27
//History:
// switched to AM2315 for temp & humidity sensor
//V1.26
//History:
// added OTA via http server
// added debug mode for serial monitoring

//#define DEBUGMODE								  //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUGMODE //Macros are usually in all capital letters.
#define DPRINT(...) Serial.print(__VA_ARGS__) //DPRINT is a macro, debug print
#define DPRINTLN(...) Serial.println(__VA_ARGS__) //DPRINTLN is a macro, debug print with new line
#else
#define DPRINT(...) //now defines a blank line
#define DPRINTLN(...) //now defines a blank line
#endif

const char* fwImageURL = "http://192.168.1.184/fota/THrain/firmware.bin"; // update with your link to the new firmware bin file.
const char* fwVersionURL = "http://192.168.1.184/fota/THrain/firmware.version"; // update with your link to a text file with new version (just a single line with a number)
// version is used to do OTA only one time, even if you let the firmware file available on the server.
// flashing will occur only if a greater number is available in the "firmware.version" text file.
// take care the number in text file is compared to "FW_VERSION" in the code => this const shall be incremented at each update.

#include <Adafruit_INA219.h>
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h> // may be used to optimise connexion to best AP.
#include <ESP8266httpUpdate.h>
#include <PCF8583.h>
//#include <SparkFun_VEML6075_Arduino_Library.h>
#include <Wire.h>
//#include <ClosedCube_HDC1080.h>
//#include <Adafruit_AM2315.h> // => Modified library from https://github.com/switchdoclabs/SDL_ESP8266_AM2315   I have switched pin 4&5 in cpp file wire.begin
#include "Adafruit_SHT31.h"
#include <PubSubClient.h>

// wifi & mqtt credentials
#include "passwords.h"

// I2C Setup    SCL: D1/GPIO5    SDA: D2/GPIO4  /////////////////////////////////////////////
// setup of sensors
// counter for rain gauge tipping bucket  I2C address 0xA0
PCF8583 rtc(0xA0);
// INA219 current and voltage sensors: to monitor solar panel & battery
Adafruit_INA219 ina219_solar(0x41); // I2C address 0x41   !default is 0x40, conflict with hdc1080
Adafruit_INA219 ina219_battery(0x45); // I2C address 0x45   !default is 0x40, conflict with hdc1080
// UV sensor
//VEML6075 veml6075 = VEML6075();
//VEML6075 uv; // sparkfun lib sensor declaration     I2C address of 0x10 and cannot be changed!
//Temp and humidity sensor.
//ClosedCube_HDC1080 hdc1080;  // default address is 0x40
//Adafruit_AM2315 am2315; // default address is 0x05C (!cannot be changed)
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// WiFi connexion informations //////////////////////////////////////////////////////////////
IPAddress ip(192, 168, 5, 22); // hard coded IP address (make the wifi connexion faster (save battery), no need for DHCP)
IPAddress gateway(192, 168, 5, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network

// MQTT Configuration  //////////////////////////////////////////////////////////////////////
WiFiClient client;
char received_topic[128];
byte received_payload[128];
unsigned int received_length;
bool received_msg = false;
void callback(char* topic, byte* payload, unsigned int length)
{
    strcpy(received_topic, topic);
    memcpy(received_payload, payload, length);
    received_msg = true;
    received_length = length;
}
IPAddress broker(192, 168, 5, 183);
PubSubClient mqtt(broker, 1883, callback, client);
#define STATUS_TOPIC "THrain/Status"
#define VERSION_TOPIC "THrain/Version"
#define CONFIG_TOPIC "THrain/Config"
#define RAIN_TOPIC "weewx/rain"
#define VSOLAR_TOPIC "weewx/Vsolar"
#define ISOLAR_TOPIC "weewx/Isolar"
#define VBAT_TOPIC "weewx/Vbat"
#define HUMI_TOPIC "weewx/outHumidity"
#define TEMP_TOPIC "weewx/outTemp"
#define UV_TOPIC "weewx/UV"
#define MQTT_CLIENT_NAME "ESP_THrain2" // make sure it's a unique identifier on your MQTT broker

// variables /////////////////////////////////////////////////////////////////////////////
unsigned long top = 0;
int   sleep_coef = 1;
float rain = -1;
float temp = -100;
float temp_array[3] = { -100, -100, -100 };
float humi = -1;
float humi_array[3] = { -100, -100, -100 };
float temp_buffer = -100;
float humi_buffer = -1;
float UVindex = -1;
float UVA = -1;
float UVB = -1;
float solar_voltage = -1;
float solar_current = -1; // to check if battery is charging well.
float battery_voltage = -1;
float battery_voltage2 = -1;


// set faster emission in debug mode (take care battery will empty faster!)
#ifdef DEBUGMODE
int sleep_duration = 5; // deep sleep duration in seconds
#else
int sleep_duration = 150; // deep sleep duration in seconds
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup_wifi() : connexion to wifi hotspot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void wifi_connect(const char* ssid, const char* password, uint8_t timeout)
{
    uint8_t timeout_wifi = timeout;
    WiFi.begin(ssid, password);
    while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0)) {
        delay(1000);
        DPRINT(".");
        timeout_wifi--;
    }
}
//Connexion au réseau WiFi
void setup_wifi()
{
    //config static IP
    WiFi.mode(WIFI_STA);
    WiFi.config(ip, gateway, subnet);
    DPRINT("Wifi status: ");
    switch (WiFi.status()) {
    case WL_NO_SHIELD:
        DPRINT("255 :WL_NO_SHIELD");
        break;
    case WL_IDLE_STATUS:
        DPRINT("0 :WL_IDLE_STATUS");
        break;
    case WL_NO_SSID_AVAIL:
        DPRINT("1 :WL_NO_SSID_AVAIL");
        break;
    case WL_SCAN_COMPLETED:
        DPRINT("2 :WL_SCAN_COMPLETED");
        break;
    case WL_CONNECTED:
        DPRINT("3 :WL_CONNECTED");
        break;
    case WL_CONNECT_FAILED:
        DPRINT("4 :WL_CONNECT_FAILED");
        break;
    case WL_CONNECTION_LOST:
        DPRINT("5 :WL_CONNECTION_LOST");
        break;
    case WL_DISCONNECTED:
        DPRINT("6 :WL_DISCONNECTED");
        break;
    }
    DPRINT(" ");
    if (WiFi.status() == WL_CONNECTED) {
        DPRINTLN("Wifi already connected");
        return;
    }
    // try 1st SSID (if you have 2 hotspots)
    DPRINT("Try to connect 1st wifi:");
    wifi_connect(ssid1, password1, 6);
    // if connexion is successful, let's go to next, no need for SSID2
    DPRINTLN("");
    if (WiFi.status() == WL_CONNECTED) {
        DPRINTLN("Wifi ssid1 connected");
        return;
    } else {
        switch (WiFi.status()) {
        case WL_NO_SHIELD:
            DPRINTLN("255 :WL_NO_SHIELD");
            break;
        case WL_IDLE_STATUS:
            DPRINTLN("0 :WL_IDLE_STATUS");
            break;
        case WL_NO_SSID_AVAIL:
            DPRINTLN("1 :WL_NO_SSID_AVAIL");
            break;
        case WL_SCAN_COMPLETED:
            DPRINTLN("2 :WL_SCAN_COMPLETED");
            break;
        case WL_CONNECTED:
            DPRINTLN("3 :WL_CONNECTED");
            break;
        case WL_CONNECT_FAILED:
            DPRINTLN("4 :WL_CONNECT_FAILED");
            break;
        case WL_CONNECTION_LOST:
            DPRINTLN("5 :WL_CONNECTION_LOST");
            break;
        case WL_DISCONNECTED:
            DPRINTLN("6 :WL_DISCONNECTED");
            break;
        }
        DPRINTLN("Let's try connecting 2nd wifi SSID");
    }
    // let's try SSID2 (if ssid1 did not worked)
    wifi_connect(ssid2, password2, 6);
    if (WiFi.status() == WL_CONNECTED) {
        DPRINTLN("Wifi ssid2 connected");
        return; // if connexion is successful, let's go to next, no need for SSID2
    } else {
        DPRINT("2nd try Failed: ");
        switch (WiFi.status()) {
        case WL_NO_SHIELD:
            DPRINTLN("255 :WL_NO_SHIELD");
            break;
        case WL_IDLE_STATUS:
            DPRINTLN("0 :WL_IDLE_STATUS");
            break;
        case WL_NO_SSID_AVAIL:
            DPRINTLN("1 :WL_NO_SSID_AVAIL");
            break;
        case WL_SCAN_COMPLETED:
            DPRINTLN("2 :WL_SCAN_COMPLETED");
            break;
        case WL_CONNECTED:
            DPRINTLN("3 :WL_CONNECTED");
            break;
        case WL_CONNECT_FAILED:
            DPRINTLN("4 :WL_CONNECT_FAILED");
            break;
        case WL_CONNECTION_LOST:
            DPRINTLN("5 :WL_CONNECTION_LOST");
            break;
        case WL_DISCONNECTED:
            DPRINTLN("6 :WL_DISCONNECTED");
            break;
        }
        DPRINTLN("let's sleep and retry later...");
        WiFi.disconnect();
        delay(50);
        ESP.deepSleep(sleep_duration * 48 * 1000000); // if no connexion, deepsleep some time, after will restart & retry (= reset)
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  setup_mqtt() : connexion to mosquitto server
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup_mqtt()
{
    DPRINTLN("Entering mqtt setup");
    if (mqtt.connected()) {
        DPRINTLN("Already connected to MQTT broker");
        return;
    }
    DPRINTLN("Connecting to MQTT broker");

    uint8_t retries = 3;
    DPRINT("Connecting...");
    while (!client.connected()) {
        mqtt.disconnect();
        delay(500);
        mqtt.connect(MQTT_CLIENT_NAME, BROKER_USERNAME, BROKER_KEY);
        DPRINT("MQTT connexion state is: ");
        switch (mqtt.state()) {
        case MQTT_CONNECTION_TIMEOUT:
            DPRINTLN("-4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time");
            break;
        case MQTT_CONNECTION_LOST:
            DPRINTLN("-3 : MQTT_CONNECTION_LOST - the network connection was broken");
            break;
        case MQTT_CONNECT_FAILED:
            DPRINTLN("-2 : MQTT_CONNECT_FAILED - the network connection failed");
            break;
        case MQTT_DISCONNECTED:
            DPRINTLN("-1 : MQTT_DISCONNECTED - the client is disconnected cleanly");
            break;
        case MQTT_CONNECTED:
            DPRINTLN("0 : MQTT_CONNECTED - the client is connected !!  \\o/");
            break;
        case MQTT_CONNECT_BAD_PROTOCOL:
            DPRINTLN("1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT");
            break;
        case MQTT_CONNECT_BAD_CLIENT_ID:
            DPRINTLN("2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier");
            break;
        case MQTT_CONNECT_UNAVAILABLE:
            DPRINTLN("3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection");
            break;
        case MQTT_CONNECT_BAD_CREDENTIALS:
            DPRINTLN("4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected");
            break;
        case MQTT_CONNECT_UNAUTHORIZED:
            DPRINTLN("5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect");
            break;
        }
        retries--;
        if (retries < 1) {
            WiFi.disconnect();
            delay(50);
            ESP.deepSleep(sleep_duration * 48 * 1000000); // if no connexion, deepsleep for 20sec, after will restart (= reset)
        }
    }
    mqtt.publish(STATUS_TOPIC, "Online!");
    mqtt.publish(VERSION_TOPIC, String(FW_VERSION).c_str());
    mqtt.subscribe(CONFIG_TOPIC);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  check_OTA() : check for some available new firmware on server?
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void check_OTA()
{
    // setup_wifi(); must be called before check_OTA();
    DPRINT("Firmware:<");
    DPRINT(FW_VERSION);
    DPRINTLN(">");
    DPRINTLN("Check for OTA");
    HTTPClient http;
    if (http.begin(client, fwVersionURL)) {
        DPRINTLN("http begin");
        int httpCode = http.GET();
        DPRINT("httpCode:");
        DPRINTLN(httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String newFWVersion = http.getString();
            DPRINT("Server  FWVersion: ");
            DPRINT(newFWVersion);
            DPRINT(" / ");
            DPRINT("Current FWVersion: ");
            DPRINTLN(FW_VERSION);
            float newVersion = newFWVersion.toFloat();
            if (newVersion > FW_VERSION) {
                DPRINTLN("start OTA !");
                http.end();
                delay(100);
                t_httpUpdate_return ret = ESPhttpUpdate.update(client, "http://192.168.1.184/fota/THrain/firmware.bin");
                switch (ret) {
                case HTTP_UPDATE_FAILED:
                    DPRINT("HTTP_UPDATE_FAILD Error");
                    DPRINT(ESPhttpUpdate.getLastError());
                    DPRINT(": ");
                    DPRINTLN(ESPhttpUpdate.getLastErrorString().c_str());
                    //USE_SERIAL.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                    break;
                case HTTP_UPDATE_NO_UPDATES:
                    DPRINTLN("HTTP_UPDATE_NO_UPDATES");
                    break;
                case HTTP_UPDATE_OK:
                    DPRINTLN("HTTP_UPDATE_OK");
                    break;
                }
            } else {
                DPRINTLN("no new version available");
            }
        }
    } // end if http.begin
    DPRINTLN("End of OTA");
} // end check_OTA()

void measure_temp_humi(byte index)
{
    DPRINT("Temperature & Humidity measurement index: ");
    DPRINTLN(index);
    if (sht31.begin(0x44)) {
        DPRINT("SHT31 detected: ");
        temp_buffer = sht31.readTemperature();
        DPRINT(temp_buffer);
        DPRINT("°C ");
        humi_buffer = sht31.readHumidity();
        DPRINT(humi_buffer);
        DPRINTLN("%");
        if (!isnan(temp_buffer)) { // check if 'is not a number'
            temp_array[index] = temp_buffer;
        } else {
            temp_array[index] = -100;
        }
        if (!isnan(temp_buffer)) { // check if 'is not a number'
            humi_array[index] = humi_buffer;
        } else {
            humi_array[index] = -100;
        }
    }
    /*
    if (am2315.begin()) {
        if (am2315.readTemperatureAndHumidity(temp_buffer, humi_buffer)) {
            temp_array[index] = temp_buffer; // if function return false, make sure the value are considered wrong and not emitted
            humi_array[index] = humi_buffer;
        } else {
            temp_array[index] = -100; // if function return false, make sure the value are considered wrong and not emitted
            humi_array[index] = -100;
        }
        top = millis();
        DPRINTLN("am2315 detected");
        DPRINT("T1:");
        DPRINTLN(temp);
        DPRINT("H1:");
        DPRINTLN(humi);
    } else {
        DPRINTLN("am2315 non detected");
    }*/
}

void process_temp_humi()
{
    // TEMPERATURE
    DPRINT("Temp_array brut: ");
    DPRINT(temp_array[0]);
    DPRINT(" ");
    DPRINT(temp_array[1]);
    DPRINT(" ");
    DPRINTLN(temp_array[2]);
    float buffer = -1000;
    if (temp_array[0] > temp_array[1]) {
        buffer = temp_array[1];
        temp_array[1] = temp_array[0];
        temp_array[0] = buffer;
    }
    if (temp_array[1] > temp_array[2]) {
        buffer = temp_array[2];
        temp_array[2] = temp_array[1];
        temp_array[1] = buffer;
        if (temp_array[0] > temp_array[1]) {
            buffer = temp_array[1];
            temp_array[1] = temp_array[0];
            temp_array[0] = buffer;
        }
    }
    DPRINT("Temp_array trié: ");
    DPRINT(temp_array[0]);
    DPRINT(" ");
    DPRINT(temp_array[1]);
    DPRINT(" ");
    DPRINTLN(temp_array[2]);
    if ((temp_array[1] - temp_array[0]) < (temp_array[2] - temp_array[1])) {
        temp = (temp_array[0] + temp_array[1]) / 2;
        DPRINT("best samples are 0-1: ave temp = ");
        DPRINTLN(temp);
    } else {
        temp = (temp_array[1] + temp_array[2]) / 2;
        DPRINT("best samples are 1-2: ave temp = ");
        DPRINTLN(temp);
    }
    // HUMIDITY
    DPRINT("Humi_array brut: ");
    DPRINT(humi_array[0]);
    DPRINT(" ");
    DPRINT(humi_array[1]);
    DPRINT(" ");
    DPRINTLN(humi_array[2]);
    if (humi_array[0] > humi_array[1]) {
        buffer = humi_array[1];
        humi_array[1] = humi_array[0];
        humi_array[0] = buffer;
    }
    if (humi_array[1] > humi_array[2]) {
        buffer = humi_array[2];
        humi_array[2] = humi_array[1];
        humi_array[1] = buffer;
        if (humi_array[0] > humi_array[1]) {
            buffer = humi_array[1];
            humi_array[1] = humi_array[0];
            humi_array[0] = buffer;
        }
    }
    DPRINT("Temp_array trié: ");
    DPRINT(humi_array[0]);
    DPRINT(" ");
    DPRINT(humi_array[1]);
    DPRINT(" ");
    DPRINTLN(humi_array[2]);
    if ((humi_array[1] - humi_array[0]) < (humi_array[2] - humi_array[1])) {
        humi = (humi_array[0] + humi_array[1]) / 2;
        DPRINT("best samples are 0-1: ave temp = ");
        DPRINTLN(humi);
    } else {
        humi = (humi_array[1] + humi_array[2]) / 2;
        DPRINT("best samples are 1-2: ave humi = ");
        DPRINTLN(humi);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Start of main programm !
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
#ifdef DEBUGMODE
    Serial.begin(115200);
#endif
    DPRINTLN("");
    DPRINT("MAC address:");
    DPRINTLN(WiFi.macAddress());
    WiFi.mode(WIFI_STA);
    WiFi.config(ip, gateway, subnet);
    WiFi.disconnect();

    measure_temp_humi(0);
    top = millis();

    // check if POR occured ( @POR, register 0x00 of PCF8583 is set to 0 )
    if (rtc.getRegister(0) == 0) { // POR occured, let's clear the SRAM of PCF8583
        for (uint8_t i = 0x10; i < 0x20; i++) { // PCF8583 SRAM start @0x10 to 0xFF, let's clear a few bytes only.
            rtc.setRegister(i, 0);
        }
        rtc.setMode(MODE_EVENT_COUNTER); // will set non zero value in register 0x00, so if no POR occured at next loop, register will not be cleared
        rtc.setCount(0); // reset rain counter
    }

    if (ina219_solar.begin()) {
        ina219_solar.powerSave(false);
        delay(5);
        solar_voltage = ina219_solar.getBusVoltage_V();
        DPRINT("solar_voltage:");
        DPRINTLN(solar_voltage);
        solar_current = (ina219_solar.getCurrent_mA());
        DPRINT("solar_current:");
        DPRINTLN(solar_current);
        solar_current = 0.001 * solar_current; // convert mA to Amps
        ina219_solar.powerSave(true);
    }
    if (ina219_battery.begin()){
        ina219_battery.powerSave(false);
        delay(5);
        battery_voltage = ina219_battery.getBusVoltage_V();
        DPRINT("battery_voltage:");
        DPRINTLN(battery_voltage);
        ina219_battery.powerSave(true);
    }

    
    rain = 0.2 * float(rtc.getCount());
    DPRINT("rain :");
    DPRINTLN(rain);

   /* if (uv.begin()) { // power ON veml6075 early to let it wakeup & warmup
        uv.powerOn();
    }*/
    DPRINT("Entering setup_wifi()....");
    setup_wifi();
    DPRINTLN("End of setup_wifi()");

    // 2nd temp & humi sample: AM2315 needs to way 2 sec between samples
    if (millis() > (top + 2000)) {
        measure_temp_humi(1);
    } else {
        delay(top + 2000 - millis());
        measure_temp_humi(1);
    }
    top = millis();

    
    check_OTA(); // check for new firmware available on server, if check OTA with occur
    //OTA must be checked before connecting to MQTT (or after disconnecting MQTT)
    //measurements done, time to send them all !
    setup_mqtt();
    delay(15);
   
   // to receive mqtt message (retained) not used.
   /* uint8_t loops = 10;
    DPRINT("Check for mqtt reception:");
    while ((received_msg == false) && (loops > 0)) {
        delay(30);
        DPRINT(".");
        mqtt.loop();
        loops--;
    }
    DPRINT("MQTT Message arrived [");
    DPRINT(received_topic);
    DPRINT("] ");
    for (unsigned int i = 0; i < received_length; i++) {
        DPRINT((char)received_payload[i]);
    }
    DPRINTLN("");
    DPRINT("length:");
    DPRINTLN(received_length);
    */

    //setup_mqtt();
    if (ina219_battery.begin()){
        ina219_battery.powerSave(false);
        delay(5);
        battery_voltage2 = ina219_battery.getBusVoltage_V();
        ina219_battery.powerSave(true);
    }


    if (millis() > (top + 2000)) {
        measure_temp_humi(2);
    } else {
        delay(top + 2000 - millis());
        measure_temp_humi(2);
    }

    process_temp_humi();

    mqtt.publish(STATUS_TOPIC, "Publishing!");
    //Status_pub.publish("Publishing!");
    delay(50);
    if ((battery_voltage >= 0) && (battery_voltage < 20.0)) {
        setup_wifi();
        setup_mqtt();
        DPRINT("Vbat:");
        DPRINT(battery_voltage);
        DPRINTLN(" V");
        mqtt.publish(VBAT_TOPIC, String(battery_voltage).c_str());
        delay(50);
    } else {
        mqtt.publish(STATUS_TOPIC, "Bat V out of range");
        delay(50);
    }
    if ((solar_voltage >= 0) && (solar_voltage < 20.0)) {
        setup_wifi();
        setup_mqtt();
        DPRINT("Vsolar:");
        DPRINT(solar_voltage);
        DPRINTLN(" V");
        mqtt.publish(VSOLAR_TOPIC, String(solar_voltage).c_str());
        delay(50);
    } else {
        mqtt.publish(STATUS_TOPIC, "Solar V out of range!");
        delay(50);
    }
    if ((solar_current >= 0) && (solar_current < 1)) {
        setup_wifi();
        setup_mqtt();
        DPRINT("Isolar:");
        DPRINT(solar_current);
        DPRINTLN(" Amps");
        char solar_currant_str[5];
        dtostrf(solar_current, 5, 3, solar_currant_str);
        mqtt.publish(ISOLAR_TOPIC, solar_currant_str);
        delay(50);
    } else {
        mqtt.publish(STATUS_TOPIC, "Solar I out of range!");
        delay(50);
    } 
    if (rain >= 0 && rain < 500) {
        setup_wifi();
        setup_mqtt();
        DPRINT("publishing rain:");
        DPRINT(rain);
        DPRINTLN(" mm");
        if (mqtt.publish(RAIN_TOPIC, String(rain).c_str())) {
            rtc.setCount(0); //  reset rain counter only if it was able to send the date to the mqtt broker
        };
        delay(50);
    } else {
        mqtt.publish(STATUS_TOPIC, "Rain out of range");
        delay(50);
    }
    if (humi >= 0 && humi <= 100) {
        setup_wifi();
        setup_mqtt();
        DPRINT("humi:");
        DPRINT(humi);
        DPRINTLN(" %");
        mqtt.publish(HUMI_TOPIC, String(humi).c_str());
        delay(50);
    } else {
        mqtt.publish(STATUS_TOPIC, "Humi out of range");
        delay(50);
    }
    if ((temp > -40) && (temp < 80)) {
        setup_wifi();
        setup_mqtt();
        DPRINT("temp:");
        DPRINT(temp);
        DPRINTLN(" deg");
        mqtt.publish(TEMP_TOPIC, String(temp).c_str());
        delay(50);
    } else {
        mqtt.publish(STATUS_TOPIC, "OutTemp out of range");
        delay(50);
    }
    // measure UV index and publish
/*if (uv.begin()) {
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
        DPRINT("UV:");
        DPRINTLN(UVindex);
        mqtt.publish(UV_TOPIC, String(UVindex).c_str());
    }*/
    // job is done, let's disconnect
    DPRINTLN("End of measurements & MQTT publish");
    //Status_pub.publish("Offline!");
    mqtt.publish(STATUS_TOPIC, "Offline!");
    delay(25);
    mqtt.disconnect();
    delay(25);
    WiFi.disconnect();
    //Serial.println("Sleep");
    DPRINTLN("Go to sleep");
    delay(15);
    sleep_coef = 1;
    if ((battery_voltage < 3.6) && (battery_voltage2 < 3.6)) {
        sleep_coef = 3; 
    } 
    if ((battery_voltage < 3.4) && (battery_voltage2 < 3.4)) {
        sleep_coef = 12; //30min 
    } 
    if ((battery_voltage < 3.2) && (battery_voltage2 < 3.2)) {
        sleep_coef = 48; // 2h
    } 
     if ((battery_voltage < 0) && (battery_voltage2 < 0)) {  // mesurement error, issue suspected ?
        sleep_coef = 12; //30min 
    } 
    ESP.deepSleep((sleep_duration * sleep_coef * 1000000) - (1000 * millis()));
}

void loop()
{
    // not used due to deepsleep
}
