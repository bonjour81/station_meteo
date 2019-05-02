/***************************************************
This is a modified library from SwitchDoc Labs / Adafruit from 17/04/2019

I have done the following modifications:
1/ twisted scl/sda pins in:    Wire.begin(4, 5);

2/ add negative temp handling
****************************************************/

/***************************************************
  SwitchDoc Labs Modifications for ESP8266
  This is a library for the AM2315 Humidity & Temp Sensor
  www.switchdoc.com

  This works for both the SwitchDoc Labs AM2315 and the Adafruit AM2315



  This is a library for the AM2315 Humidity Pressure & Temp Sensor

  Designed specifically to work with the AM2315 sensor from Adafruit
  ----> https://www.adafruit.com/products/1293

  These displays use I2C to communicate, 2 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#if (ARDUINO >= 100)
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif
#include "Wire.h"

#define AM2315_I2CADDR       0x5C
#define AM2315_READREG       0x03

class Adafruit_AM2315 {
 public:
  Adafruit_AM2315();
  boolean begin(void);
  float readTemperature(void);
  float readHumidity(void);
  bool readTemperatureAndHumidity(float &t, float &h);

 private:
  boolean readData(void);
  float humidity, temp;
};
