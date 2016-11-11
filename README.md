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

Go to http://esp_ip , choose config from menu and enter all the options

MUST edit to work:

* mode (can be 'homeduino' for attached to Raspberry Pi or 'node' when standalone)
* receiverPin (can be left nu none for homdeuino mode, will follow pin number in pimatic)
* transmitterPin (can be left nu none for homdeuino mode, will follow pin number in pimatic)
* pimaticIP
* pimaticPort
* apikey (also, make sure you have the apikey entered in the pimatic config for homeduino plugin)
* receiveAction
* transmitAction

receiveAction in 'node' mode:
* receiveAction = 0 [don't do anything with protocol received by local RF]
* receiveAction = 1 [send protocol received by local RF to pimatic]

transmitAction in 'node' mode:
* transmitAction = 0 [don't do anything with protocol received by UDP]
* transmitAction = 1 [transmit protocol received by UDP to local RF]


Currently send/receive is only available on the node (beta, you know).
