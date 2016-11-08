#include <RFControl.h>


int interrupt_pin = -1;

void rfcontrol_command_send();
void rfcontrol_command_receive();
void rfcontrol_command_raw();

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

  String bucketsStr = "[";
  for (unsigned int i = 0; i < 8; i++) {
    unsigned long bucket = buckets[i] * pulse_length_divider;
    if (bucket == 0) {
      break;
    }
    if (i != 0) {
      bucketsStr += ",";
    }
    bucketsStr += bucket;

  }
  bucketsStr += "]";
  String pulsesStr;
  for (unsigned int i = 0; i < timings_size; i++) {
    pulsesStr += timings[i];
  }
  RFControl::continueReceiving();
  Serial.println("Ik ontvang RF uit de lucht, doorsturen!");
  String url = "/homeduino/received?apikey=" + String(apikey) + "&buckets=" + bucketsStr + "&pulses=" + pulsesStr;
  WiFiClient client;
  if (!client.connect(pimaticIP.c_str(), pimaticPort)) {
    Serial.println("connection failed");
    return;
  }

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: 192.168.2.118\r\n" +
               "Connection: close\r\n\r\n");
  delay(10);
}

void rfcontrol_loop() {
  if (in_raw_mode) {
    if (RFControl::existNewDuration()) {
      SerialPrint(String(RFControl::getLastDuration() * RFControl::getPulseLengthDivider()));
      SerialPrint(", ");
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
        SerialPrint(String(bucket));
        Serial.write(' ');
      }
      for (unsigned int i = 0; i < timings_size; i++) {
        Serial.write('0' + timings[i]);
      }
      SerialPrint("\r\n");
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
  SerialPrint("ACK\r\n");
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
  SerialPrint("ACK\r\n");
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

  //SerialPrint("ACK\r\n");
  Serial.print("ACK\r\n");

  int len = root.measureLength() + 1;
  char ch[len];
  size_t n = root.printTo(ch, sizeof(ch));
  String tt(ch);


  if (transmitAction >= 2)
  {
    RFControl::sendByCompressedTimings(transmitter_pin, buckets, arg, repeats);
  }

  if (transmitAction == 1 || transmitAction == 3)
  {
    send_udp(tt);
  }
  in_raw_mode = false;
}
