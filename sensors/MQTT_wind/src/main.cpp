// MQTT wind sensor for weewx
const int FW_VERSION = 1;

#include <Arduino.h>
#include <WiFi.h>
#include <DS3232RTC.h>
// MQTT
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// wifi & mqtt credentials
#include "passwords.h"



#define DEBUGMODE   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef  DEBUGMODE    //Macros are usually in all capital letters.
  #define led 13
  #define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DPRINT(...)     //now defines a blank line
  #define DPRINTLN(...)   //now defines a blank line
#endif

const int SleepSecs  = 1;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define mS_TO_S_FACTOR 1000  /* Conversion factor for micro seconds to seconds */

long time_to_sleep;            /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int test;



// WiFi connexion informations //////////////////////////////////////////////////////////////
IPAddress ip(192, 168, 1, 193);      // hard coded IP address (make the wifi connexion faster (save battery), no need for DHCP)
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0);  // set subnet mask to match your network
//IPAddress primaryDNS(212, 27, 40, 241); //optional free.fr 212.27.40.241
//IPAddress secondaryDNS(91, 121, 161, 184); //optional ovh 91.121.161.184

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT,"ESP_wind", AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish Status_pub      = Adafruit_MQTT_Publish(&mqtt, "THwind/Status", 1);
Adafruit_MQTT_Publish Version_pub     = Adafruit_MQTT_Publish(&mqtt, "THwind/Version", 1);
Adafruit_MQTT_Publish Debug_pub       = Adafruit_MQTT_Publish(&mqtt, "THwind/Debug", 1);


// RTC DS3232
DS3232RTC RTC(false);


// Declaration of function
void setup_wifi();
void setup_mqtt();
void printDigits(int digits);
//void printLocalTime();
//void digitalClockDisplay();
//void printDigits(int digits);
//void print_wakeup_reason();

void setup() {
	RTC.begin();

	if ( RTC.oscStopped(true) ) {          //check the oscillator
		 test = 1000;
	}
	else {

   	}

	setSyncProvider(RTC.get); // the function to get the time from the RTC
	// initialize the alarms to known values, clear the alarm flags, clear the alarm interrupt flags
/*     RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
     RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
     RTC.alarm(ALARM_1);
     RTC.alarm(ALARM_2);
     RTC.alarmInterrupt(ALARM_1, false);
     RTC.alarmInterrupt(ALARM_2, false);*/
	RTC.squareWave(SQWAVE_NONE);

	// set alarm every 10second
	time_t t = now();
	uint8_t sec = second(t); // Returns the second for the given
	if ((sec>=0) && (sec<=14)) {
		RTC.setAlarm(ALM1_MATCH_SECONDS, 15, 0, 0, 1);
	} else if ((sec>=15) && (sec<=29)) {
		RTC.setAlarm(ALM1_MATCH_SECONDS, 30, 0, 0, 1);
	} else if ((sec>=30) && (sec<=44)) {
		RTC.setAlarm(ALM1_MATCH_SECONDS, 45, 0, 0, 1);
	} else if ((sec>=45) && (sec<=59)) {
		RTC.setAlarm(ALM1_MATCH_SECONDS, 00, 0, 0, 1);
	}
	RTC.alarm(ALARM_1);
	RTC.alarmInterrupt(ALARM_1, true);



  #ifdef DEBUGMODE
	Serial.begin(115200);
	DPRINT("test:");DPRINTLN(test);
	DPRINTLN("DEBUGMODE ON");
	if(timeStatus() != timeSet)
		DPRINTLN("Unable to sync with the RTC");
	else
  DPRINT(hour()); DPRINT(":");
	DPRINT(minute()); DPRINT(":");
	DPRINT(second());
	DPRINT(' ');
	DPRINT(day());
	DPRINT(' ');
	DPRINT(month());
	DPRINT(' ');
	DPRINT(year());
  float celsius = RTC.temperature() / 4.0;
  DPRINT("  T °C:"); DPRINTLN(celsius);
	#endif
	//setup_wifi();

  test++;

	//  test for light sleep mode
	//esp_sleep_enable_timer_wakeup(SleepSecs * 1000000);
	esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,0); //1 = Low to High, 0 = High to








	//init and get the time
/*
   printLocalTime();
   configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
   printLocalTime();
 */
	//setup_mqtt();


	//DPRINTLN("Setup ESP32 to sleep for every " + String(time_to_sleep) +  " Seconds");

	//DPRINTLN(millis());
	DPRINTLN("Going to sleep now");
	Serial.flush();
	//time_to_sleep = 5000 - millis();
	//time_to_sleep = 5;
	//esp_sleep_enable_timer_wakeup(time_to_sleep * uS_TO_S_FACTOR);
	esp_deep_sleep_start();
	DPRINTLN("This will never be printed");








}

void loop() {

}

//Connexion au réseau WiFi
void setup_wifi() {
	//config static IP
	WiFi.mode(WIFI_STA);
	//WiFi.config(ip, gateway, subnet, primaryDNS, secondaryDNS);
	WiFi.config(ip, gateway, subnet);
	if (WiFi.status() == WL_CONNECTED) {
		return;
	}
	// try 1st SSID (if you have 2 hotspots)
	WiFi.begin(ssid, password);
	DPRINT("connecting to :"); DPRINTLN(ssid);
	uint8_t timeout_wifi = 6;
	while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0)) {
		delay(1000);
		DPRINT(".");
		timeout_wifi--;
	}
	DPRINTLN(" ");
// if connexion is successful, let's go to next, no need for SSID2
	if (WiFi.status() == WL_CONNECTED) {
		DPRINTLN("connected");
		return;
	}
// let's try SSID2 (if ssid1 did not worked)
	DPRINT("connecting to :"); DPRINTLN(ssid2);
	WiFi.begin(ssid2, password);
	timeout_wifi = 6;
	while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0)) {
		delay(1000);
		DPRINT(".");
		timeout_wifi--;
	}

	if (WiFi.status() == WL_CONNECTED) {
		return; // if connexion is successful, let's go to next, no need for SSID2
	}
	else {
		time_to_sleep = 15000 - millis();
		esp_sleep_enable_timer_wakeup(time_to_sleep * mS_TO_S_FACTOR);
		esp_deep_sleep_start();
	}
}


void setup_mqtt() {
	int8_t ret;
	// Stop if already connected.
	if (mqtt.connected()) {
		return;
	}
	DPRINTLN(" ");
	DPRINT("Connecting to MQTT");
	uint8_t timeout_mqtt = 3;
	while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
		DPRINT(".");
		//DPRINTLN(mqtt.connectErrorString(ret));
		//DPRINTLN("Retrying MQTT connection in 1 seconds...");
		mqtt.disconnect();
		delay(500); // wait 1 seconds
		timeout_mqtt--;
		if (timeout_mqtt == 0) {
			time_to_sleep = 15000 - millis();
			esp_sleep_enable_timer_wakeup(time_to_sleep * mS_TO_S_FACTOR);
			esp_deep_sleep_start();
		}
	}
	Status_pub.publish("Online!");
	delay(5);
	Version_pub.publish(FW_VERSION);
}
