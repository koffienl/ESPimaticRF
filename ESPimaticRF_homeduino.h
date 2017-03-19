// Global variables used for 'homeduino'
String PrevHash = "";                                       // Stores the hash from the last received UDP packet
int UDPrepeat;                                              // Number of UDP packets to send
String protocolsjson;                                       // In-memory copy protocols.json
unsigned int localUdpPort;                                  // Local UDP port for listening
IPAddress ipMulti(239, 0, 0, 57);                           // UDP multicast IP address
unsigned int portMulti = 12345;                             // Local UDP port for listening
bool in_raw_mode = false;                                   // For RAW mode

// Create a UDP instance for sending UDP packets to node(s)
WiFiUDP Udp;

// Predefine some functions
void rfcontrol_command_send();
void rfcontrol_command_receive();
void rfcontrol_command_raw();
void digital_read_command();
void digital_write_command();
void analog_read_command();
void analog_write_command();
void reset_command();
void pin_mode_command();
void ping_command();
void unrecognized(const char *command);
void argument_error();

void digital_read_command() {
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int pin = atoi(arg);
  int val = digitalRead(pin);
  Serial.print("ACK ");
  Serial.write('0' + val);
  Serial.print("\r\n");
}

void analog_read_command() {
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int pin = atoi(arg);
  int val = analogRead(pin);
  Serial.print("ACK ");
  Serial.print(val);
  Serial.print("\r\n");
}

void digital_write_command() {
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int pin = atoi(arg);
  arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int val = atoi(arg);
  digitalWrite(pin, val);
  Serial.print("ACK\r\n");
}

void analog_write_command() {
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int pin = atoi(arg);
  arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int val = atoi(arg);
  analogWrite(pin, val);
  Serial.print("ACK\r\n");
}

void pin_mode_command() {
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int pin = atoi(arg);
  arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  // INPUT 0x0
  // OUTPUT 0x1
  int mode = atoi(arg);

  //pinMode(pin, mode);
  pinMode(5, mode);



  Serial.print("ACK\r\n");
}


void ping_command() {
  char *arg;
  Serial.print("PING");
  arg = sCmd.next();
  if (arg != NULL) {
    Serial.write(' ');
    Serial.print(arg);
  }
  Serial.print("\r\n");
}


void reset_command() {
  RFControl::stopReceiving();
  Serial.print("ready\r\n");
}

void argument_error() {
  Serial.print("ERR argument_error\r\n");
}
// This gets set as the default handler, and gets called when no other command matches.
void unrecognized(const char *command) {
  Serial.print("ERR unknown_command\r\n");
}



// convert a binary string to a decimal number, returns decimal value
int bin2dec(char *bin)
{
  int  b, k, m, n;
  int  len, sum = 0;

  len = strlen(bin) - 1;
  for (k = 0; k <= len; k++)
  {
    n = (bin[k] - '0'); // char to numeric value
    if ((n > 1) || (n < 0))
    {
      puts("\n\n ERROR! BINARY has only 1 and 0!\n");
      return (0);
    }
    for (b = 1, m = len; m > k; m--)
    {
      // 1 2 4 8 16 32 64 ... place-values, reversed here
      b *= 2;
    }
    // sum it up
    sum = sum + n * b;
    //printf("%d*%d + ",n,b);  // uncomment to show the way this works
  }
  return (sum);
}

void rfcontrol_loop() {
  if (in_raw_mode) {
    if (RFControl::existNewDuration()) {
      Serial.print(String(RFControl::getLastDuration() * RFControl::getPulseLengthDivider()));
      Serial.print(", ");
      static byte line = 0;
      line++;
      if (line >= 16) {
        line = 0;
        Serial.write('\n');
      }
    }
  }
  else {
    if (RFControl::hasData() && receiveAction == 1) {
      unsigned int *timings;
      unsigned int timings_size;
      RFControl::getRaw(&timings, &timings_size);
      unsigned int buckets[8];
      unsigned int pulse_length_divider = RFControl::getPulseLengthDivider();
      RFControl::compressTimings(buckets, timings, timings_size);
      Serial.print("RF receive ");
      for (unsigned int i = 0; i < 8; i++) {
        unsigned long bucket = buckets[i] * pulse_length_divider;
        Serial.print(String(bucket));
        Serial.write(' ');
      }
      for (unsigned int i = 0; i < timings_size; i++) {
        Serial.write('0' + timings[i]);
      }
      Serial.print("\r\n");
      RFControl::continueReceiving();
    }
  }
}

void rfcontrol_command() {
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  if (strcmp(arg, "send") == 0) {
    rfcontrol_command_send();
  } else if (strcmp(arg, "receive") == 0) {
    rfcontrol_command_receive();
  } else if (strcmp(arg, "raw") == 0) {
    rfcontrol_command_raw();
  } else {
    argument_error();
  }
}

void rfcontrol_command_raw() {
  /*
    char* arg = sCmd.next();
    if (arg == NULL) {
    argument_error();
    return;
    }
    int interrupt_pin = atoi(arg);
  */
  RFControl::startReceiving(receiverPin.toInt());
  in_raw_mode = true;
  Serial.print("ACK\r\n");
}

void rfcontrol_command_receive() {
  /*
    char* arg = sCmd.next();
    if (arg == NULL) {
    argument_error();
    return;
    }
    interrupt_pin = atoi(arg);
  */
  RFControl::startReceiving(receiverPin.toInt());
  in_raw_mode = false;
  Serial.print("ACK\r\n");
}

void send_udp(String data)
{
  char pls[data.length() + 1];
  data.toCharArray(pls, data.length() + 1);

  RFControl::stopReceiving();

  for (int i = 0; i < UDPrepeat; i++)
  {
    Udp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP());
    Udp.write(pls);
    Udp.endPacket();
    Serial.println("UDP Packet " + String(i) + " done");
    delay(50);
  }
  RFControl::startReceiving(receiverPin.toInt());
}


void rfcontrol_command_send()
{
  DynamicJsonBuffer Bufferjsn;
  JsonObject& root = Bufferjsn.parseObject("{}");
  root.createNestedObject("buckets");
  JsonObject& bckets = root["buckets"];

  String str;
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int transmitter_pin = atoi(arg);

  arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int repeats = atoi(arg);
  root["repeats"] = String(arg);

  // read pulse lengths
  unsigned long buckets[8];
  for (unsigned int i = 0; i < 8; i++) {
    arg = sCmd.next();
    if (arg == NULL) {
      argument_error();
      return;
    }
    buckets[i] = strtoul(arg, NULL, 10);
    bckets[String(i)] = String(arg);
    if (i != 7)
    {
      str += String(arg) + " ";
    }
    else
    {
      str += String(arg);
    }
  }

  //read pulse sequence
  arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  root["pulse"] = String(arg);

  Serial.print("ACK\r\n");

  if (transmitAction >= 2)
  {
    RFControl::sendByCompressedTimings(transmitterPin.toInt(), buckets, arg, repeats);
  }


  String ProtoCopy = protocolsjson;
  String pulses = String(arg);
  DynamicJsonBuffer BufferProtocols;
  JsonObject& protocols = BufferProtocols.parseObject(const_cast<char*>(ProtoCopy.c_str()));

  if (protocols.containsKey(str))
  {
    JsonObject& protoTT = protocols[str];
    root["protocol"] = protoTT["name"].asString();
    JsonObject& removeTT = protoTT["remove"];
    for (JsonObject::iterator it2 = removeTT.begin(); it2 != removeTT.end(); ++it2)
    {
      String tt2 = it2->key;
      pulses.replace(tt2, "");
      pulses.replace(tt2, "");
    }

    int PulsesLen = pulses.length();
    String BinPulse;
    String Bit = protoTT["0"];
    int BitLen = Bit.length();
    for (int i = 0; i <= PulsesLen; i = i + BitLen)
    {
      String conv = pulses.substring(i, i + BitLen);
      if (pulses.substring(i, i + BitLen) == protoTT["1"].asString())
      {
        BinPulse += "1";
      }
      if (pulses.substring(i, i + BitLen) == protoTT["0"].asString())
      {
        BinPulse += "0";
      }
    }

    char unit[BinPulse.substring(protoTT["unitStart"], protoTT["unitEnd"]).length() + 1];
    BinPulse.substring(protoTT["unitStart"], protoTT["unitEnd"]).toCharArray(unit, BinPulse.substring(protoTT["unitStart"], protoTT["unitEnd"]).length() + 1);
    root["unit"] = String(bin2dec(unit));

    char id[BinPulse.substring(protoTT["idStart"], protoTT["idEnd"]).length() + 1];
    BinPulse.substring(protoTT["idStart"], protoTT["idEnd"]).toCharArray(id, BinPulse.substring(protoTT["idStart"], protoTT["idEnd"]).length() + 1);
    root["id"] = String(bin2dec(id));
  }
  else
  {
    Serial.println("protocol niet gevonden?");
    root["protocol"] = "unknown";
    root["id"] = "unknown";
    root["unit"] = "unknown";
  }

  int len = root.measureLength() + 1;
  char ch[len];
  size_t n = root.printTo(ch, sizeof(ch));
  String tt(ch);

  if (transmitAction == 1 || transmitAction == 3)
  {
    if (connectivity == "MQTT")
    {
      Serial.print("Publish to topic: ");
      Serial.println(outTopic);
      char data[len];
      tt.toCharArray(data, (tt.length() + 1));
      client.publish(outTopic, data);
      Serial.println("Message send!");
    }
    if (connectivity == "UDP")
    {
      int hash = random(10000000, 99999999);
      tt = String(hash).substring(0, 8) + tt;
      send_udp(tt);
    }
  }
  in_raw_mode = false;
}

