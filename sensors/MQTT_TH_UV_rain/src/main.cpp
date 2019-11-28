const float FW_VERSION = 1.47;
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
#ifdef DEBUGMODE								  //Macros are usually in all capital letters.
#define DPRINT(...) Serial.print(__VA_ARGS__)	 //DPRINT is a macro, debug print
#define DPRINTLN(...) Serial.println(__VA_ARGS__) //DPRINTLN is a macro, debug print with new line
#else
#define DPRINT(...)   //now defines a blank line
#define DPRINTLN(...) //now defines a blank line
#endif

const char *fwImageURL   = "http://192.168.1.181/fota/THrain/firmware.bin";	  // update with your link to the new firmware bin file.
const char *fwVersionURL = "http://192.168.1.181/fota/THrain/firmware.version"; // update with your link to a text file with new version (just a single line with a number)
// version is used to do OTA only one time, even if you let the firmware file available on the server.
// flashing will occur only if a greater number is available in the "firmware.version" text file.
// take care the number in text file is compared to "FW_VERSION" in the code => this const shall be incremented at each update.

#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h> // may be used to optimise connexion to best AP.
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PCF8583.h>
#include <Adafruit_INA219.h>
#include <SparkFun_VEML6075_Arduino_Library.h>
//#include <ClosedCube_HDC1080.h>
#include <Adafruit_AM2315.h> // => Modified library from https://github.com/switchdoclabs/SDL_ESP8266_AM2315   I have switched pin 4&5 in cpp file wire.begin
#include <PubSubClient.h>

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
//Temp and humidity sensor.
//ClosedCube_HDC1080 hdc1080;  // default address is 0x40
Adafruit_AM2315 am2315; // default address is 0x05C (!cannot be changed)

// WiFi connexion informations //////////////////////////////////////////////////////////////
IPAddress ip(192, 168, 1, 191);		 // hard coded IP address (make the wifi connexion faster (save battery), no need for DHCP)
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0);  // set subnet mask to match your network

// MQTT server informations /////////////////////////////////////////////////////////////////
// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// MQTT subscribe handling /////////////////////////////////////////////////////////////////
char received_topic[128];
byte received_payload[128];
unsigned int received_length;
bool received_msg = false;
void callback(char *topic, byte *payload, unsigned int length)
{
	strcpy(received_topic, topic);
	memcpy(received_payload, payload, length);
	received_msg = true;
	received_length = length;
}
IPAddress broker(192, 168, 1, 181);
PubSubClient mqtt(broker, 1883, callback, client);
#define STATUS_TOPIC  "THrain/Status"
#define VERSION_TOPIC "THrain/Version"
#define CONFIG_TOPIC  "THrain/Config"
#define RAIN_TOPIC    "weewx/rain"
#define VSOLAR_TOPIC  "weewx/Vsolar"
#define ISOLAR_TOPIC  "weewx/Isolar"
#define VBAT_TOPIC    "weewx/Vbat"
#define HUMI_TOPIC    "weewx/outHumidity"
#define TEMP_TOPIC    "weewx/outTemp"
#define UV_TOPIC      "weewx/UV"
#define MQTT_CLIENT_NAME    "ESP_THrain" // make sure it's a unique identifier on your MQTT broker

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
/*Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, "ESP_test", AIO_USERNAME, AIO_KEY);
   Adafruit_MQTT_Publish rain_pub        = Adafruit_MQTT_Publish(&mqtt, "xxweewx/rain", 0);  // QoS 0 because QoS 1 may send several time rain (could be counted several times by weewx
   Adafruit_MQTT_Publish outTemp_pub     = Adafruit_MQTT_Publish(&mqtt, "xxweewx/outTemp", 1);
   Adafruit_MQTT_Publish outHumidity_pub = Adafruit_MQTT_Publish(&mqtt, "xxweewx/outHumidity", 1);
   Adafruit_MQTT_Publish UV_pub          = Adafruit_MQTT_Publish(&mqtt, "xxweewx/UV", 1);
   Adafruit_MQTT_Publish Vsolar_pub      = Adafruit_MQTT_Publish(&mqtt, "xxweewx/Vsolar", 1);
   Adafruit_MQTT_Publish Isolar_pub      = Adafruit_MQTT_Publish(&mqtt, "xxweewx/Isolar", 1);
   Adafruit_MQTT_Publish IsolarA_pub     = Adafruit_MQTT_Publish(&mqtt, "xxweewx/Isolar", 1);
   Adafruit_MQTT_Publish Vbat_pub        = Adafruit_MQTT_Publish(&mqtt, "xxweewx/Vbat", 1);
   Adafruit_MQTT_Publish Status_pub      = Adafruit_MQTT_Publish(&mqtt, "xxTHrain/Status", 1);
   Adafruit_MQTT_Publish Version_pub     = Adafruit_MQTT_Publish(&mqtt, "xxTHrain/Version", 1);
   Adafruit_MQTT_Publish Debug_pub       = Adafruit_MQTT_Publish(&mqtt, "xxTHrain/Debug", 1);*/

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
float solar_current = -1; // to check if battery is charging well.
float battery_voltage = -1;

// set faster emission in debug mode (take care battery will empty faster!)
#ifdef DEBUGMODE
int sleep_duration = 5; // deep sleep duration in seconds
#else
int sleep_duration = 150; // deep sleep duration in seconds
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup_wifi() : connexion to wifi hotspot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void wifi_connect(const char *ssid, const char* password, uint8_t timeout)
{
	uint8_t timeout_wifi = timeout;
	WiFi.begin(ssid, password);
	while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0))
	{
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
	switch (WiFi.status())
	{
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
	if (WiFi.status() == WL_CONNECTED)
	{
		DPRINTLN("Wifi already connected");
		return;
	}
	// try 1st SSID (if you have 2 hotspots)
	DPRINT("Try to connect 1st wifi:");
	wifi_connect(ssid1, password1, 30);
	// if connexion is successful, let's go to next, no need for SSID2
	if (WiFi.status() == WL_CONNECTED)
	{
		DPRINTLN("Wifi ssid1 connected");
		return;
	}
	else
	{
		DPRINTLN("Let's try connecting 2nd wifi SSID");
	}
	// let's try SSID2 (if ssid1 did not worked)
	wifi_connect(ssid2, password2, 30);
	if (WiFi.status() == WL_CONNECTED)
	{
		DPRINTLN("Wifi ssid2 connected");
		return; // if connexion is successful, let's go to next, no need for SSID2
	}
	else
	{
		DPRINTLN("let's sleep and retry later...");
		delay(50);
		ESP.deepSleep(sleep_duration * 1000000); // if no connexion, deepsleep some time, after will restart & retry (= reset)
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  setup_mqtt() : connexion to mosquitto server
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup_mqtt()
{
	DPRINTLN("Entering mqtt setup");
	if (mqtt.connected())
	{
		DPRINTLN("Already connected to MQTT broker");
		return;
	}
	DPRINTLN("Connecting to MQTT broker");

	uint8_t retries = 3;
	DPRINT("Connecting...");
	while (!client.connected())
	{
		mqtt.disconnect();
		delay(1000);
		mqtt.connect(MQTT_CLIENT_NAME, BROKER_USERNAME, BROKER_KEY);
		DPRINT("MQTT connexion state is: ");
		switch (mqtt.state())
		{
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
		if (retries == 0)
		{
			ESP.deepSleep(sleep_duration * 1000000); // if no connexion, deepsleep for 20sec, after will restart (= reset)
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
	if (http.begin(client, fwVersionURL))
	{
		DPRINTLN("http begin");
		int httpCode = http.GET();
		DPRINT("httpCode:");
		DPRINTLN(httpCode);
		if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
		{
			String newFWVersion = http.getString();
			DPRINT("Server  FWVersion: ");
			DPRINT(newFWVersion);
			DPRINT(" / ");
			DPRINT("Current FWVersion: ");
			DPRINTLN(FW_VERSION);
			float newVersion = newFWVersion.toFloat();
			if (newVersion > FW_VERSION)
			{
				DPRINTLN("start OTA !");
				http.end();
				delay(100);
				t_httpUpdate_return ret = ESPhttpUpdate.update(client, "http://192.168.1.181/fota/THrain/firmware.bin");
				switch (ret)
				{
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
			}
			else
			{
				DPRINTLN("no new version available");
			}
		}
	} // end if http.begin
	DPRINTLN("End of OTA");
} // end check_OTA()

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
	// check if POR occured ( @POR, register 0x00 of PCF8583 is set to 0 )
	if (rtc.getRegister(0) == 0)
	{ // POR occured, let's clear the SRAM of PCF8583
		for (uint8_t i = 0x10; i < 0x20; i++)
		{ // PCF8583 SRAM start @0x10 to 0xFF, let's clear a few bytes only.
			rtc.setRegister(i, 0);
		}
		rtc.setMode(MODE_EVENT_COUNTER); // will set non zero value in register 0x00, so if no POR occured at next loop, register will not be cleared
		rtc.setCount(0);				 // reset rain counter
	}
	ina219_solar.begin();
	ina219_battery.begin();
	/*hdc1080.begin(0x40);
	   if(hdc1080.readManufacturerId() == 0x5449) {  // as "begin" does not provide boolean answer, check id to confirm HDC1080 is connected
	   temp = hdc1080.readTemperature();
	   humi = hdc1080.readHumidity();
	   };*/
	if (am2315.begin())
	{
		if (am2315.readTemperatureAndHumidity(temp_buffer, humi_buffer))
		{
			temp = temp_buffer; // if function return false, make sure the value are considered wrong and not emitted
			humi = humi_buffer;
		}
		else
		{
			temp = -100; // if function return false, make sure the value are considered wrong and not emitted
			humi = -1;
		}
		DPRINTLN("am2315 detected");
		DPRINT("T:");
		DPRINTLN(temp);
		DPRINT("H:");
		DPRINTLN(humi);
	}
	else
	{
		DPRINTLN("am2315 non detected");
	}
	rain = 0.2 * float(rtc.getCount());
	DPRINT("rain :");
	DPRINTLN(rain);
	solar_voltage = ina219_solar.getBusVoltage_V();
	DPRINT("solar_voltage:");
	DPRINTLN(solar_voltage);
	solar_current = (ina219_solar.getCurrent_mA());
	DPRINT("solar_current:");
	DPRINTLN(solar_current);
	solar_current = 0.001 * solar_current; // convert mA to Amps
	battery_voltage = ina219_battery.getBusVoltage_V();
	DPRINT("battery_voltage:");
	DPRINTLN(battery_voltage);

	if (uv.begin())
	{ // power ON veml6075 early to let it wakeup
		uv.powerOn();
	}
	DPRINT("Entering setup_wifi()....");
	setup_wifi();
	DPRINTLN("End of setup_wifi()");
	check_OTA(); // check for new firmware available on server, if check OTA with occur
	//OTA must be checked before connecting to MQTT (or after disconnecting MQTT)
	//measurements done, time to send them all !
	setup_mqtt();
	delay(15);
	uint8_t loops = 10;
	DPRINT("Check for mqtt reception:");
	while ((received_msg == false) && (loops > 0))
	{
		delay(30);
		DPRINT(".");
		mqtt.loop();
		loops--;
	}
	DPRINT("MQTT Message arrived [");
	DPRINT(received_topic);
	DPRINT("] ");
	for (unsigned int i = 0; i < received_length; i++)
	{
		DPRINT((char)received_payload[i]);
	}
	DPRINTLN("");
	DPRINT("length:");
	DPRINTLN(received_length);

	//setup_mqtt();

	// I wise to average T & H on a few samples, but am2315 does not allow fast reading (min 2sec recommanded according to adafruit)
	// I use the wifi connexion as a delay (a few seconds)
	if (am2315.readTemperatureAndHumidity(temp_buffer, humi_buffer))
	{
		temp = (temp + temp_buffer) / 2; // if function return false, make sure the value are considered wrong and not emitted
		humi = (humi + humi_buffer) / 2;
	} // no else here, if we were lucky at 1st measurement, we have a least a value, if not, let's forget T or H this time.
	mqtt.publish(STATUS_TOPIC, "Publishing!");
	//Status_pub.publish("Publishing!");
	delay(50);
	if ((solar_voltage >= 0) && (solar_voltage < 20.0))
	{
		setup_wifi();
		setup_mqtt();
		DPRINT("Vsolar:");
		DPRINT(solar_voltage);
		DPRINTLN(" V");
		mqtt.publish(VSOLAR_TOPIC, String(solar_voltage).c_str());
		delay(50);
	}
	if ((solar_current >= 0) && (solar_current < 1))
	{
		setup_wifi();
		setup_mqtt();
		DPRINT("Isolar:");
		DPRINT(solar_current);
		DPRINTLN(" Amps");
		char solar_currant_str[5];
		dtostrf(solar_current,5,3,solar_currant_str);
		mqtt.publish(ISOLAR_TOPIC, solar_currant_str);
		delay(50);
	}
	if ((battery_voltage >= 0) && (battery_voltage < 20.0))
	{
		setup_wifi();
		setup_mqtt();
		DPRINT("Vbat:");
		DPRINT(battery_voltage);
		DPRINTLN(" V");
		mqtt.publish(VBAT_TOPIC, String(battery_voltage).c_str());
		delay(50);
	}
	if (rain >= 0 && rain < 500)
	{
		setup_wifi();
		setup_mqtt();
		DPRINT("publishing rain:");
		DPRINT(rain);
		DPRINTLN(" mm");
		if (mqtt.publish(RAIN_TOPIC, String(rain).c_str()))
		{
			rtc.setCount(0); //  reset rain counter only if it was able to send the date to the mqtt broker
		};
		delay(50);
	}
	if (humi >= 0 && humi <= 100)
	{
		setup_wifi();
		setup_mqtt();
		DPRINT("humi:");
		DPRINT(humi);
		DPRINTLN(" %");
		mqtt.publish(HUMI_TOPIC, String(humi).c_str());
		delay(50);
	}
	if ((temp > -40) && (temp < 80))
	{
		setup_wifi();
		setup_mqtt();
		DPRINT("temp:");
		DPRINT(temp);
		DPRINTLN(" deg");
		mqtt.publish(TEMP_TOPIC, String(temp).c_str());
		delay(50);
	}
	// measure UV index and publish
	if (uv.begin())
	{
		UVindex = uv.index();
		delay(2);
		UVindex = UVindex + uv.index();
		UVindex = UVindex + uv.index();
		UVindex = UVindex / 3;
		uv.shutdown();
	}
	if ((UVindex > 0) && (UVindex < 25))
	{
		setup_wifi();
		setup_mqtt();
		DPRINT("UV:");
		DPRINTLN(UVindex);
		mqtt.publish(UV_TOPIC, String(UVindex).c_str());
	}
	// job is done, let's disconnect
	DPRINTLN("End of measurements & MQTT publish");
	//Status_pub.publish("Offline!");
	mqtt.publish(STATUS_TOPIC, "Offline!");
	delay(15);
	mqtt.disconnect();
	delay(15);
	WiFi.disconnect();
	//Serial.println("Sleep");
	DPRINTLN("Go to sleep");
	delay(15);
	ESP.deepSleep(sleep_duration * 1000000);
}

void loop()
{
	// not used due to deepsleep
}
