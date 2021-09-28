// MQTT wind sensor for weewx
const float FW_VERSION = 1.49;
// history:
// 1.49 : update IP addresses
// 1.48 : improve watchdog.


const char* fwImageURL = "http://192.168.1.184/fota/Wind/firmware.bin"; // update with your link to the new firmware bin file.
const char* fwVersionURL = "http://192.168.1.184/fota/Wind/firmware.version";
// update with your link to a text file with new version (just a single line with a number)
// version is used to do OTA only one time, even if you let the firmware file available on the server.
// flashing will occur only if a greater number is available in the "firmware.version" text file.
// take care the number in text file is compared to "FW_VERSION" in the code => this const shall be incremented at each update.

#include <esp_task_wdt.h>
#include <WiFiMulti.h>

#include "PCF8583.h" // https://github.com/xoseperez/pcf8583 
#include <Adafruit_ADS1015.h> // library for ADS1115 too
#include <Adafruit_INA219.h>
#include <Arduino.h>
#include <DS3232RTC.h> // https://github.com/JChristensen/DS3232RTC
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <PubSubClient.h>
#include <WiFi.h>
// wifi & mqtt credentials
#include "passwords.h"

// pining
#define enable_dir_sensor 32 // GPIO36 'A4'  used to drive lowside switch on sensor GND.
#define led 13

// general timings
#define RATIO_KMH_TO_HZ 4
#define RATIO_MPS_TO_HZ 1.111112 //  meter per second
#define TSAMPLE 10
// Define the sample rate:  the ESP will wake every "TSAMPLE" second  to measure speed & direction, ex every 15sec
// TSAMPLE must be a subdivision of 60sec, ex 10,12,15, but  not 8, 11, 13...
#define RATIO 30 // define how many TSAMPLE period are needed to perform average, example 8
const uint16_t Taverage = TSAMPLE * RATIO; // Define the average rate:  the ESP will process average value "Taverage" second , example 8x15 = 120sec = 2min
//#define STILL_ALIVE 15 // will emit every STILL_ALIVE min even if no wind, must be a multiple of Taverage

// macro for debug
//#define DEBUGMODE //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUGMODE //Macros are usually in all capital letters.
#define DPRINT(...) Serial.print(__VA_ARGS__) //DPRINT is a macro, debug print
#define DPRINTLN(...) Serial.println(__VA_ARGS__) //DPRINTLN is a macro, debug print with new line
#else
#define DPRINT(...) //now defines a blank line
#define DPRINTLN(...) //now defines a blank line
#endif

// WiFi connexion informations //////////////////////////////////////////////////////////////

IPAddress ip(192, 168, 5, 21); // hard coded IP address (make the wifi connexion faster (save battery), no need for DHCP)
IPAddress gateway(192, 168, 5, 254); // set gateway to match your network
IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
//IPAddress primaryDNS(212, 27, 40, 241); //optional free.fr 212.27.40.241
//IPAddress secondaryDNS(91, 121, 161, 184); //optional ovh 91.121.161.184

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
#define STATUS_TOPIC "wind/Status"
#define VERSION_TOPIC "wind/Version"
#define CONFIG_TOPIC "wind/Config"
#define ERROR_TOPIC "wind/Error"
#define WINDSPEED_TOPIC "weewx/windSpeed"
#define WINDDIR_TOPIC "weewx/windDir"
#define WINDGUST_TOPIC "weewx/windGust"
#define WINDGUSTDIR_TOPIC "weewx/windGustDir"
#define VSOLAR_TOPIC "weewx/WVsolar"
#define ISOLAR_TOPIC "weewx/WIsolar"
#define VBAT_TOPIC "weewx/WVbat"
#define MQTT_CLIENT_NAME "ESP32_WIND" // make sure it's a unique identifier on your MQTT broker

#define WINDSPEED_TTOPIC "tttweewx/windSpeed"
#define WINDDIR_TTOPIC "tttweewx/windDir"
#define WINDGUST_TTOPIC "tttweewx/windGust"
#define WINDGUSTDIR_TTOPIC "tttweewx/windGustDir"
#define WINDGUSTMAX_TTOPIC "tttweewx/windGustMax"
#define WINDGUSTMAXDIR_TTOPIC "tttweewx/windGustMaxDir"

#define ISOLAR_TTOPIC "tttweewx/Isolar"
#define VDIR_TTOPIC "tttweewx/Vdir"
#define VREF_TTOPIC "tttweewx/Vref"
#define INDEX_TTOPIC "tttweewx/index"
/*
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT,"ESP_wind", AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish windSpeed_pub   = Adafruit_MQTT_Publish(&mqtt, "tweewx/windSpeed",   1);
Adafruit_MQTT_Publish windDir_pub     = Adafruit_MQTT_Publish(&mqtt, "tweewx/windDir",     1);
Adafruit_MQTT_Publish windGust_pub    = Adafruit_MQTT_Publish(&mqtt, "tweewx/windGust",    1);
Adafruit_MQTT_Publish windGustDir_pub = Adafruit_MQTT_Publish(&mqtt, "tweewx/windGustDir", 1);
Adafruit_MQTT_Publish Status_pub      = Adafruit_MQTT_Publish(&mqtt, "wind/Status",        1);
Adafruit_MQTT_Publish Error_pub       = Adafruit_MQTT_Publish(&mqtt, "wind/Errorcode",     1);

Adafruit_MQTT_Publish Version_pub     = Adafruit_MQTT_Publish(&mqtt, "wind/Version", 1);
Adafruit_MQTT_Publish Debug_pub       = Adafruit_MQTT_Publish(&mqtt, "wind/Debug", 1);
Adafruit_MQTT_Publish Vsolar_pub      = Adafruit_MQTT_Publish(&mqtt, "weewx/WVsolar", 1);
Adafruit_MQTT_Publish Isolar_pub      = Adafruit_MQTT_Publish(&mqtt, "weewx/WIsolar", 1);
Adafruit_MQTT_Publish Vbat_pub        = Adafruit_MQTT_Publish(&mqtt, "weewx/WVbat", 1);
*/

/**********************  I2C  Components  *************************************/
// RTC DS3232
DS3232RTC RTC(false); // DS3232M  I2C address = D0h.
// PCF8583 RTC used as even counter (will count the turns of the wind speed sensor)
PCF8583 counter(0xA0); // PCF8583 RTC used as event counter  // I2C address A0
// ADS1115 I2C ADC
Adafruit_ADS1115 ads; // 0x48-0x4B, selectable with jumpers
// INA219 current and voltage sensors: to monitor solar panel & battery
Adafruit_INA219 ina219_battery(0x40); // I2C address 0x40   !default is 0x40
Adafruit_INA219 ina219_solar(0x41); // I2C address 0x41   !default is 0x40

/********************************** variables *********************************/
uint8_t All_is_fine;
uint16_t Vref = 0; // measure reference voltage of the wind dir sensor
uint16_t Vdir = 0; // measure wind dir sensor

// RTC memory (not lost during deepsleep
RTC_DATA_ATTR uint16_t Table_pulsecount[RATIO];
RTC_DATA_ATTR uint16_t Table_windDir[RATIO];
RTC_DATA_ATTR uint8_t Tindex;
RTC_DATA_ATTR uint8_t ready; // data are valids when it ready 0...used to avoid sending during 1st minutes when we don't have  valid average value
RTC_DATA_ATTR uint16_t pulsecount;
RTC_DATA_ATTR uint16_t prev_pulsecount;
RTC_DATA_ATTR int windGustMaxDir;
RTC_DATA_ATTR float windGustMax;

uint16_t total_pulsecount;
//uint16_t total_dircount;
long total_dircount;
uint8_t nextwake;
time_t t;
// values ready to emit
int windGustDir = -1; // -1 is just to help detecting an untouched value
int windDir = -1;
float windGust = -1;
float windSpeed = -1;

float solar_voltage = -1;
float solar_current = -1;
float battery_voltage = -1;

// Declaration of function
void setup_wifi();
void setup_mqtt();
void report_wake_source();
void check_OTA();
void wifi_scan();

//  watchdog timer in case of unexpected crash ?
#define WDT_TIMEOUT 30

/******************************************************************************/
/*                      BEGINNING OF PROGRAMM                                 */
/******************************************************************************/

void setup()
{
  
#ifdef DEBUGMODE
    Serial.begin(115200);
    DPRINT(millis());
    //delay(2000);
    DPRINTLN(" HELLO WORLD!");
    //DPRINT("MAC Address: ");
    //DPRINTLN(WiFi.macAddress());
    //DPRINTLN("Init watchdog");
#endif

  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  esp_task_wdt_delete(NULL);

#ifdef DEBUGMODE
    //DPRINTLN("Watchdog init done!");
#endif

    WiFi.mode(WIFI_STA);
    if (!WiFi.config(ip, gateway, subnet)) {
        DPRINTLN("STA Failed to configure");
    };
    WiFi.disconnect();
    RTC.begin(); // init of RTC DS3232M
    setSyncProvider(RTC.get); // the function to get the time from the RTC
    t = now(); // save the wakeup time for later processing.
    prev_pulsecount = pulsecount;
    pulsecount = counter.getCount(); // record pulsecount as early as possible after wake to be as accurate as possible on timings.
    pinMode(enable_dir_sensor, OUTPUT);
    pinMode(led, OUTPUT);
    digitalWrite(led, HIGH);
    ina219_solar.begin();
    ina219_battery.begin();
    ads.setGain(GAIN_ONE); // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
    digitalWrite(led, LOW); // short blink at startup
    // first, let's check if everything is All_is_fine
    // a/ check if the wakeup source is the DS3232M, in case the wake is due to ESP internal timer, something is wrong with RTC, let's try to reinit
    // b/ check if oscStopped flag from DS3232M is ok, if not, we cannot rely on its timing, let's try to reinit too
    // c/ check if the PCF8583 event counter has seen a Power ON reset);
    if ((esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0)) { // check if the ESP was wakeup by it's backup timer, normal wake is due to DS3232M
#ifdef DEBUGMODE
        report_wake_source();
#endif
        All_is_fine = All_is_fine + 1;
    }
    if (RTC.oscStopped(true)) { // OSC stopped for some reason (could be power on)
        DPRINTLN("OSC stop flag occured on DS3232M");
        All_is_fine = All_is_fine + 2;
    }
    if (counter.getRegister(0) == 0) { // POR occured on PCF8583
        All_is_fine = All_is_fine + 4;
    }
    if (All_is_fine == 0) { // skipped if the ESP doest not wake in expected situation.

        // every 15sec task ////////////////////////////////////////  15sec task ///
        // enable wind dir sensor
        if ((pulsecount - prev_pulsecount) > 0) { // perform direction measurement if there are some wind only (to save some battery)
            digitalWrite(enable_dir_sensor, HIGH);
            delay(10); // turn on delay of the sensor,value to be checked
            Vref = ads.readADC_Differential_0_1();
            Vdir = ads.readADC_Differential_2_3();
            digitalWrite(enable_dir_sensor, LOW);
            DPRINT("Vref:");
            DPRINT(Vref);
            DPRINT(" Vdir:");
            DPRINTLN(Vdir);
            if (Vdir <= Vref && Vref > 17000 && Vref < 21000 && Vdir >= 0) { // calculate wind direction
                if (Vdir > (Vref - 256)) {
                    windGustDir = 360; // just in case the calibration is wrong and Vdir gets higher than corrected reference.
                } else {
                    windGustDir = int(360 * (float(Vdir) / float(Vref - 256)));
                    //apparently, the sensor output cannot reach supply voltage,  remove 256 seems make possible that the output reach supply = 360°
                }
            }
        } else { // If no wind, keep same dir as previous: we need a value to calculate average windDir
            if (Tindex == 0) {
                windGustDir = int(Table_windDir[RATIO - 1]);
            } else {
                windGustDir = int(Table_windDir[Tindex - 1]);
            }
        }

        // Store measured wind / winddir
        Table_pulsecount[Tindex] = pulsecount - prev_pulsecount;
        Table_windDir[Tindex] = windGustDir;
        // calculate average wind speed & dir
        total_pulsecount = 0;
        total_dircount = 0;
        for (byte i = 0; i < RATIO; i = i + 1) {
            total_pulsecount = total_pulsecount + Table_pulsecount[i];
            total_dircount = total_dircount + Table_windDir[i];
        }
        windGust = RATIO_MPS_TO_HZ * float(pulsecount - prev_pulsecount) / TSAMPLE;
        windSpeed = RATIO_MPS_TO_HZ * float(total_pulsecount) / Taverage;
        windDir = int(total_dircount / RATIO);
        if (windGustMax < windGust) {
            windGustMax = windGust;
            windGustMaxDir = windGustDir;
        }

        //************************************************ for debug *****************************************
        /*        setup_wifi();
        setup_mqtt();
        delay(10);
        mqtt.publish(STATUS_TOPIC, "debug!");
        delay(10);
        mqtt.publish(INDEX_TTOPIC, String(Tindex).c_str());
        delay(10);

        //mqtt.publish(WINDGUSTDIR_TTOPIC, String(windGustDir).c_str());
        //delay(10);
        mqtt.publish(WINDGUST_TTOPIC, String(windGust).c_str());
        delay(10);
        //mqtt.publish(WINDGUSTMAXDIR_TTOPIC, String(windGustMaxDir).c_str());
        //delay(10);
        mqtt.publish(WINDGUSTMAX_TTOPIC, String(windGustMax).c_str());
        delay(10);
        //mqtt.publish(VDIR_TTOPIC, String(Vdir).c_str());
        //delay(10);
        //mqtt.publish(VREF_TTOPIC, String(Vref).c_str());
        //delay(10);
        mqtt.publish(STATUS_TOPIC, "enddebug!");
        delay(10);
        if (mqtt.connected()) {
            mqtt.publish(STATUS_TOPIC, "Offline!");
            mqtt.disconnect();
        }
        delay(10);
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.disconnect();
        }
        */
        //************************************************ end for debug *****************************************

#ifdef DEBUGMODE
        DPRINT("Tindex:");
        DPRINT(Tindex);
        DPRINT("  ready:");
        DPRINT(ready);
        DPRINT("  pulsecount:");
        DPRINT(pulsecount);
        DPRINT("  prev_pulsecount:");
        DPRINT(prev_pulsecount);
        DPRINT(" windGustDir=");
        DPRINTLN(windGustDir);
        DPRINT("Table_pulsecount:");
        for (byte i = 0; i < RATIO; i = i + 1) {
            DPRINT(" [");
            DPRINT(i);
            DPRINT("]={");
            DPRINT(Table_pulsecount[i]);
            DPRINT("}");
        }
        DPRINTLN(" #");
        DPRINT("Table_windDir:   ");
        for (byte i = 0; i < RATIO; i = i + 1) {
            DPRINT(" [");
            DPRINT(i);
            DPRINT("]={");
            DPRINT(Table_windDir[i]);
            DPRINT("}");
        }
        DPRINTLN(" #");
        DPRINT("windSpeed:");
        DPRINT(windSpeed);
        DPRINT("km/h / windDir:");
        DPRINT(windDir);
        DPRINT("° ");
        DPRINT("windGust:");
        DPRINT(windGust);
        DPRINT("km/h / windGustDir:");
        DPRINT(windGustDir);
        DPRINTLN("° ");
#endif

        if (ready > 0) {
            ready--;
        }

        if (ready == 0) { // does not allow emit a while after power on (until we have enough sample for average measurements)
            if (Tindex == 0) {
                if ((windSpeed > 1) && (windSpeed < 400) && (windDir >= 0) && (windDir <= 360)) {
                    // let's emit every 2min, if valid data & if speed > 1km/h  (let's save battery if below 1km/h)
                    DPRINTLN("Let's emit average wind ************************************************");
                    setup_wifi();
                    setup_mqtt();
                    delay(10);
                    mqtt.publish(WINDSPEED_TOPIC, String(windSpeed).c_str());
                    delay(10);
                    mqtt.publish(WINDDIR_TOPIC, String(windDir).c_str());
                }
                //   if ((windGust > (windSpeed + 5)) && (windGust < 400) && (windGustDir >= 0) && (windGustDir <= 360) && (windSpeed > 1)) {
                // little filter here too,emit gust only if high enought
                if ((windSpeed > 1) && (windGustMax > windSpeed) && (windGustMax < 400) && (windGustMaxDir >= 0) && (windGustMaxDir <= 360)) {
                    // little filter here too,emit gust only if high enought
                    DPRINTLN("Let's emit Gust *********************************************************");
                    setup_wifi();
                    setup_mqtt();
                    /*if (Tindex == 0) {
                    delay(10);
                    }*/
                    delay(10);
                    mqtt.publish(WINDGUST_TOPIC, String(windGustMax).c_str());
                    delay(10);
                    mqtt.publish(WINDGUSTDIR_TOPIC, String(windGustMaxDir).c_str());
                }
                //if (((minute() % STILL_ALIVE) == 0)) {
                // let's emit a few times even if there is no wind (so we know the sensor is alive), we can use it to send also battery & solar situation
                DPRINTLN("Let's emit STILL_ALIVE min message");
                solar_voltage = ina219_solar.getBusVoltage_V();
                DPRINT("solar_voltage:");
                DPRINTLN(solar_voltage);
                solar_current = ina219_solar.getCurrent_mA();
                DPRINT("solar_current:");
                DPRINTLN(solar_current);
                solar_current = 0.001 * solar_current; // convert to Amps instead of mA
                battery_voltage = ina219_battery.getBusVoltage_V();
                DPRINT("battery_voltage:");
                DPRINTLN(battery_voltage);
                delay(10);
                if ((solar_voltage >= 0) && (solar_voltage < 20.0)) {
                    setup_wifi();
                    setup_mqtt();
                    delay(10);
                    mqtt.publish(VSOLAR_TOPIC, String(solar_voltage).c_str());
                }
                if ((solar_current >= -0.10) && (solar_current < 2000.0)) {
                    setup_wifi();
                    setup_mqtt();
                    if (solar_current < 0) {
                        solar_current = 0;
                    }
                    char solar_currant_str[5];
                    dtostrf(solar_current, 5, 3, solar_currant_str);
                    delay(10);
                    mqtt.publish(ISOLAR_TOPIC, solar_currant_str);
                    //mqtt.publish(ISOLAR_TOPIC, String(solar_current).c_str());
                }
                if ((battery_voltage >= 0) && (battery_voltage < 20.0)) {
                    setup_wifi();
                    setup_mqtt();
                    delay(10);
                    mqtt.publish(VBAT_TOPIC, String(battery_voltage).c_str());
                }

                if ((windSpeed >= 0) && (windSpeed <= 1) && (windDir >= 0) && (windDir <= 360)) {
                    setup_wifi();
                    setup_mqtt();
                    delay(10);
                    mqtt.publish(WINDSPEED_TOPIC, String(windSpeed).c_str());
                    delay(10);
                    mqtt.publish(WINDDIR_TOPIC, String(windDir).c_str());
                    if ((windGustMax >= windSpeed) && (windGustMax >= 0) && (windGustMax <= 10) && (windGustMaxDir >= 0) && (windGustMaxDir <= 360)) {
                        delay(10);
                        mqtt.publish(WINDGUST_TOPIC, String(windGustMax).c_str());
                        delay(10);
                        mqtt.publish(WINDGUSTDIR_TOPIC, String(windGustMaxDir).c_str());
                    }
                    //    } // end if (((minute() % STILL_ALIVE)
                }
                windGustMax = -1;
                windGustMaxDir = -1;
            } // end if (Tindex == 0)
            if (mqtt.connected()) {
                //setup_wifi();
                //setup_mqtt();
                delay(10);
                mqtt.publish(STATUS_TOPIC, "Offline!");
                delay(10);
                mqtt.publish(STATUS_TOPIC, "Check_OTA!");
                mqtt.disconnect();
                delay(20);
                check_OTA();
            }
        } //if (ready == 0)
        Tindex++;
        if (Tindex == RATIO) {
            Tindex = 0;
            prev_pulsecount = counter.getCount(); // check counter in case a few pulse was counted since wakeup
            DPRINT("getCount()=");
            DPRINT(prev_pulsecount - pulsecount);
            counter.setCount(prev_pulsecount - pulsecount);
            // reset counter, initiale with the pulses that occured since wake (should be small but let's be accurate)
            DPRINT("   setCount()=");
            DPRINTLN(counter.getCount());
            prev_pulsecount = 0;
            pulsecount = 0;
        }

    } // if All_is_fine == 0

    /*********** something goes wrong, let's clean up and re-init *****************/
    if (All_is_fine > 0) {
        DPRINTLN("*************************************************************");
        DPRINT(" Errors or power on occured, code:");
        DPRINTLN(All_is_fine);
        DPRINTLN("RE-INIT on going !");
        digitalWrite(led, HIGH);
        setTime(00, 00, 00, 13, 4, 2019); // set time to 0 on DS3232M, we don't care about day or hours, only minutes & seconds
        RTC.set(now());
        counter.setMode(MODE_EVENT_COUNTER); // will set non zero value in register 0x00, so if no POR occured at next loop, register will not be cleared
        counter.setCount(0); // reset anemometer pulse counter
        RTC.squareWave(SQWAVE_NONE);

        // clear variables
        for (byte i = 0; i < RATIO; i = i + 1) {
            Table_pulsecount[i] = 0;
            Table_windDir[i] = 0;
        }
        t = now();
        Tindex = 0;
        ready = RATIO;
        pulsecount = 0;
        prev_pulsecount = 0;
        total_pulsecount = 0;
        windGustDir = -1;
        windGustMaxDir = -1;
        windGust = -1;
        windGustMax = -1;
        windSpeed = -1;
        windDir = -1;
        nextwake = TSAMPLE;

        DPRINTLN("RE-INIT done !");
        delay(1000);
        setup_wifi();
        delay(1000);
        check_OTA();
        DPRINTLN("*************************************************************");
        setup_mqtt();
        mqtt.publish(STATUS_TOPIC, "Re-init done!");
        mqtt.publish(ERROR_TOPIC, String(All_is_fine).c_str());
        //delay(800);
        digitalWrite(led, LOW);
        delay(100);
        digitalWrite(led, HIGH);
        delay(100);
        digitalWrite(led, LOW);
        delay(100);
        digitalWrite(led, HIGH);
        delay(100);
        digitalWrite(led, LOW);
        All_is_fine = 0;
        DPRINTLN("Let's emit Voltage/Current at init");
        solar_voltage = ina219_solar.getBusVoltage_V();
        DPRINT("solar_voltage:");
        DPRINTLN(solar_voltage);
        solar_current = ina219_solar.getCurrent_mA();
        DPRINT("solar_current:");
        DPRINTLN(solar_current);
        solar_current = 0.001 * solar_current; // convert to Amps instead of mA
        battery_voltage = ina219_battery.getBusVoltage_V();
        DPRINT("battery_voltage:");
        DPRINTLN(battery_voltage);
        if ((solar_voltage >= 0) && (solar_voltage < 20.0)) {
            setup_wifi();
            setup_mqtt();
            mqtt.publish(VSOLAR_TOPIC, String(solar_voltage).c_str());
            delay(10);
        }
        if ((solar_current >= 0) && (solar_current < 2000.0)) {
            setup_wifi();
            setup_mqtt();
            char solar_currant_str[5];
            dtostrf(solar_current, 5, 3, solar_currant_str);
            mqtt.publish(ISOLAR_TOPIC, solar_currant_str);
            //mqtt.publish(ISOLAR_TOPIC, String(solar_current).c_str());
            delay(10);
        }
        if ((battery_voltage >= 0) && (battery_voltage < 20.0)) {
            setup_wifi();
            setup_mqtt();
            mqtt.publish(VBAT_TOPIC, String(battery_voltage).c_str());
            delay(10);
        }
    } // end if All_is_fine > 0

    // set alarm every Tsample seconds
    if ((second(now())) < (second(t) + TSAMPLE)) { // check if previous processing were not too long. if yes, something goes wrong, let's go for a reset.
        nextwake = TSAMPLE * (second(t) / TSAMPLE) + TSAMPLE;
        if (nextwake == 60)
            nextwake = 0;
        RTC.setAlarm(ALM1_MATCH_SECONDS, nextwake, 0, 0, 1);
        RTC.alarm(ALARM_1);
        RTC.alarmInterrupt(ALARM_1, true);
    } else {
        All_is_fine = 13; // will perform a reset of all stats at next boot.
    }
#ifdef DEBUGMODE
    if (timeStatus() != timeSet) {
        DPRINTLN("Unable to sync with the RTC");
    } else {
        DPRINT("time: ");
        if (hour() < 10)
            DPRINT("0");
        DPRINT(hour());
        DPRINT(":");
        if (minute() < 10)
            DPRINT("0");
        DPRINT(minute());
        DPRINT(":");
        if (second() < 10)
            DPRINT("0");
        DPRINT(second());
        DPRINT(" nextwake at:");
        if (hour() < 10)
            DPRINT("0");
        DPRINT(hour());
        DPRINT(":");
        if (minute() < 10)
            DPRINT("0");
        DPRINT(minute());
        DPRINT(":");
        if (nextwake < 10)
            DPRINT("0");
        DPRINT(nextwake);
    }
#endif
    if (mqtt.connected()) {
        mqtt.publish(STATUS_TOPIC, "Offline!");
        mqtt.disconnect();
    }
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
    }
    esp_sleep_enable_timer_wakeup((TSAMPLE + 5) * 1000000);
    // backup if RTC had a poweron reset or whatever and loose it's config. then internal timer will wake us in 30sec
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0);
    //1 = Low to High, 0 = High to low   DS3232M will wake us if "/INT" is connected to GPIO33  (dont forget it's an opendrain, pullup is mandatory)
    DPRINT("    Going to sleep now ");
    DPRINTLN(millis());
#ifdef DEBUGMODE
    Serial.flush();
#endif

    esp_task_wdt_delete(NULL);  // detach watchdog
    esp_task_wdt_deinit(); // disable watchdog
    esp_deep_sleep_start(); // go to deep sleep !
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
void loop()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// setup_wifi() : connexion to wifi hotspot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void wifi_connect(const char* ssid, const char* password, uint8_t timeout)
{
    uint8_t timeout_wifi = timeout;
    //WiFiMulti.addAP(ssid, password);
    WiFi.begin(ssid, password);
    while ((WiFi.status() != WL_CONNECTED) && (timeout_wifi > 0)) {

        delay(1000);
        DPRINT(".");
        WiFi.begin(ssid, password);
        //WiFiMulti.addAP(ssid, password);
        timeout_wifi--;
    }
}
//Connexion au réseau WiFi
void setup_wifi()
{
    //wifi_scan();
    //config static IP
    WiFi.mode(WIFI_STA);
    WiFi.config(ip, gateway, subnet);
#ifdef DEBUGMODE //Macros are usually in all capital letters.
    DPRINT("Wifi status: ");
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
    DPRINT(" ");
#endif
    if (WiFi.status() == WL_CONNECTED) {
        DPRINTLN("Wifi already connected");
        return;
    }

    // try 1st SSID (if you have 2 hotspots)
    DPRINT("Try to connect 1st wifi:");
    wifi_connect(ssid1, password1, 10);
    // if connexion is successful, let's go to next, no need for SSID2
    if (WiFi.status() == WL_CONNECTED) {
        DPRINTLN("Wifi ssid1 connected");
        return;
    } else {
#ifdef DEBUGMODE //Macros are usually in all capital letters.
        DPRINTLN("Wifi ssid1 FAILED");
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
#endif
        DPRINTLN("Let's try connecting 2nd wifi SSID");
    }
    // let's try SSID2 (if ssid1 did not worked)
    wifi_connect(ssid2, password2, 10);
    if (WiFi.status() == WL_CONNECTED) {
        DPRINTLN("Wifi ssid2 connected");
        return; // if connexion is successful, let's go to next, no need for SSID2
    } else {
#ifdef DEBUGMODE //Macros are usually in all capital letters.
        DPRINTLN("Wifi ssid2 FAILED");
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
#endif
        DPRINTLN("Failed to connect wifi, let's sleep and retry later...");
        WiFi.disconnect();
#ifdef DEBUGMODE
        Serial.flush();
#endif
        delay(50);
        esp_sleep_enable_timer_wakeup(300 * 1000000); //300 seconds
        esp_task_wdt_delete(NULL);  // detach watchdog
        esp_task_wdt_deinit(); // disable watchdog
        esp_deep_sleep_start();
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
#ifdef DEBUGMODE //Macros are usually in all capital letters.
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
#endif
        retries--;
        if (retries < 1) {
            DPRINTLN("Failed to connect MQTT broker, will restart in 10min...");
            WiFi.disconnect();
#ifdef DEBUGMODE
            Serial.flush();
#endif
            delay(50);
            esp_sleep_enable_timer_wakeup(300 * 1000000); //300 seconds
            esp_task_wdt_delete(NULL);  // detach watchdog
            esp_task_wdt_deinit(); // disable watchdog
            esp_deep_sleep_start();
        }
    }
    mqtt.publish(STATUS_TOPIC, "Online!");
    mqtt.publish(VERSION_TOPIC, String(FW_VERSION).c_str());
    //mqtt.subscribe(CONFIG_TOPIC);
}

void report_wake_source()
{
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
        DPRINTLN("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        DPRINTLN("Wakeup caused by external signal using RTC_CNTL");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        DPRINTLN("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        DPRINTLN("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        DPRINTLN("Wakeup caused by ULP program");
        break;
        //default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
    }
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
        DPRINT("http begin | ");
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
                t_httpUpdate_return ret = httpUpdate.update(client, "http://192.168.1.184/fota/Wind/firmware.bin");
                switch (ret) {
                case HTTP_UPDATE_FAILED:
                    DPRINT("HTTP_UPDATE_FAILD Error");
                    DPRINT(httpUpdate.getLastError());
                    DPRINT(": ");
                    DPRINTLN(httpUpdate.getLastErrorString().c_str());
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

void wifi_scan()
{
#ifdef DEBUGMODE //Macros are usually in all capital letters.
    Serial.println("scan start");
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
            delay(10);
        }
    }
    Serial.println("");
#endif
}