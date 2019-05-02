// MQTT wind sensor for weewx
const int FW_VERSION = 1;

#include <Arduino.h>
#include <WiFi.h>
#include <DS3232RTC.h>  // https://github.com/JChristensen/DS3232RTC
#include "PCF8583.h"    //https://bitbucket.org/xoseperez/pcf8583.git
#include <Adafruit_ADS1015.h> // library for ADS1115 too
// MQTT
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


// wifi & mqtt credentials
#include "passwords.h"

#define enable_dir_sensor 36  // GPIO36 'A4'  used to drive lowside switch on sensor GND.
#define led 13

#define DEBUGMODE   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef  DEBUGMODE    //Macros are usually in all capital letters.
  #define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DPRINT(...)     //now defines a blank line
  #define DPRINTLN(...)   //now defines a blank line
#endif



// WiFi connexion informations //////////////////////////////////////////////////////////////
IPAddress ip(192, 168, 1, 193);      // hard coded IP address (make the wifi connexion faster (save battery), no need for DHCP)
IPAddress gateway(192, 168, 1, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0);  // set subnet mask to match your network
//IPAddress primaryDNS(212, 27, 40, 241); //optional free.fr 212.27.40.241
//IPAddress secondaryDNS(91, 121, 161, 184); //optional ovh 91.121.161.184

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT,"ESP_wind", AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish windSpeed_pub   = Adafruit_MQTT_Publish(&mqtt, "THwind/windSpeed",   1);
Adafruit_MQTT_Publish windDir_pub     = Adafruit_MQTT_Publish(&mqtt, "THwind/windDir",     1);
Adafruit_MQTT_Publish windGust_pub    = Adafruit_MQTT_Publish(&mqtt, "THwind/windGust",    1);
Adafruit_MQTT_Publish windGustDir_pub = Adafruit_MQTT_Publish(&mqtt, "THwind/windGustDir", 1);
Adafruit_MQTT_Publish Status_pub      = Adafruit_MQTT_Publish(&mqtt, "THwind/Status", 1);
Adafruit_MQTT_Publish Version_pub     = Adafruit_MQTT_Publish(&mqtt, "THwind/Version", 1);
Adafruit_MQTT_Publish Debug_pub       = Adafruit_MQTT_Publish(&mqtt, "THwind/Debug", 1);
bool pub_ok = false;

// RTC DS3232
DS3232RTC RTC(false);  // DS3232M  I2C address = D0h.
// PCF8583 RTC used as even counter (will count the turns of the wind speed sensor)
PCF8583 counter(0xA0); // PCF8583 RTC used as event counter  // I2C address A0
// ADS1115 I2C ADC
Adafruit_ADS1115 adc; // 0x48-0x4B, selectable with jumpers

// variables
RTC_DATA_ATTR int test;
byte All_is_fine; // 8 = 1st boot
uint16_t Vref = 0;  // measure reference voltage of the wind dir sensor
uint16_t Vdir = 0;  // measure wind dir sensor

float WindDir_inst = -1;   // WindDir for inst measurement.

// for average windDir calculations
RTC_DATA_ATTR float WindDir_sum = 0;
RTC_DATA_ATTR int WindDir_samples = 0;

RTC_DATA_ATTR uint16_t T_pulsecount[5];
RTC_DATA_ATTR uint8_t pulsecount_index;
RTC_DATA_ATTR uint8_t first_10min;



int pulsecount = -1;


// values ready to emit
float WindGustDir = -1;
float WindDir = -1;
float WindGust = -1;
float WindSpeed = -1;


// Declaration of function
void setup_wifi();
void setup_mqtt();
void printDigits(int digits);
//void printLocalTime();
//void digitalClockDisplay();
//void printDigits(int digits);
//void print_wakeup_reason();



/******************************************************************************/
/*                      BEGINNING OF PROGRAMM                                 */
/******************************************************************************/

void setup() {
  #ifdef DEBUGMODE
	Serial.begin(115200);
	//DPRINT("DEBUGMODE ON! ");
	//DPRINTLN("HELLO WORLD!");
  #endif
	// init of RTC DS3232M



	RTC.begin();
	setSyncProvider(RTC.get); // the function to get the time from the RTC
	RTC.squareWave(SQWAVE_NONE);
	// first, let's check if everything is All_is_fine
	// a/ check if the wakeup source is the DS3232M, in case the wake is due to ESP internal timer, something is wrong with RTC, let's try to reinit
	// b/ check if oscStopped flag from DS3232M is ok, if not, we cannot rely on its timing, let's try to reinit too
	// c/ check if the PCF8583 event counter has seen a Power ON reset);
	if ((esp_sleep_get_wakeup_cause() !=  ESP_SLEEP_WAKEUP_EXT0)) {  // check if the ESP was wakeup by it's backup timer, normal wake is due to DS3232M
    #ifdef DEBUGMODE
		switch(esp_sleep_get_wakeup_cause())
		{
		case ESP_SLEEP_WAKEUP_EXT0: DPRINTLN("Wakeup caused by external signal using RTC_IO"); break;
		case ESP_SLEEP_WAKEUP_EXT1: DPRINTLN("Wakeup caused by external signal using RTC_CNTL"); break;
		case ESP_SLEEP_WAKEUP_TIMER: DPRINTLN("Wakeup caused by timer"); break;
		case ESP_SLEEP_WAKEUP_TOUCHPAD: DPRINTLN("Wakeup caused by touchpad"); break;
		case ESP_SLEEP_WAKEUP_ULP: DPRINTLN("Wakeup caused by ULP program"); break;
			//default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
		}
    #endif
		All_is_fine = All_is_fine+1;
	}
	if (RTC.oscStopped(true)) {  // OSC stopped for some reason (could be power on)
		DPRINTLN("OSC stop flag occured on DS3232M");
		All_is_fine = All_is_fine+2;
	}
	if ( counter.getRegister(0) == 0 ) { // POR occured on PCF8583
		All_is_fine = All_is_fine+3;
	}






	if (All_is_fine == 0) {
		analogSetAttenuation(ADC_11db);
		analogSetWidth(11);
		// every 15sec task ////////////////////////////////////////  15sec task ///
		// enable wind dir sensor
		digitalWrite(enable_dir_sensor, HIGH);
    #ifdef DEBUGMODE
		digitalWrite(led, HIGH);
    #endif
		delay(10); // turn on delay of the sensor
		Vref = analogRead(A0); // measure reference voltage of the wind dir sensor
		Vdir = analogRead(A1); // measure wind dir sensor
		digitalWrite(enable_dir_sensor, LOW);
		/*DPRINT("Vref:"); DPRINTLN(Vref);
		   DPRINT("Vdir:"); DPRINTLN(Vdir);
		   DPRINT("V-A3:"); DPRINTLN(analogRead(A3));*/
    #ifdef DEBUGMODE
		digitalWrite(led, LOW);
    #endif
		if (Vdir<Vref && Vref>0) { // calculate wind direction  it
			WindDir_inst = 360 *  (float(Vdir) / float(Vref));
			DPRINT("WindDir:"); DPRINTLN(WindDir);
		}

		WindDir_samples++;
		WindDir_sum = WindDir_sum + WindDir_inst;

		// end of 15sec task

		DPRINT("counter:"); DPRINTLN(counter.getCount());

		if ((minute() > 0) && ((minute() % 2) == 0) && (second() < 15)) { //////// every 2 minutes task
			DPRINTLN("********************2 min task***************************");
			// record measurements
			T_pulsecount[pulsecount_index] = counter.getCount(); // get how many pulse where captured bu the counter: we use a rolling array to calculate 10min average
			counter.setCount(0); // reset pulse counter
			// calculate windGust & windSpeed
			WindGust = 4 * float(T_pulsecount[pulsecount_index]) / 120; // calculate gust: 4km/h per Hz:   average frequency is counter/120sec  (2mins)
			WindSpeed = 0;
			for (byte i = 0; i < 5; i = i + 1) {
				DPRINT(".i="); DPRINT(i); DPRINT("WindSpeed:"); DPRINTLN(WindSpeed);
				WindSpeed = WindSpeed + 0.2* 4* float(T_pulsecount[i]) / 120;  // calculate speed on 10minutes.
			}
      #ifdef DEBUGMODE
			DPRINT("WindSpeed:"); DPRINT(WindSpeed); DPRINT(" / WindGust:"); DPRINT(WindGust); DPRINT(" / first_10min"); DPRINTLN(first_10min);
			DPRINT("pulsecount_index:"); DPRINT(pulsecount_index); DPRINT(" T[0]="); DPRINT(T_pulsecount[0]); DPRINT(" T[1]="); DPRINT(T_pulsecount[1]);
			DPRINT(" T[2]="); DPRINT(T_pulsecount[2]); DPRINT(" T[3]="); DPRINT(T_pulsecount[3]); DPRINT(" T[4]="); DPRINTLN(T_pulsecount[4]);
      #endif
			if (pulsecount_index == 4)
				pulsecount_index = 0;
			else
				pulsecount_index++;
			if (first_10min > 0) first_10min--; // during 1st 10 minutes, average value on 10mins has no meaning: we do not emit windspeed during this period. when first_10min reach 0, let's go !



			WindGustDir = WindDir_sum / WindDir_samples;


// end of 2min task




		}
		if ((minute() > 0) && ((minute() % 10) == 0)) { // every 10 minutes task

		}// end of 10min task


	} // end if All_is_fine == 0



	//setup_wifi();
	//setup_mqtt();

//  setup_wifi(wifi_ssid, wifi_password);
//  setup_wifi(wifi_ssid2, wifi_password);
//  setup_mqtt();

	if (WindGust > WindSpeed) {
//emit Gust speed only if higher than average

	}


//  if (windGust_pub.publish(WindGust)) { // publish  dir if speed was published  (speed without direction is acceptable, but what to do with direction without speed?)
//    delay(5);
//    windGustDir_pub.publish(WindGustDir);
//  }

if (All_is_fine > 0) { // something goes wrong, let's clean up and re-init
  DPRINT("Errors or power on occured, code:"); DPRINTLN(All_is_fine);
  setTime(00, 00, 00, 13, 4, 2019); // set time to 0 on DS3232M, we don't care about day or hours, only minutes & seconds
  RTC.set(now());
  /*   for (uint8_t i = 0x10; i < 0x20; i++) { // PCF8583 SRAM start @0x10 to 0xFF, let's clear a few bytes only.
                    counter.setRegister(i, 0);
     }*/
  counter.setMode(MODE_EVENT_COUNTER); // will set non zero value in register 0x00, so if no POR occured at next loop, register will not be cleared
  counter.setCount(0); // reset anemometer pulse counter
  RTC.squareWave(SQWAVE_NONE);
  first_10min = 5;
  WindDir_samples = 0;
  WindDir_sum = 0;
  WindDir = -1;
  pub_ok = false;
  for (byte i = 0; i < 5; i = i + 1) {
    T_pulsecount[i] = 0;
  }
  pulsecount_index = 0;
  All_is_fine = 0;
} // end if All_is_fine > 0





	// set alarm every 15 seconds
	time_t t = now();
	uint8_t sec = second(t);
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
	// end of set  alarmInterrupt
  #ifdef DEBUGMODE
	//DPRINT("test:"); DPRINTLN(test);
	if(timeStatus() != timeSet)
		DPRINTLN("Unable to sync with the RTC");
	else
		DPRINT("time: ");
	if (hour()<10) DPRINT("0");
	DPRINT(hour()); DPRINT(":");
	if (minute()<10) DPRINT("0");
	DPRINT(minute()); DPRINT(":");
	if (second()<10) DPRINT("0");
	DPRINTLN(second());
	#endif

	esp_sleep_enable_timer_wakeup(30 * 1000000);  // backup if RTC had a poweron reset or whatever and loose it's config. then internal timer will wake us in 30sec
	esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,0); //1 = Low to High, 0 = High to low   DS3232M will wake us if "/INT" is connected to GPIO33  (dont forget it's an opendrain, pullup is mandatory)
	DPRINTLN("Going to sleep now");
  #ifdef DEBUGMODE
	Serial.flush();
  #endif
	esp_deep_sleep_start(); // go to deep sleep !
}


/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
void loop() {

}

//Connexion au réseau WiFi
bool setup_wifi(const char* ssid, const char* password) {
	if (WiFi.status() == WL_CONNECTED) {
		return true;
	}
	WiFi.mode(WIFI_STA);
	WiFi.config(ip, gateway, subnet);
	WiFi.begin(ssid, password);
	DPRINT("connecting to :"); DPRINTLN(ssid);
	uint8_t timeout_wifi = 25; //  5sec timeout (25x200ms = 5sec)
	while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0)) {
		delay(200);
		DPRINT(".");
		timeout_wifi--;
	}
	if (WiFi.status() == WL_CONNECTED) {
		DPRINTLN("connected");
		return true;
	} else {
		DPRINT("connection to:"); DPRINT(ssid); DPRINTLN("has failed!");
		return false;
	}
} // end of setup_wifi



/*void setup_wifi_old() {
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
                //esp_sleep_enable_timer_wakeup(time_to_sleep * mS_TO_S_FACTOR);
                //esp_deep_sleep_start();
        }
   }
 */

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
			//time_to_sleep = 15000 - millis();
			//esp_sleep_enable_timer_wakeup(time_to_sleep * mS_TO_S_FACTOR);
			//esp_deep_sleep_start();
		}
	}
	Status_pub.publish("Online!");
	delay(5);
	Version_pub.publish(FW_VERSION);
}
