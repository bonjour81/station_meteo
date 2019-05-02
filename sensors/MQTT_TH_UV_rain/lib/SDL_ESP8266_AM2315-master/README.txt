  SwitchDoc Labs Modifications for ESP8266  
  This is a library for the AM2315 Humidity & Temp Sensor
  www.switchdoc.com

  This works for both the SwitchDoc Labs AM2315 and the Adafruit AM2315

  Note:  There are ESP8266-12 modules that have pins 4 and 5 reversed.   The one we are testing has these lines reversed, hence the statement:

  Wire.begin(5,4);

  If you have the lines in your ESP8266 correct, then change this to Wire.begin(4,5)

This is a library for the AM2315 Humidity + Temp sensor

Designed specifically to work with the AM2315 in the Adafruit shop 
  ----> https://www.adafruit.com/products/1293

These displays use I2C to communicate, 2 pins are required to interface
Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Limor Fried/Ladyada for Adafruit Industries.  
BSD license, all text above must be included in any redistribution

Check out the links above for our tutorials and wiring diagrams 

To download. click the ZIP button in the top-middle navbar, 
rename the uncompressed folder Adafruit_AM2315. 
Check that the Adafruit_AM2315 folder contains Adafruit_AM2315.cpp and Adafruit_AM2315.h

Place the Adafruit_AM2315 library folder your arduinosketchfolder/libraries/ folder. 
You may need to create the libraries subfolder if its your first library. Restart the IDE.

We also have a great tutorial on Arduino library installation at:
http://learn.adafruit.com/adafruit-all-about-arduino-libraries-install-use
