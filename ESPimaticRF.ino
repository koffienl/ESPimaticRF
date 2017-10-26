// Global variables
String receiverPin;                                         // Holds the receiver GPIO
String transmitterPin;                                      // Holds the transmitter GPIO
int receiveAction;                                          // What to do on receive command
int transmitAction;                                         // What to do on transmit command
String ssidStored;                                          // Holds the SSID
String passStored;                                          // Holds the wifi password
const char* APssid = "ESPimaticRF";                         // Holds the SSID for AP mode
const char* APpassword = "espimaticrf";                     // Holds the AP wifi password
String WMode = "";                                          // Holds the wifi mode at boot
String sep = "____";                                        // Sepeator for sending data to frontends
String Mode;                                                // Holds the ESPimaticRF mode (homeduino or node)
String DeviceName;                                          // Holds the ESPimaticRF device name
long mqtt_checkInterval = 60000;                            // 1 minute timeout for MQTT connection check
long mqtt_lastInterval  = 0;
int mqtt_connected;
String esp_hostname;
char mqtt_hostname[1];
String mqtt_server;
unsigned int mqtt_server_port;
const char* outTopic = "/pimaticrf";                        // Topic for MQTT messages
String connectivity;

// External includes
#include <SerialCommand.h>
#include <RFControl.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>                                   // IMPORTANT: make sure to edit PubSubClient.h and set MQTT_MAX_PACKET_SIZE 512

#include "Base64.h"

SerialCommand sCmd;                                         // Setup serial commands
ESP8266WebServer server(80);                                // Setup webserver on port 80
WiFiClient ESPclient;                                       // Setup wifi client
File UploadFile;                                            // File for SPIFFS
String fileName;
PubSubClient client(ESPclient);                             // Setup MQTT client

// Local includes
#include "ESPimaticRF_homeduino.h"
#include "ESPimaticRF_node.h"


void setup() {
  Serial.begin(115200);

  // Check if SPIFFS is OK
  if (!SPIFFS.begin())
  {
    Serial.println("SPIFFS failed, needs formatting");
    handleFormat();
    delay(500);
    ESP.restart();
  }
  else
  {
    FSInfo fs_info;
    if (!SPIFFS.info(fs_info))
    {
      Serial.println("fs_info failed\n");
    }
  }

  CheckParseConfigJson();

  if (Mode != "homeduino")
  {
    Serial.println("");
  }

  if (ssidStored == "" || passStored == "")
  {
    Serial.println("No wifi configuration found, starting in AP mode");
    Serial.println("SSID: ");
    Serial.println(APssid);
    Serial.println("password: ");
    Serial.println(APpassword);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(APssid, APpassword);
    Serial.println("Connected to ");
    Serial.println(APssid);
    Serial.println("IP address: ");
    Serial.println(WiFi.softAPIP().toString());
    WMode = "AP";
  }
  else
  {
    int i = 0;
    Serial.print("Connecting to: ");
    Serial.println(ssidStored);

    if (WiFi.status() != WL_CONNECTED)
    {
      WiFi.disconnect();
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssidStored.c_str(), passStored.c_str());
    }


    while (WiFi.status() != WL_CONNECTED && i < 31 && Mode != "homeduino")
    {
      delay(1000);
      Serial.print(".");
      ++i;
    }
    if (WiFi.status() != WL_CONNECTED && i >= 30)
    {
      Serial.println("Couldn't connect to network :( ");
      WiFi.disconnect();
      delay(1000);
      Serial.println("Setting up access point");
      Serial.println("SSID: ");
      Serial.println(APssid);
      Serial.println("password: ");
      Serial.println(APpassword);
      WiFi.mode(WIFI_AP);
      WiFi.softAP(APssid, APpassword);
      Serial.println("Connected to ");
      Serial.println(APssid);
      IPAddress myIP = WiFi.softAPIP();
      Serial.println("IP address: ");
      Serial.println(WiFi.softAPIP().toString());
      WMode = "AP";
    }
    else
    {
      delay(500);
      Serial.print("Connected to: ");
      Serial.println(ssidStored);

      int ii = 0;
      Serial.println("Waiting for local IP");
      while (WiFi.localIP().toString() == "0.0.0.0" && ii < 15)
      {
        delay(1000);
        Serial.print(".");
        ++i;
      }
      Serial.println(WiFi.localIP().toString());
    }
  }


  // If no root page && AP mode, set root to simple upload page
  if ( !SPIFFS.exists("/root.html") && !SPIFFS.exists("/root.html.gz") && WMode != "AP")
  {
    server.on("/", handle_fupload_html);
  }

  // If connected to AP mode, set root to simple wifi settings page
  if (WMode == "AP")
  {
    server.on("/", handle_wifim_html);
  }
  server.on ("/format", handleFormat );                      // Format Flash ROM - dangerous! Remove if you dont't want this option!
  server.on("/fupload", handle_fupload_html);
  server.on("/wifi_ajax", handle_wifi_ajax);
  server.on("/bwlist_ajax", handle_bwlist_ajax);
  server.on("/list", HTTP_GET, printDirectory);
  server.on("/updatefwm", handle_updatefwm_html);
  server.on("/wifim", handle_wifim_html);
  server.on("/filemanager_ajax", handle_filemanager_ajax);
  server.on("/delete", handleFileDelete);
  server.on("/api", handle_api);
  server.on("/ping", handle_ping);
  server.on("/config_ajax", handle_config_ajax);
  server.on("/root_ajax", handle_root_ajax);
  server.on("/updatefw2", HTTP_POST, []() {
    returnOKReboot();
  }, handle_fwupload);
  server.on("/fupload2", HTTP_POST, []() {
    returnOK();
  }, handle_fileupload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.begin();

  if (Mode == "homeduino")
  {
    // read protocols into global String
    protocolsjson = ReadJson("/protocols.json");

    // Setup callbacks for SerialCommand commands
    sCmd.addCommand("DR", digital_read_command);
    sCmd.addCommand("DW", digital_write_command);
    sCmd.addCommand("AR", analog_read_command);
    sCmd.addCommand("AW", analog_write_command);
    sCmd.addCommand("PM", pin_mode_command);
    sCmd.addCommand("RF", rfcontrol_command);
    sCmd.addCommand("PING", ping_command);
    sCmd.addCommand("RESET", reset_command);
    sCmd.setDefaultHandler(unrecognized);
  }

  if (Mode == "node")
  {
    if (connectivity == "MQTT")
    {
      client.setCallback(callback);
    }
    if (connectivity == "UDP")
    {
      UdpMulti.beginMulticast(WiFi.localIP(),  ipMulti, portMulti);
    }
    bwlistjson = ReadJson("/bwlist.json");                      // read black and whitelist into global String
  }

  if (receiveAction >= 1)
  {
    pinMode(receiverPin.toInt(), INPUT);
    RFControl::startReceiving(receiverPin.toInt());
  }

  esp_hostname = "[" + Mode + "]" + String(WiFi.hostname());
  if (connectivity == "MQTT")
  {
    Serial.println("Attempting MQTT connection...");
    // Connect to MQTT server
    client.setServer(mqtt_server.c_str(), mqtt_server_port);
    esp_hostname.toCharArray(mqtt_hostname, (esp_hostname.length() + 1));
    if (client.connect(mqtt_hostname ) )
    {
      Serial.println("connected");
      client.publish("debug", "This is node:");
      client.publish("debug", outTopic);
      client.subscribe("/pimaticrf");
      mqtt_connected = 1;
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.println("Retry in 1 minute");
      mqtt_connected = 0;
    }
  }
  Serial.println("ESPimaticRF ready ...");
}

void loop()
{
  if (receiveAction == 1)
  {
    if (RFControl::hasData())
    {
      rfcontrolNode_loop();
    }
  }
  
  server.handleClient();
  client.loop();

  if (millis() - mqtt_lastInterval > mqtt_checkInterval && connectivity == "MQTT")
  {
    mqtt_lastInterval = millis();
    Check_mqtt();
  }

  if (Mode == "node" && transmitAction == 1 && connectivity == "UDP")
  {
    udpLoop();
  }


  if (Mode == "homeduino")
  {
    sCmd.readSerial();
    rfcontrol_loop();
  }
}

void CheckParseConfigJson()
{
  // Function to check the config.json form some mandatory settings (ore create them with defaults)
  // and set some values into global variables

  String configjsn = ReadJson("/config.json");
  DynamicJsonBuffer BufferSetup;
  JsonObject& systm = BufferSetup.parseObject(const_cast<char*>(configjsn.c_str()));

  if (systm.containsKey("settings"))
  {
    JsonObject& settings = systm["settings"];

    // Read Wifi settings into global variables
    if (settings.containsKey("wifi"))
    {
      JsonObject& wifi = settings["wifi"];
      ssidStored = wifi["ssid"].asString();
      passStored = wifi["password"].asString();
    }

    if (!settings.containsKey("ESPimaticRF"))
    {
      settings.createNestedObject("ESPimaticRF");
    }
    JsonObject& ESPimaticRF = settings["ESPimaticRF"];

    Mode = ESPimaticRF["mode"].asString();

    UDPrepeat = ESPimaticRF["UDPrepeat"];
    if (UDPrepeat == 0)
    {
      UDPrepeat = 3;
    }
    receiverPin = ESPimaticRF["receiverPin"].asString();
    transmitterPin = ESPimaticRF["transmitterPin"].asString();
    receiveAction = ESPimaticRF["receiveAction"];
    transmitAction = ESPimaticRF["transmitAction"];
    apikey = ESPimaticRF["apikey"].asString();
    pimaticIP = ESPimaticRF["pimaticIP"].asString();
    pimaticPort = ESPimaticRF["pimaticPort"];
    BWmode = ESPimaticRF["BWmode"];
    connectivity = ESPimaticRF["connectivity"].asString();
    mqtt_server = ESPimaticRF["MQTTIP"].asString();
    mqtt_server_port = ESPimaticRF["MQTTPort"];
  }

  int len = systm.measurePrettyLength() + 1;
  char ch[len];
  size_t n = systm.prettyPrintTo(ch, sizeof(ch));
  String tt(ch);
  WriteJson(tt, "/config.json");
}

String ReadJson(String filename)
{
  String json_string;
  File configFile = SPIFFS.open(filename, "r");
  if (!configFile)
  {
    Serial.println("error?");
    if (!SPIFFS.exists(filename))
    {
      Serial.println("File '" + String(filename) + "' not found, create empty json");
      json_string = "{}";
      File jsonFile = SPIFFS.open(filename, "w");
      if (!jsonFile)
      {
        Serial.println("Failed to open '" + filename + "' for writing");
      }
      else
      {
        jsonFile.print(json_string);
        jsonFile.flush();
        jsonFile.close();
      }
      return json_string;
    }
    else
    {
      Serial.println("Error accessing SPIFFS?!");
    }
  }
  else
  {
    while (configFile.available())
    {
      String line = configFile.readStringUntil('\n');
      json_string += line;
    }
    return json_string;
  }
}

void WriteJson(String json, String file)
{
  File jsonFile = SPIFFS.open(file, "w");
  if (!jsonFile)
  {
    Serial.println("Failed to open '" + file + "' for writing");
    return;
  }
  else
  {
    jsonFile.print(json);
    jsonFile.flush();
    jsonFile.close();
  }
}

// An empty ESP8266 Flash ROM must be formatted before using it, actual a problem
void handleFormat()
{
  server.send ( 200, "text/html", "OK");
  Serial.println("Format SPIFFS");
  if (SPIFFS.format())
  {
    if (!SPIFFS.begin())
    {
      Serial.println("Format SPIFFS failed");
    }
  }
  else
  {
    Serial.println("Format SPIFFS failed");
  }
  if (!SPIFFS.begin())
  {
    Serial.println("SPIFFS failed, needs formatting");
  }
  else
  {
    Serial.println("SPIFFS mounted");
  }
}

void handleFileDelete()
{
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if (!path.startsWith("/")) path = "/" + path;
  Serial.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handle_filemanager_ajax()
{
  String form = server.arg("form");
  if (form != "filemanager")
  {
    String HTML;
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
      fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      HTML += String("<option>") + fileName + String("</option>");
    }

    // Glue everything together and send to client
    server.send(200, "text/html", HTML);
  }
}

bool handleFileRead(String path)
{
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "root.html";

  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    if ( (path.startsWith("/css/") || path.startsWith("/js/") || path.startsWith("/fonts/")) && !path.startsWith("/js/insert") )
    {
      server.sendHeader("Cache-Control", " max-age=31104000");
    }
    server.sendHeader("Connection", "close");

    size_t sent = server.streamFile(file, contentType);
    size_t contentLength = file.size();
    file.close();
    return true;
  }
  return false;
}

String getContentType(String filename)
{
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handle_fupload_html()
{
  String HTML = "<br>Files on flash:<br>";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next())
  {
    fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    HTML += fileName.c_str();
    HTML += " ";
    HTML += formatBytes(fileSize).c_str();
    HTML += " , ";
    HTML += fileSize;
    HTML += "<br>";
    //Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }

  server.send ( 200, "text/html", "<form method='POST' action='/fupload2' enctype='multipart/form-data'><input type='file' name='update' multiple><input type='submit' value='Update'></form><br<b>For webfiles only!!</b>Multiple files possible<br>" + HTML);
}

//-------------- FSBrowser application -----------
//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}


void handle_wifi_ajax()
{
  String form = server.arg("form");
  if (form != "wifi")
  {
    String systmjsn = ReadJson("/config.json");
    DynamicJsonBuffer BufferWifi;
    JsonObject& systm = BufferWifi.parseObject(const_cast<char*>(systmjsn.c_str()));
    String ssidStored;
    String passStored;
    if (systm.containsKey("settings"))
    {
      JsonObject& settings = systm["settings"];

      if (settings.containsKey("wifi"))
      {
        JsonObject& wifi = settings["wifi"];

        ssidStored = wifi["ssid"].asString();
        passStored = wifi["password"].asString();
      }
    }

    // Glue everything together and send to client
    server.send(200, "text/html", ssidStored + sep + passStored);
  }
  if (form == "wifi")
  {
    String ssidArg = server.arg("ssid");
    String passArg = server.arg("password");

    String systmjsn = ReadJson("/config.json");
    DynamicJsonBuffer BufferWifi;
    JsonObject& systm = BufferWifi.parseObject(const_cast<char*>(systmjsn.c_str()));
    if (!systm.containsKey("settings"))
    {
      systm.createNestedObject("settings");
    }
    JsonObject& settings = systm["settings"];
    if (!settings.containsKey("wifi"))
    {
      settings.createNestedObject("wifi");
    }
    JsonObject& wifi = settings["wifi"];
    wifi["ssid"] = ssidArg;
    wifi["password"] = passArg;

    int len = systm.measurePrettyLength() + 1;
    char ch[len];
    size_t n = systm.prettyPrintTo(ch, sizeof(ch));
    String tt = (ch);
    WriteJson(tt, "/config.json");

    server.send ( 200, "text/html", "OK");
    delay(500);
    ESP.restart();
  }
}

void handle_wifim_html()
{
  server.send ( 200, "text/html", "<form method='POST' action='/wifi_ajax'><input type='hidden' name='form' value='wifi'><input type='text' name='ssid'><input type='password' name='password'><input type='submit' value='Submit'></form><br<b>Enter WiFi credentials</b>");
}

void printDirectory()
{
  String jsnOutput = "[";
  int i = 0;

  String output;
  Dir dir = SPIFFS.openDir("/");
  while (dir.next())
  {
    fileName = dir.fileName();
    fileName.remove(0, 1);
    size_t fileSize = dir.fileSize();

    if (i != 0)
    {
      jsnOutput += ',';
    }
    jsnOutput += "{\"type\":\"";
    jsnOutput +=  "file";
    jsnOutput += "\",\"name\":\"";
    jsnOutput += fileName;
    jsnOutput += "\"";
    jsnOutput += "}";
    i++;
  }
  jsnOutput += "]";
  server.send ( 200, "text/html", jsnOutput);
}

void handle_updatefwm_html()
{
  server.send ( 200, "text/html", "<form method='POST' action='/updatefw2' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form><br<b>For firmware only!!</b>");
}

void handle_api()
{
  // Get vars for all commands
  String action = server.arg("action");
  String value = server.arg("value");

  int ValidCall = 0;

  if (action == "GET")
  {
    if (value == "lastreceived")
    {
      ValidCall = 1;
      server.send(200, "text/html", LastReceived);
    }
  }

  if (action == "reboot" && value == "true")
  {
    ValidCall = 1;
    Serial.println("Reboot on user request");
    server.send ( 200, "text/html", "OK");
    delay(500);
    ESP.restart();
  }

  if (ValidCall == 0)
  {
    server.send ( 200, "text/html", "ERROR: Unknown API command");
  }
}

void handle_ping()
{
  server.send ( 200, "text/html", "pong");
}

void handle_bwlist_ajax()
{
  String action = server.arg("action");

  if (action == "RemoveBWDevice")
  {
    String protocol = server.arg("protocol");
    String id = server.arg("id");
    String unit = server.arg("unit");

    server.send(200, "text/html", LastReceived);

    Serial.println("Remove from bwlist : " + protocol + "," + unit + "," + id);

    bwlistjson = ReadJson("/bwlist.json");
    DynamicJsonBuffer bwbuffer;
    JsonObject& list = bwbuffer.parseObject(const_cast<char*>(bwlistjson.c_str()));

    if (list.containsKey(protocol))
    {
      JsonObject& bwprotocol = list[protocol];
      if (bwprotocol.containsKey(id))
      {
        JsonObject& bwid = bwprotocol[id];
        bwid.remove(unit);

        int len = list.measurePrettyLength() + 1;
        char ch[len];
        size_t n = list.prettyPrintTo(ch, sizeof(ch));
        String tt = (ch);
        WriteJson(tt, "/bwlist.json");
        bwlistjson = tt;  // update the bwlist in memory
      }
    }
  }

  if (action == "AddBWDevice")
  {

    String jsondata = server.arg("device");
    String type = server.arg("type");

    DynamicJsonBuffer incomingbuffer;
    JsonObject& root = incomingbuffer.parseObject(const_cast<char*>(jsondata.c_str()));

    String protocol = root["protocol"].asString();
    String unit = root["unit"].asString();
    String id = root["id"].asString();

    bwlistjson = ReadJson("/bwlist.json");
    DynamicJsonBuffer bwbuffer;
    JsonObject& list = bwbuffer.parseObject(const_cast<char*>(bwlistjson.c_str()));

    if (!list.containsKey(protocol))
    {
      list.createNestedObject(protocol);
    }
    JsonObject& bwprotocol = list[protocol];

    if (!bwprotocol.containsKey(id))
    {
      bwprotocol.createNestedObject(id);
    }
    JsonObject& bwid = bwprotocol[id];

    bwid[unit] = type;

    int len = list.measurePrettyLength() + 1;
    char ch[len];
    size_t n = list.prettyPrintTo(ch, sizeof(ch));
    String tt = (ch);
    WriteJson(tt, "/bwlist.json");
    bwlistjson = tt;  // update the bwlist in memory

    server.send ( 200, "text/html", "OK");
  }
}

void handle_config_ajax()
{
  String form = server.arg("form");

  if (form == "system")
  {
    String Mode = server.arg("mode");
    String receiver_pin = server.arg("receiver_pin");
    String transmitter_pin = server.arg("transmitter_pin");
    String receive_action = server.arg("receive_action");
    String transmit_action = server.arg("transmit_action");
    String udprepeat = server.arg("udprepeat");
    String pimaticIP = server.arg("pimaticIP");
    String pimaticPort = server.arg("pimaticPort");
    String apikey = server.arg("apikey");
    String bwmode = server.arg("bwmode");
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String connectivity = server.arg("connectivity");
    String mqttserver = server.arg("MQTTIP");
    String mqttport = server.arg("MQTTPort");

    String systmjsn = ReadJson("/config.json");
    DynamicJsonBuffer BufferWifi;
    JsonObject& systm = BufferWifi.parseObject(const_cast<char*>(systmjsn.c_str()));
    if (!systm.containsKey("settings"))
    {
      systm.createNestedObject("settings");
    }
    JsonObject& settings = systm["settings"];
    if (!settings.containsKey("wifi"))
    {
      settings.createNestedObject("wifi");
    }
    JsonObject& wifi = settings["wifi"];
    wifi["ssid"] = ssid;
    wifi["password"] = password;

    if (!settings.containsKey("ESPimaticRF"))
    {
      settings.createNestedObject("ESPimaticRF");
    }
    JsonObject& ESPimaticRF = settings["ESPimaticRF"];

    ESPimaticRF["mode"] = Mode;
    ESPimaticRF["receiverPin"] = receiver_pin;
    ESPimaticRF["transmitterPin"] = transmitter_pin;
    ESPimaticRF["receiveAction"] = receive_action;
    ESPimaticRF["transmitAction"] = transmit_action;
    ESPimaticRF["UDPrepeat"] = udprepeat;
    ESPimaticRF["pimaticIP"] = pimaticIP;
    ESPimaticRF["pimaticPort"] = pimaticPort;
    ESPimaticRF["apikey"] = apikey;
    ESPimaticRF["BWmode"] = bwmode;
    ESPimaticRF["connectivity"] = connectivity;
    ESPimaticRF["MQTTIP"] = mqttserver;
    ESPimaticRF["MQTTPort"] = mqttport;

    server.send ( 200, "text/html", "OK");

    int len = systm.measurePrettyLength() + 1;
    char ch[len];
    size_t n = systm.prettyPrintTo(ch, sizeof(ch));
    String tt = (ch);
    WriteJson(tt, "/config.json");
  }
}


void handle_fileupload()
{
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    noInterrupts();
    fileName = upload.filename;
    Serial.setDebugOutput(true);
    //fileName = upload.filename;
    Serial.println("Upload Name: " + fileName);
    String path;
    if (fileName.indexOf(".css") >= 0)
    {
      path = "/css/" + fileName;
    }
    else if (fileName.indexOf(".js") >= 0 && !fileName.endsWith(".json"))
    {
      path = "/js/" + fileName;
    }
    else if (fileName.indexOf(".otf") >= 0 || fileName.indexOf(".eot") >= 0 || fileName.indexOf(".svg") >= 0 || fileName.indexOf(".ttf") >= 0 || fileName.indexOf(".woff") >= 0 || fileName.indexOf(".woff2") >= 0)
    {
      path = "/fonts/" + fileName;
    }
    else
    {
      path = "/" + fileName;
    }
    UploadFile = SPIFFS.open(path, "w");
    // already existing file will be overwritten!
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (UploadFile)
      UploadFile.write(upload.buf, upload.currentSize);
    Serial.println(fileName + " size: " + upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    Serial.println("Upload Size: ");
    Serial.println(String(upload.totalSize));  // need 2 commands to work!
    if (UploadFile)
      UploadFile.close();
  }
  yield();
}

void returnOK()
{
  server.send(200, "text/plain", "");
}

void returnOKReboot()
{
  server.send(200, "text/plain", "");
  ESP.restart();
}


void handle_fwupload()
{
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    fileName = upload.filename;
    Serial.setDebugOutput(true);
    Serial.printf("Update: %s\n", upload.filename.c_str());
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) { //start with max available size
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true)) //true to set the size to the current progress
    {
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    }
    else
    {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);

  }
  yield();
}

void handle_root_ajax()
{
  // Collect everything for uptime
  long milliseconds   = millis();
  long seconds    = (long) ((milliseconds / (1000)) % 60);
  long minutes    = (long) ((milliseconds / (60000)) % 60);
  long hours      = (long) ((milliseconds / (3600000)) % 24);
  long days       = (long) (milliseconds / 86400000);
  String Uptime   = days + String (" d ") + hours + String(" h ") + minutes + String(" min ") + seconds + String(" sec");

  // Collect free memory
  int FreeHeap = ESP.getFreeHeap();

  // Collect SPIFFS info
  FSInfo fsinfo;
  SPIFFS.info(fsinfo);
  int FSTotal = fsinfo.totalBytes;
  int FSUsed = fsinfo.usedBytes;

  DeviceName = "[" + Mode + "]" + String(WiFi.hostname());
  server.send(200, "text/html", String(FreeHeap) + sep + Uptime + sep + FSTotal + sep + FSUsed + sep + DeviceName + sep + Mode + sep + mqtt_connected);
}

void Check_mqtt()
{
  if (!client.connected() )
  {
    Serial.println("Connection with mqtt lost, reconnect");
    if (client.connect(mqtt_hostname ) )
    {
      Serial.println("connected");
      //client.publish("debug", "This is node:");
      //client.publish("debug", outTopic);
      client.subscribe("/pimaticrf");
      mqtt_connected = 1;
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.println("Retry in 1 minute");
      mqtt_connected = 0;
    }
  }
  else
  {
    mqtt_connected = 1;
  }
}

