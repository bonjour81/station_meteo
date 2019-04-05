# station_meteo

This project is a full weather station.
It rely on [WeeWX](http://weewx.com/) to process the data and generate database, archive, graphics and web output

The weather station hardware is based on ESP8266 and other arduino stuff.
Related code is in the [sensors](./sensors) directory.

The datatransmission from sensors to [WeeWX](http://weewx.com/) is based on MQTT. So a specifi MQTT driver is needed for weewx.
It's available in the [weewx section](./weewx).


[See wiki for details](https://github.com/bonjour81/station_meteo/wiki)
