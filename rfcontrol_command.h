#include <RFControl.h>


int interrupt_pin = -1;

void rfcontrol_command_send();
void rfcontrol_command_receive();
void rfcontrol_command_raw();

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


bool in_raw_mode = false;

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


  Serial.println("buckets is: " + String(BucketCount) );
  //brands: ["Eurodomest"],
  //pulseLengths: [295, 886, 9626],




  Serial.println(bucketsStr + pulsesStr);

  if ((bucketsStr + pulsesStr) == PrevRcv && (millis() - RcvTime) <= 1500)    // If captured data is not the same as previous captured data AND previous captured data is not older than 1000 ms
  {
    Serial.println("Ik ontvang RF uit de lucht, zelfde als hiervoor! timerverschil : " + String(millis() - RcvTime));
  }
  else
  {
    Serial.println("Ik ontvang RF uit de lucht, doorsturen!");
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
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  int interrupt_pin = atoi(arg);
  RFControl::startReceiving(interrupt_pin);
  in_raw_mode = true;
  Serial.print("ACK\r\n");
}

void rfcontrol_command_receive() {
  char* arg = sCmd.next();
  if (arg == NULL) {
    argument_error();
    return;
  }
  interrupt_pin = atoi(arg);
  RFControl::startReceiving(interrupt_pin);
  in_raw_mode = false;
  Serial.print("ACK\r\n");
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

  int hash = random(10000000, 99999999);

  int len = root.measureLength() + 1;
  char ch[len];
  size_t n = root.printTo(ch, sizeof(ch));
  String tt(ch);

  tt = String(hash).substring(0, 8) + tt;

  if (transmitAction == 1 || transmitAction == 3)
  {
    Serial.println(tt);
    send_udp(tt);
  }
  in_raw_mode = false;
}

