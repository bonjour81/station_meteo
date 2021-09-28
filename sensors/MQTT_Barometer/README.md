MS5611 to MQTT python script

This script read pressure and temperature from a MS5611 sensor connected to the **OrangePI** I2C bus. Value are published on MQTT (mosquitto).
Current script use basic altitude compensation to calculate barometer value. More evolution may come to use more accurate calculations.

You can execute this scrip every 5 minutes by adding a line in /etc/crontab:

*/5 * * * * username /update-your-path/ms5611

Despite it's a python script, it looks like crontab does not like file xxx.py...
better to keep the filename with no extensions.


Dont forget to:
* make the script executable (chmod +x ms5611)
* update values in the script:
line 12-13 : mqtt username and password
line 107: the altitude of your sensor in meter.
line 119: the IP address of your mosquitto broker


May work on raspberry pi too (not tested).

[See wiki for more technical detail](https://github.com/bonjour81/station_meteo/wiki/MS5611-Barometer-by-MQTT-on-OrangePi) about sensor characteristics and how to use it.


---

The following modification was reported by @gdlesage to work on **Raspberry pi 2**:

mqttuser = "user_name" #double quotes instead of single quotes

mqttpass = "password" #double quotes instead of single quotes

bus = smbus.SMBus(1) #1 instead of 0  (work on RPi2 but may vary on other Raspberry type)

mqttc.username_pw_set(mqttuser,mqttpass) #no quotes around mqttuser or mqttpass

https://github.com/bonjour81/station_meteo/issues/5#issue-952261133
