See the Wiki for help on installing.
This version supports both MQTT and UDP for communication between the ESP’s.

CAUTION : This version needs the librarie pubsubclient (included in the download). If you allready have it installed or don’t want to use the included librarie be aware you MUST edit pubsubclient\src\PubSubClient.h and set MQTT_MAX_PACKET_SIZE to 512 (line 26).
The included librarie allready has this change.

I’m quite new to MQTT but so far everything looks fine to me. Only tested with mosquitto on debian.
Ofcourse all the ESP’s in the ESPimaticRF chain should use the same meganism : either MQTT or UDP you can’t mix the protocols.

The ESP will connect to the MQTT broker on startup. Every 1 minute the connection is check and retried / reestablished when the connection is dropped.