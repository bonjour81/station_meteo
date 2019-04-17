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

#include "Adafruit_AM2315.h"
//#include <util/delay.h>

Adafruit_AM2315::Adafruit_AM2315() {
}


boolean Adafruit_AM2315::begin(void) {
  //Wire.begin(5, 4);
  Wire.begin(4, 5);          // modified compared to original library

  // try to read data, as a test
  return readData();

}

boolean Adafruit_AM2315::readData(void) {
  uint8_t reply[10];

  Wire.beginTransmission(AM2315_I2CADDR);
  Wire.write(AM2315_READREG);
  Wire.write(0x00);  // start at address 0x0
  Wire.write(4);  // request 4 bytes data
  Wire.endTransmission();
  delay(50);

  // for reasons unknown we have to send the data twice :/
  // whats the bug here?
  Wire.beginTransmission(AM2315_I2CADDR);
  Wire.write(AM2315_READREG);
  Wire.write(0x00);  // start at address 0x0
  Wire.write(4);  // request 4 bytes data
  Wire.endTransmission();

  delay(50);
  Wire.requestFrom(AM2315_I2CADDR, 8);
  for (uint8_t i = 0; i < 8; i++) {
    reply[i] = Wire.read();
    //Serial.println(reply[i], HEX);
  }

  if (reply[0] != AM2315_READREG) return false;
  if (reply[1] != 4) return false; // bytes req'd

  humidity = reply[2];
  humidity *= 256;
  humidity += reply[3];
  humidity /= 10;
  //Serial.print("H"); Serial.println(humidity);



  temp = reply[4] & 0x7F;
  temp *= 256;
  temp += reply[5];
  temp /= 10;
  //Serial.print("T"); Serial.println(temp);

  // change sign
  if (reply[4] >> 7) temp = -temp;



  //if (reply[4] >> 7) temp = -temp;

  return true;
}


float Adafruit_AM2315::readTemperature(void) {
  if (! readData()) return NAN;
  return temp;
}

float Adafruit_AM2315::readHumidity(void) {
  if (! readData()) return NAN;
  return humidity;
}

/*********************************************************************/
