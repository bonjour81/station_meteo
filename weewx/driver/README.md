To be added in your /etc/weewx/weewx.conf:

    [wxMesh]
        driver = user.wxMesh

        # MQTT specifics
        host = your_mqtt_broker_IP     # ex 192.168.1.xx
        client = weewx_mqtt            # just a client name for mqtt, must be unique!
        username = mqtt_username       # your mqtt username
        password = mqtt_password       # your mqtt password
        topic = yourtopic              # your mqtt topic root, your sensor will publish as "yourtopic/outTemp" etc
        poll_interval = 2              # the driver will process the receiver data every poll_interval (seconds)

        [[label_map]]

            # mqtt topic = weewx variable
            barometer = barometer
            pressure = pressure
            altimeter = altimeter
            inTemp = inTemp
            outTemp = outTemp
            inHumidity = inHumidity
            outHumidity = outHumidity
            windSpeed = windSpeed
            windDir = windDir
            windGust = windGust
            windGustDir = windGustDir
            rainRate = rainRate
            rain = rain
            dewpoint = dewpoint
            windchill = windchill
            heatindex = heatindex
            ET = ET
            radiation = radiation
            UV = UV
            extraTemp1 = extraTemp1
            extraTemp2 = extraTemp2
            extraTemp3 = extraTemp3
            -----

The driver will take any data (number) published to yourtopic/yourlabel.
Example :

if yourtopic is 'weewx', outTemp shall be avaible in topic  weewx/outTemp  with payload 23.2  (if out temperature is 23.2°C or 23.2°F  (follows your weewx units).
