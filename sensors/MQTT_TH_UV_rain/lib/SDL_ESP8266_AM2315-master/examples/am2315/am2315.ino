#include <Wire.h>
#include <Adafruit_AM2315.h>

/*************************************************** 
  This is an example for the AM2315 Humidity + Temp sensor


  SwitchDoc Labs Modifications for ESP8266
  This is a library for the AM2315 Humidity & Temp Sensor
  www.switchdoc.com

  This works for both the SwitchDoc Labs AM2315 and the Adafruit AM2315


  Designed specifically to work with the Adafruit BMP085 Breakout 
  ----> https://www.adafruit.com/products/1293

  These displays use I2C to communicate, 2 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

// Connect RED of the AM2315 sensor to 5.0V
// Connect BLACK to Ground
// Connect WHITE to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5
// Connect YELLOW to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4

Adafruit_AM2315 am2315;

void setup() {
  Serial.begin(9600);
  Serial.println("AM2315 Test!");

  if (! am2315.begin()) {
     Serial.println("Sensor not found, check wiring & pullups!");
     // while (1);  // this bombs the latest version of ESP8266 software
     while (1)
	{
		yield();
		delay(10);
	}
  }
}

void loop() {
  Serial.print("Hum: "); Serial.println(am2315.readHumidity());
  Serial.print("Temp: "); Serial.println(am2315.readTemperature());

  delay(1000);
}}
