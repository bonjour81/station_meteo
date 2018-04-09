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

