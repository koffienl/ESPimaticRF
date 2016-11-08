# ESPimaticRF
Highly beta, use only when you got instructions!

Use Arduino IDE 1.6.5
Use Arduino ESP8266 2.3.0
Compile & upload sketch to ESP with enough SPIFFS
After first wait finish format SPIFFS
Connecto to Access Point ESPimaticRF (password espimaticrf)
Go to http://192.168.4.1
Fill in local wifi credentials and save
Wait for restart
Grab assigned DHCP IP address from Serial Monitor
Go to http://esp_ip
Upload all the files that are in 'SPIFFS' folder
Reboot ESP
Go to http://esp_ip/edit.html and point to config.json
Edit config.json as follow:

{
  "settings": {
    "wifi": {
      "ssid": "your SSID",
      "password": "your wifi pass"
    },
    "ESPimaticRF": {
      "mode": "node",
      "receiverPin": "5",
      "transmitterPin": "4",
      "receiveAction": "",
      "TransmitAction": "",
      "pimaticIP": "192.168.2.118",
      "pimaticPort": "82",
      "apikey": "x"
    }
  }
}

Makre sure to edit:
mode (can be 'homeduino' for attached to Raspberry Pi or 'node' when standalone)
receiverPin
transmitterPin
pimaticIP
pimaticPort
apikey (also, make sure you have the apikey entered in the pimatic config for homeduino plugin)

Currently send/receive is only available on the node (beta, you know).
