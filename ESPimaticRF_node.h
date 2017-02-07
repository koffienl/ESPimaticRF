// Global variables used for 'node'
int BWmode;                                                 // What BWmode is used
String bwlistjson;                                          // In-memory copy bwlist.json
String LastReceived;                                        // Holds the last UDP received device
String PrevRcv;                                             // Holds the last RF received command
int RcvTime;                                                // Holds the last RF received command time
String apikey;                                              // Holds the Pimatic API key
String pimaticIP;                                           // Holds the Pimatic address
long int pimaticPort;                                       // Holds the Pimatic port
char incomingPacket[355];                                   // buffer for incoming UDP packets

// Create a UDP instance for receiving UDP multicast packets to node(s)from homeduino
WiFiUDP UdpMulti;

void callback(char* topic, byte* payload, unsigned int length)
{
  String incomingPacket;
  for (int i = 0; i < length; i++) {
    incomingPacket = incomingPacket + (char)payload[i] ;
  }

  //String strippedJson = String(incomingPacket).substring(8, incoming.length());
  //Serial.println("Stripped: " + strippedJson);
  //PrevHash = String(incomingPacket).substring(0, 8);
  DynamicJsonBuffer BufferSetup;
  JsonObject& rf = BufferSetup.parseObject(incomingPacket);

  if (rf.success())
  {
    JsonObject& bckets = rf["buckets"];

    // read pulse lengths
    unsigned long buckets[8];
    for (unsigned int i = 0; i < 8; i++)
    {
      buckets[i] = strtoul(bckets[String(i)], NULL, 10);
    }

    int repeats = rf["repeats"];
    String pulse = rf["pulse"].asString();
    String protocol = rf["protocol"].asString();
    String unit = rf["unit"].asString();
    String id = rf["id"].asString();

    Serial.println("Receive message from homeduino, protocol:" + protocol + " , unit:" + unit + " , id:" + id);

    if (BWmode == 1)
    {
      // BWmode 1 = Allow everything except blacklist
      String BWCopy = bwlistjson;
      DynamicJsonBuffer bwbuffer;
      JsonObject& bw = bwbuffer.parseObject(const_cast<char*>(BWCopy.c_str()));

      if ( String(bw[protocol][id][unit].asString()) != "blacklisted")
      {
        char pls[pulse.length() + 1];
        pulse.toCharArray(pls, pulse.length() + 1);
        RFControl::sendByCompressedTimings(transmitterPin.toInt(), buckets, pls, repeats);
        //LastReceived = String(incomingPacket).substring(8, incoming.length());
        //LastReceived = incomingPacket;
      }
      else
      {
        Serial.println("This protocol/ID/Unit is blacklisted, do nothing");
      }
    }

    if (BWmode == 2)
    {
      // BWmode 2 = Allow nothing except whitelist
      String BWCopy = bwlistjson;
      DynamicJsonBuffer bwbuffer;
      JsonObject& bw = bwbuffer.parseObject(const_cast<char*>(BWCopy.c_str()));

      if ( String(bw[protocol][id][unit].asString()) == "whitelisted")
      {
        char pls[pulse.length() + 1];
        pulse.toCharArray(pls, pulse.length() + 1);
        RFControl::sendByCompressedTimings(transmitterPin.toInt(), buckets, pls, repeats);
      }
      else
      {
        Serial.println("This protocol/ID/Unit is not in the whitelist, do nothing");
        //LastReceived = String(incomingPacket).substring(8, incoming.length());
        //LastReceived = incomingPacket;
      }
    }

    if (BWmode == 0)
    {
      // BWmode 0 = Allow everything
      char pls[pulse.length() + 1];
      pulse.toCharArray(pls, pulse.length() + 1);
      RFControl::sendByCompressedTimings(transmitterPin.toInt(), buckets, pls, repeats);
      //LastReceived = String(incomingPacket).substring(8, incoming.length());
      //LastReceived = incomingPacket;
    }
  }
  else
  {
    Serial.println("problem with incoming json?");
  }



}


void udpLoop()
{
  int packetSize = UdpMulti.parsePacket();
  if (packetSize)
  {
    // receive incoming UDP packets
    //Serial.println("Received " + String(packetSize) + " bytes from " + String(UdpMulti.remoteIP().toString().c_str()) + " , port " + String(UdpMulti.remotePort()));
    int len = UdpMulti.read(incomingPacket, 355);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }
    if (String(incomingPacket).substring(0, 8) != PrevHash )
    {
      String strippedJson = String(incomingPacket).substring(8, packetSize);
      PrevHash = String(incomingPacket).substring(0, 8);
      DynamicJsonBuffer BufferSetup;
      JsonObject& rf = BufferSetup.parseObject(strippedJson);

      if (rf.success())
      {
        JsonObject& bckets = rf["buckets"];

        // read pulse lengths
        unsigned long buckets[8];
        for (unsigned int i = 0; i < 8; i++)
        {
          buckets[i] = strtoul(bckets[String(i)], NULL, 10);
        }

        int repeats = rf["repeats"];
        String pulse = rf["pulse"].asString();
        String protocol = rf["protocol"].asString();
        String unit = rf["unit"].asString();
        String id = rf["id"].asString();

        Serial.println("Receive message from homeduino, protocol:" + protocol + " , unit:" + unit + " , id:" + id);

        if (BWmode == 1)
        {
          // BWmode 1 = Allow everything except blacklist
          String BWCopy = bwlistjson;
          DynamicJsonBuffer bwbuffer;
          JsonObject& bw = bwbuffer.parseObject(const_cast<char*>(BWCopy.c_str()));

          if ( String(bw[protocol][id][unit].asString()) != "blacklisted")
          {
            char pls[pulse.length() + 1];
            pulse.toCharArray(pls, pulse.length() + 1);
            RFControl::sendByCompressedTimings(transmitterPin.toInt(), buckets, pls, repeats);
            LastReceived = String(incomingPacket).substring(8, packetSize);
            //LastReceived = incomingPacket;
          }
          else
          {
            Serial.println("This protocol/ID/Unit is blacklisted, do nothing");
          }
        }

        if (BWmode == 2)
        {
          // BWmode 2 = Allow nothing except whitelist
          String BWCopy = bwlistjson;
          DynamicJsonBuffer bwbuffer;
          JsonObject& bw = bwbuffer.parseObject(const_cast<char*>(BWCopy.c_str()));

          if ( String(bw[protocol][id][unit].asString()) == "whitelisted")
          {
            char pls[pulse.length() + 1];
            pulse.toCharArray(pls, pulse.length() + 1);
            RFControl::sendByCompressedTimings(transmitterPin.toInt(), buckets, pls, repeats);
          }
          else
          {
            Serial.println("This protocol/ID/Unit is not in the whitelist, do nothing");
            LastReceived = String(incomingPacket).substring(8, packetSize);
            //LastReceived = incomingPacket;
          }
        }

        if (BWmode == 0)
        {
          // BWmode 0 = Allow everything
          char pls[pulse.length() + 1];
          pulse.toCharArray(pls, pulse.length() + 1);
          RFControl::sendByCompressedTimings(transmitterPin.toInt(), buckets, pls, repeats);
          LastReceived = String(incomingPacket).substring(8, packetSize);
          //LastReceived = incomingPacket;
        }
      }
      else
      {
        Serial.println("problem with incoming json?");
      }
    }
    else
    {
      Serial.println("Receive message from homeduino, ignore (duplicate)");
    }

  }
}

void handle_udp(String incomingPacket, int packetSize)
{

}



void rfcontrolNode_loop()
{
  //  if (RFControl::hasData()) {
  unsigned int *timings;
  unsigned int timings_size;
  RFControl::getRaw(&timings, &timings_size);
  unsigned int buckets[8];
  unsigned int pulse_length_divider = RFControl::getPulseLengthDivider();
  RFControl::compressTimings(buckets, timings, timings_size);

  int BucketCount = 1;

  String bucketsStr = "[";
  for (unsigned int i = 0; i < 8; i++) {
    unsigned long bucket = buckets[i] * pulse_length_divider;
    if (bucket == 0) {
      break;
    }
    if (i != 0) {
      bucketsStr += ",";
      BucketCount++;
    }
    bucketsStr += bucket;


  }
  bucketsStr += "]";
  //BucketCount--;

  String pulsesStr;
  for (unsigned int i = 0; i < timings_size; i++) {
    pulsesStr += timings[i];
  }
  RFControl::continueReceiving();

  if ((bucketsStr + pulsesStr) == PrevRcv && (millis() - RcvTime) <= 1500)    // If captured data is not the same as previous captured data AND previous captured data is not older than 1000 ms
  {
    Serial.println("duplicate RF signal detected, not sending to Pimatic");
  }
  else
  {
    Serial.println("RF signal detected, send to Pimatic");
    String url = "/homeduino/received?apikey=" + String(apikey) + "&buckets=" + bucketsStr + "&pulses=" + pulsesStr;
    WiFiClient client;
    if (!client.connect(pimaticIP.c_str(), pimaticPort))
    {
      Serial.println("connection failed");
    }
    else
    {
      // This will send the request to the server
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: 192.168.2.118\r\n" +
                   "Connection: close\r\n\r\n");
      delay(10);
    }
  }

  PrevRcv = bucketsStr + pulsesStr;
  RcvTime = millis();
}

