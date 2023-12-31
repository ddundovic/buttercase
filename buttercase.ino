#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Smoothed.h>
#include <Preferences.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define CYCLES 50000
#define trace 1               // trace prints on/off
#define github_template 1   // GitHub template
//#define test_loc 1          // test location
//#define kitchen 1             // kitchen location
// pin definitions
 #define RELAY 0              // relay connected to GPIO0
 #define oneWireBus 2         // pin for DS18B20 temp. sensor

#ifdef test_loc
  #include "test_loc.h"
#elif kitchen
  #include "kitchen.h"
#elif github_template
  #include "github_template.h"
#endif

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
int httpCode; 
HTTPClient http;
DynamicJsonDocument doc(1024);

const char *flashNamespace = "data";         // flash memory namespace
const char *device = "esp8266";              // device part of topic name
char *topicPing;                             // ESP initiated ping (ping server) topic
char *topicPong;                             // server ping response topic
char *topicSrvCmd;                           // server command to ESP
char *topicSrvCmdAck;                        // server command ACK
char *topicTemp;                             // sensor temperature topic
char *topicState;                            // current state of state machine topic
char *topicLastNTPSync;                      // last NTP sync topic
char *topicStateStr;                         // current state of state machine topic (string)
char *topicCycleUs;                          // topic for cycle time in us
char *topicForceCmd;                         // topic for command to switch relay ON
char *topicForceStopCmd;                     // topic for command to switch relay OFF
// char *topicESP32Time;                        // topic for ESP RTC time, reported every minute
// char *topicSetDateTime;                      // topic for setting ESP RTC datetime (ISO format)
char *topicBootCounter;                      // boot counter
char *topicCPU0ResetMsg;                     // CPU0 last reset message
char *topicCPU1ResetMsg;                     // CPU1 last reset message
// char *topicNTP1;                             // NTP1
// char *topicNTP2;                             // NTP2
// char *topicNTP3;                             // NTP3
char *topicMAC;                              // ESP MAC address - unique ID (formatted string)
char *topicFwVersion;                        // ESP32 firmware version string in format: "<git hash>"
char *topicFwTimestamp;                      // ESP32 firmware timestamp string in format: "YYYY-MM-DDTHH:MM"
char *topicMsgToMQTT;                        // topic for sending messages to MQTT
char *topicHiTempSP;                         // high temperature setpoint
char *topicWidthTemp;                        // hysteresis width in C
char *topicRelaySwitchCounter;               // relay switch counter

unsigned long state, cycleCnt, cycleTimeUs, lastMilisSent, triggers = 0, nextState = 0;
byte outputRelay = HIGH, nextOutputRelay = HIGH;
float sensorTemp = 0;   // filtered temperature from sensor

unsigned int bootCounter;  // boot counter saved in flash memory
float hiTempSP;         // high temperature setpoint saved in flash memory
float widthTemp;        // hysteresis width in C saved in flash memory, switch ON point ==hiTempSP - widthTemp
unsigned int relaySwitchCnt;        // number of times relay is being energized
char MAC[20] = {0};                 // ESP's MAC address
char fw_version[30] = {0};          // firmware version string in format: "<git hash>" auto updated
char fw_timestamp[30] = {0};        // firmware timestamp in format: "YYYY-MM-DDTHH:MM" auto updated
char fw_version_new[30] = {0};      // new firmware version string, ready for OTA update
char fw_timestamp_new[30] = {0};    // new firmware timestamp in format: "YYYY-MM-DDTHH:MM" auto updated
int newFwCheckTimer = 0;            // counts minutes until next check for new fw update
const int newFwCheckInterval = 13;  // new fw update check interval in minutes
char msg[200] = {0};                // message to MQTT
// Global OTA variables
long totalLength;       //total size of firmware
long currentLength = 0; //current size of written firmware
// const int maxNtpNameLength = 40;
// ... = {"192.168.0.123", ...            // fake NTP server, with 0 TZ offset
// 3 NTP servers saved in flash memory
// char NTPs[3][maxNtpNameLength+1] = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};  

char resetMsgs[2][128];
int resetCodes[2];
enum triggerOfs
{
  _t10ms,
  _t100ms,
  _t1s,
  _t10s,
  _t1m,
  _t10m,
  _t1h,
  _t12h,
  _t24h
};

void pingServer();
byte hysteresisDO(float temp, float hiTempSP, float width, byte curOutput, bool negativeLogic = false);
void saveFloatToFlash(const char *name, float value);
void saveUIntToFlash(const char *name, unsigned int value);
void saveULongToFlash(const char *name, unsigned long value);
void saveBytesToFlash(const char *name, const void *buf, int len);
void saveStringToFlash(const char *name, const char* value);
void updateRelaySwitchCnt(byte curOutput, byte nextOutput, bool negativeLogic);
Ticker tmrPing(pingServer, 5000);
void t10ms();
Ticker tmr10ms(t10ms, 10, 0, MILLIS);
void t100ms();
Ticker tmr100ms(t100ms, 100, 0, MILLIS);
void t1s();
Ticker tmr1s(t1s, 1000, 0, MILLIS);
void t10s();
Ticker tmr10s(t10s, 10000, 0, MILLIS);
void t1m();
Ticker tmr1m(t1m, 60 * (unsigned long)1000, 0, MILLIS);
void t10m();
Ticker tmr10m(t10m, 10 * 60 * (unsigned long)1000, 0, MILLIS);
void t1h();
Ticker tmr1h(t1h, 60 * 60 * (unsigned long)1000, 0, MILLIS);
void t12h();
Ticker tmr12h(t12h, 12 * 60 * 60 * (unsigned long)1000, 0, MILLIS);
void t24h();
Ticker tmr24h(t24h, 24 * 60 * 60 * (unsigned long)1000, 0, MILLIS);
Smoothed <float> tempSensor;
String url; // url String will be used to store the final generated URL

void pingServer()
{
  // Serial.println("pingServer");
  lastMilisSent = millis();
  mqttClient.publish(topicPing, (byte *)&lastMilisSent, sizeof(long));
  // doc = sendGETRequest("?q=ping");
  // if (strcmp(doc["a"], "pong"))
  // {
  //   // set flag to restart state machine
  // }
}

void t10ms() { triggers |= (unsigned long)1 << _t10ms; }
void t100ms() { triggers |= (unsigned long)1 << _t100ms; }
void t1s() { triggers |= (unsigned long)1 << _t1s; }
void t10s() { triggers |= (unsigned long)1 << _t10s; }
void t1m() { triggers |= (unsigned long)1 << _t1m; }
void t10m() { triggers |= (unsigned long)1 << _t10m; }
void t1h() { triggers |= (unsigned long)1 << _t1h; }
void t12h() { triggers |= (unsigned long)1 << _t12h; }
void t24h() { triggers |= (unsigned long)1 << _t24h; }
int triggered(int trigger) { return triggers & (unsigned long)1 << trigger; }

void callback(char *topic, byte *message, unsigned int length)
{
  char publishTopic[50] = {0};
  if (!strcmp(topicPong, topic))
  {
    // pong received
    Serial.println(String("Ping RTT: ") + (millis() - lastMilisSent) + "ms");
  }
  else if (!strcmp(topicForceCmd, topic))
  {
    if (*message == 0x31)  // ASCII for "1"
    {
      nextOutputRelay = LOW;
      mqttClient.publish(topicForceCmd, "0");
    }
  }
  else if (!strcmp(topicForceStopCmd, topic))
  {
   if (*message == 0x31)  // ASCII for "1"
    {
      nextOutputRelay = HIGH;
      mqttClient.publish(topicForceStopCmd, "0");
    }
  }
  // else if (!strcmp(topicSrvCmd, topic))
  // {
  //   // server command received
  //   String cmd = "";
  //   for (int i = 0; i < length; i++)
  //     cmd += (char)message[i];
  //   int res = decodeCmd(cmd);
  //   if (!res)
  //     mqttClient.publish(topicSrvCmdAck, cmd.c_str());
  //   else
  //     mqttClient.publish(topicSrvCmdAck, (String("Unknown command: ") + cmd).c_str());
  // }
  // else if (!strcmp(topicSetDateTime, topic))
  // {
  //   int y, M, d, h, m, s = 0;
  //   char buf[30] = {0};
  //   strncpy(buf, (char *) message, length);
  //   if (strcmp(timestampFmt, buf))
  //   { 
  //     // message is not "yyyy-mm-ddThh:mm:ss"
  //     if (5 <= sscanf(buf, "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s)) 
  //     {
  //       struct tm timeinfo = {0};
  //       timeinfo.tm_year = y - 1900; // Year since 1900
  //       timeinfo.tm_mon = M - 1;     // 0-11
  //       timeinfo.tm_mday = d;        // 1-31
  //       timeinfo.tm_hour = h;        // 0-23
  //       timeinfo.tm_min = m;         // 0-59
  //       timeinfo.tm_sec = (int)s;    // 0-61 (0-60 in C++11)    
  //       time_t t = mktime(&timeinfo);
  //       rtc.setTime(t);
  //       #ifdef trace
  //         Serial.println(rtc.getTime("%Y-%m-%dT%H:%M:%S"));
  //       #endif
  //     }
  //     mqttClient.publish(topicSetDateTime, timestampFmt, true);
  //   }
  // }
  else if (!strcmp(topicBootCounter, topic))
  {
    char buf[30] = {0};
    unsigned int new_value, cur_value;
    cur_value = bootCounter;
    strncpy(buf, (char *) message, length);
    if (sscanf(buf, "%d", &new_value))
    {
      if (cur_value != new_value)
      {
        saveUIntToFlash("bootCounter", new_value);
        bootCounter = new_value;
        mqttClient.publish(topicBootCounter, String(bootCounter).c_str(), true);  // force retain flag
      }
    }
    // if input was garbage, write last good value back
    if (String(bootCounter) != String(buf)) mqttClient.publish(topicBootCounter, String(bootCounter).c_str(), true);
  }
  else if (!strcmp(topicRelaySwitchCounter, topic))
  {
    char buf[30] = {0};
    unsigned int new_value, cur_value;
    cur_value = relaySwitchCnt;
    strncpy(buf, (char *) message, length);
    if (sscanf(buf, "%d", &new_value))
    {
      if (cur_value != new_value)
      {
        saveUIntToFlash("relaySwitchCnt", new_value);
        relaySwitchCnt = new_value;
        mqttClient.publish(topicRelaySwitchCounter, String(relaySwitchCnt).c_str(), true);  // force retain flag
      }
    }
    // if input was garbage, write last good value back
    if (String(relaySwitchCnt) != String(buf)) mqttClient.publish(topicRelaySwitchCounter, String(relaySwitchCnt).c_str(), true);
  }
  // else if (!strncmp(topicNTP1, topic, strlen(topicNTP1) - 1))  // matches "*/NTP"
  // {
  //   char buf[maxNtpNameLength+1] = {0};
  //   char new_value[maxNtpNameLength+1] = {0};
  //   char cur_value[maxNtpNameLength+1] = {0};
  //   int i;
  //   if (strlen(topic) == strlen(topicNTP1))
  //   {
  //     if (sscanf(topic + strlen(topic) - 1, "%d", &i))
  //     {
  //       strncpy(cur_value, NTPs[i-1], maxNtpNameLength);
  //       strncpy(buf, (char *) message, length);
  //       if (strcmp(cur_value, buf))
  //       {
  //         saveStringToFlash(topic + strlen(topic) - 4, buf);  
  //         strncpy(NTPs[i-1], buf, maxNtpNameLength);
  //         strncpy(buf, topic, maxNtpNameLength);  // save topic in another variable because it is overwritten!
  //         mqttClient.publish(buf, NTPs[i-1], true);  // force retain flag
  //       }
  //     }
  //   }
  // }
  else if (!strcmp(topicHiTempSP, topic))
  {
    char buf[30] = {0};
    float new_value, cur_value;
    cur_value = hiTempSP;
    strncpy(buf, (char *) message, length);
    if (sscanf(buf, "%f", &new_value))
    {
      if (abs(cur_value - new_value) >= 0.09999)
      {
        saveFloatToFlash("hiTempSP", new_value);
        hiTempSP = new_value;
        mqttClient.publish(topicHiTempSP, String(hiTempSP).c_str(), true);  // force retain flag
      }
    }
    // if input was garbage, write last good value back
    if (String(hiTempSP) != String(buf)) mqttClient.publish(topicHiTempSP, String(hiTempSP).c_str(), true);
  }
  else if (!strcmp(topicWidthTemp, topic))
  {
    char buf[30] = {0};
    float new_value, cur_value;
    cur_value = widthTemp;
    strncpy(buf, (char *) message, length);
    if (sscanf(buf, "%f", &new_value))
    {
      if (abs(cur_value - new_value) >= 0.09999)
      {
        saveFloatToFlash("widthTemp", new_value);
        widthTemp = new_value;
        mqttClient.publish(topicWidthTemp, String(widthTemp).c_str(), true);  // force retain flag
      }
    }
    // if input was garbage, write last good value back
    if (String(widthTemp) != String(buf)) mqttClient.publish(topicWidthTemp, String(widthTemp).c_str(), true);
  }
  else
  {
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String messageTemp;

    for (int i = 0; i < length; i++)
    {
      Serial.print((char)message[i]);
      messageTemp += (char)message[i];
    }
    Serial.println();

    // Feel free to add more if statements to control more GPIOs with MQTT

    // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
    // Changes the output state according to the message
    // if (String(topic) == "esp32/output") {
    //   Serial.print("Changing output to ");
    //   if(messageTemp == "on"){
    //     Serial.println("on");
    //     digitalWrite(ledPin, HIGH);
    //   }
    //   else if(messageTemp == "off"){
    //     Serial.println("off");
    //     digitalWrite(ledPin, LOW);
    //   }
    //}
  }
}

void reconnect()
{
  int retry = 0;
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    retry++;
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(mqttClientName, mqttUser, mqttPassword))
    {
      Serial.println("connected");
      // initial publish
      // mqttClient.publish(topicSetDateTime, timestampFmt, true);
      mqttClient.publish(topicBootCounter, String(bootCounter).c_str(), true);
      // mqttClient.publish(topicNTP1, NTPs[0], true);
      // mqttClient.publish(topicNTP2, NTPs[1], true);
      // mqttClient.publish(topicNTP3, NTPs[2], true);
      mqttClient.publish(topicMAC, MAC, true);
      mqttClient.publish(topicFwVersion, fw_version, true);
      mqttClient.publish(topicFwTimestamp, fw_timestamp, true);
      mqttClient.publish(topicHiTempSP, String(hiTempSP).c_str(), true);
      mqttClient.publish(topicWidthTemp, String(widthTemp).c_str(), true);
      mqttClient.publish(topicRelaySwitchCounter, String(relaySwitchCnt).c_str(), true);
      // Subscribe
      mqttClient.subscribe(topicPong);
      mqttClient.subscribe(topicSrvCmd);
      mqttClient.subscribe(topicForceCmd);
      mqttClient.subscribe(topicForceStopCmd);
      // mqttClient.subscribe(topicSetDateTime);
      mqttClient.subscribe(topicBootCounter);
      // mqttClient.subscribe(topicNTP1);
      // mqttClient.subscribe(topicNTP2);
      // mqttClient.subscribe(topicNTP3);
      mqttClient.subscribe(topicHiTempSP);
      mqttClient.subscribe(topicWidthTemp);
      mqttClient.subscribe(topicRelaySwitchCounter);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      if (retry >= 2)
      {
        //state = 100;
        return;
      }
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

int connectToWiFi(const int network)
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);  
  delay(500);
  int res = -1, cnt = 0;
  const int MAX_CYCLES = 20; // max number of 500ms cycles to wait for a wifi connection
  Serial.println("");
  Serial.print("Trying SSID: ");
  Serial.println(ssid[network]);
  WiFi.begin(ssid[network], password[network]);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    cnt++;
    Serial.print(".");
    if (cnt >= MAX_CYCLES)
      break;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    res = 0;
    mqttClient.setServer(mqttBrokerAdr.c_str(), 1883);
    mqttClient.setCallback(callback);
    // doc = sendGETRequest("?q=ping");
    // if (!strcmp(doc["a"], "pong")) res = 0;
    Serial.println("\nWiFi connected.");
  }
  return res;
}

String stateStr(int state)
{
  String res;
  switch (state)
  {
  case 100:
    res = String("Start position");
    break;
  case 101:
    res = String("Connected to WiFi");
    break;
  case 200:
    res = String("Run position");
    break;
  default:
    res = String("???");
    break;
  }
  return res;
}

void setup() 
{
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);
  String s;
  char buf[50];
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  // resetMsgs[1][0] = '\0';  // if there is only 1 core
  // for (int i=0; i<ESP.getChipCores(); i++)
  // {
  //   resetCodes[i] = rtc_get_reset_reason(i);
  //   snprintf(resetMsgs[i], 128, "CPU%d reset reason: (0x%02X) %s", i, resetCodes[i], 
  //     get_verbose_reset_reason(resetCodes[i]).c_str());
  //   Serial.println(resetMsgs[i]);
  // }
  // setenv("TZ", String(TZstr).c_str(), 1);
  // tzset();
  // getVersionStr(fw_version);
  strncpy(MAC, WiFi.macAddress().c_str(), 20);
  snprintf(mqttClientName, sizeof(mqttClientName), "%s-%s", device, MAC);

  tempSensor.begin(SMOOTHED_EXPONENTIAL, 10);
  // tempSensor.clear();  // sends ESP32 into boot loop
  sensors.begin();
  preferences.begin(flashNamespace, false);
  bootCounter = preferences.getUInt("bootCounter", 0);
  bootCounter++;
  preferences.putUInt("bootCounter", bootCounter);
  // for (int i=0; i<3; i++)
  // {
  //   sprintf(buf, "NTP%d", i+1);
  //   s = preferences.getString(buf, String(""));
  //   if (s == "") preferences.putString(buf, NTPs[i]);
  //   else if (String(NTPs[i]) != s)  // preffer values stored in flash to hardcoded ones
  //   {
  //     strncpy(NTPs[i], s.c_str(), maxNtpNameLength);
  //   }
  // }
  hiTempSP = preferences.getFloat("hiTempSP", 0);
  widthTemp = preferences.getFloat("widthTemp", 0);
  preferences.putFloat("hiTempSP", hiTempSP);
  preferences.putFloat("widthTemp", widthTemp);
  relaySwitchCnt = preferences.getUInt("relaySwitchCnt", 0);
  preferences.putUInt("relaySwitchCnt", relaySwitchCnt);
  s = preferences.getString("fw_version", String(""));
  if (s == "") preferences.putString("fw_version", s);  // run only once when variable is created in prefs
  strncpy(fw_version, s.c_str(), 30-1);
  s = preferences.getString("fw_timestamp", String(""));
  if (s == "") preferences.putString("fw_timestamp", s);  // run only once when variable is created in prefs
  strncpy(fw_timestamp, s.c_str(), 30-1);
  preferences.end();
  delay(1000);

  cycleCnt = 0;
  nextState = 100; // start position

  typedef struct 
  {
    const char *base;
    char **topic;
  } Topic;
  // For easier integration into Telegraf  
  // start base with:
  // "int/" - for integers
  // "float/" - for floats
  // "str/" - for C style strings
  Topic topics[] = {{"int/ping", &topicPing}, {"int/pong", &topicPong}, {"int/srv_cmd", &topicSrvCmd}, 
  {"int/srv_cmd_ack", &topicSrvCmdAck}, {"int/state", &topicState}, {"str/stateStr", &topicStateStr}, 
  {"int/cycleTimeUs", &topicCycleUs}, {"str/MAC", &topicMAC}, {"int/bootCounter", &topicBootCounter}, 
  {"str/CPU0ResetMsg", &topicCPU0ResetMsg}, {"str/CPU1ResetMsg", &topicCPU1ResetMsg}, 
  {"float/temp", &topicTemp}, {"str/forceCmd", &topicForceCmd}, {"str/forceStopCmd", &topicForceStopCmd},
  {"float/hiTempSP", &topicHiTempSP}, {"float/widthTemp", &topicWidthTemp}, 
  {"str/fwVersion", &topicFwVersion}, 
  {"str/fwTimestamp", &topicFwTimestamp}, {"str/message", &topicMsgToMQTT}, 
  {"int/relaySwitchCnt", &topicRelaySwitchCounter}, 
  {"", NULL}};
  

  // {"lastNTPSync", &topicLastNTPSync}, {"forceCmd", &topicForceCmd}, 
  // {"forceStopCmd", &topicForceStopCmd}, {"ESP32Time", &topicESP32Time}, 
  // {"setDateTimeCmd", &topicSetDateTime}, {"NTP1", &topicNTP1}, {"NTP2", &topicNTP2}, {"NTP3", &topicNTP3}, 

  int i = 0;
  while (strcmp(topics[i].base, ""))
  {
    s = String(String(location) + "/" + device + "/" + topics[i].base);
    *topics[i].topic = (char *)malloc(s.length() + 1);
    s.toCharArray(*topics[i].topic, s.length() + 1);
    i++;
  }

  tmr10ms.start();
  tmr100ms.start();
  tmr1s.start();
  tmr10s.start();
  tmr1m.start();
  tmr10m.start();
  tmr1h.start();
  tmr12h.start();
  tmr24h.start();
  //  boolean res = mqttClient.setBufferSize(50*1024); // ok for 640*480
  //  if (res) Serial.println("Buffer resized."); else Serial.println("Buffer resizing failed");

  delay(1000);
  }

void s100()
{
  static int network = 0;
  int res;
  tmrPing.stop();
#ifdef trace
  Serial.println("state: 100");
#endif
  res = connectToWiFi(network);
  if (!res)
  {
    nextState = 101; // WiFi connected -> next state
  }
  else
  {
    network = (network + 1) % NUM_NETWORKS;
    // delay(5000); // WiFi not connected -> same state, try again Reset?
  }
}

// void s101()
// {
//   static int tries = 0;
//   tries++;
//   if (syncTime(TZstr)) // Set for CET
//   {
//     tries = 0;
//     nextState = 102;
//   }
//   else sleep(5);
//   if (tries >= 5) 
//   {
//     tries = 0;
//     nextState = 102;
//   }
// }

void s101()
{
  //  TODO look for config in MQTT broker and configure working dataset
  nextState = 200;
}

void s200()
{
  if (tmrPing.state() == STOPPED)
    tmrPing.start();
  // TODO regulate temp.
}

void updateTimers()
{
  tmrPing.update();
  tmr10ms.update();
  tmr100ms.update();
  tmr1s.update();
  tmr10s.update();
  tmr1m.update();
  tmr10m.update();
  tmr1h.update();
  tmr12h.update();
  tmr24h.update();
}

void loop() 
{
  updateTimers();
  if (triggered(_t10s))
  {
    #ifdef trace
    // Serial.println("*********************************");
    Serial.printf("fw_ver: %s fw_timestamp: %s\n", fw_version, fw_timestamp);
    Serial.println("state: " + String(state));
    #endif
    sensors.requestTemperatures(); 
    sensorTemp = sensors.getTempCByIndex(0);
    tempSensor.add(sensorTemp);
    sensorTemp = tempSensor.get();
    nextOutputRelay = hysteresisDO(sensorTemp, hiTempSP, widthTemp, outputRelay, true);  
    #ifdef trace
    #endif
    mqttClient.publish(topicState, String(state).c_str());
    mqttClient.publish(topicStateStr, stateStr(state).c_str());
    mqttClient.publish(topicTemp, String(sensorTemp).c_str());
  }
  if (triggered(_t1m))
  {
    #ifdef trace
    // Serial.println("_t1m");
    // Serial.println(rtc.getTime("%Y-%m-%dT%H:%M:%S"));
    // printLocalTime();
    #endif
    mqttClient.publish(topicCPU0ResetMsg, resetMsgs[0]);
    mqttClient.publish(topicCPU1ResetMsg, resetMsgs[1]);
    // // message_to_whatsapp("ESP32: " + rtc.getTime("%Y-%m-%dT%H:%M:%S"));
    if (!newFwCheckTimer)   // check for new fw update every <newFwCheckInterval> minutes
    {
      String fwURL = checkForNewFW();
      Serial.printf("fwURL=%s\n", fwURL.c_str());
      if (fwURL != "") doOTA(fwURL);
    }
    newFwCheckTimer = (newFwCheckTimer + 1) % newFwCheckInterval;
  }
  if (state >= 101)
  {
    if (!(cycleCnt % CYCLES))
    {
      cycleTimeUs = cycleTime(CYCLES);
      #ifdef trace
        Serial.println("Cycle time (us): " + String(cycleTimeUs));
      #endif
      mqttClient.publish(topicCycleUs, String(cycleTimeUs).c_str());
    }
    if (triggered(_t1m))  // send timestamp every minute to MQTT
    {
      // mqttClient.publish(topicESP32Time, rtc.getTime("%Y-%m-%dT%H:%M:%S").c_str(), true);
    }
    // update every now and then
    if (triggered(_t10m))
    {
      // if (syncTime(TZstr))
      // {
      //   #ifdef trace
      //   Serial.println("Time updated: " + rtc.getTime("%Y-%m-%dT%H:%M:%S"));
      //   #endif
      // }
    }
  }
  if (state >= 200)
  {
    if (!mqttClient.connected())
    {
      reconnect();
    }
    mqttClient.loop();
    if (triggered(_t10m))
    {
      #ifdef trace
      // Serial.println(rtc.getTime("%Y-%m-%dT%H:%M:%S"));
      #endif
    }
  }
  if (nextState)  // new state selected?
  {
    state = nextState;  // set the new state
    nextState = 0;
  }
  switch (state)
  {
  case 100: // start position
    s100();
    break;
  case 101: // wifi connected
    s101();
    break;
  case 200: // run position
    s200();
    break;
  }
  // write outputs
  updateRelaySwitchCnt(outputRelay, nextOutputRelay, true);
  outputRelay = nextOutputRelay;
  digitalWrite(RELAY, outputRelay);
  triggers = 0; // clear triggers
  cycleCnt++;
  }

int cycleTime(const long cycles)
{
  static unsigned long prev_us = 0;
  unsigned long new_us = micros(), res;
  // Serial.println("prev_us: " + String(prev_us));
  // Serial.println("new_us: " + String(new_us));
  // Serial.println("cycleCnt: " + String(cycleCnt));
  res = (unsigned long) ((new_us - prev_us) / cycles);
  prev_us = new_us;
  return res;
}

byte hysteresisDO(float temp, float hiTempSP, float width, byte curOutput, bool negativeLogic)
{
  byte res = curOutput;
  if ((curOutput == (LOW ^ negativeLogic)) && (temp < hiTempSP - width)) res = HIGH ^ negativeLogic;
  if ((curOutput == (HIGH ^ negativeLogic)) && (temp > hiTempSP)) res = LOW ^ negativeLogic;
  return res;
}

void saveFloatToFlash(const char *name, float value)
{
  preferences.begin(flashNamespace, false);
  preferences.putFloat(name, value);
  preferences.end();
}

void saveUIntToFlash(const char *name, unsigned int value)
{
  preferences.begin(flashNamespace, false);
  preferences.putUInt(name, value);
  preferences.end();
}

void saveULongToFlash(const char *name, unsigned long value)
{
  preferences.begin(flashNamespace, false);
  preferences.putULong(name, value);
  preferences.end();
}

void saveStringToFlash(const char *name, const char* value)
{
  preferences.begin(flashNamespace, false);
  preferences.putString(name, value);
  preferences.end();
}

void saveBytesToFlash(const char *name, const void *buf, int len)
{
  preferences.begin(flashNamespace, false);
  preferences.putBytes(name, buf, len);
  preferences.end();
}

void updateRelaySwitchCnt(byte curOutput, byte nextOutput, bool negativeLogic)
{
  
  if ((curOutput == (LOW ^ negativeLogic)) && (nextOutput == (HIGH ^ negativeLogic)))
  {
    relaySwitchCnt++;
    saveUIntToFlash("relaySwitchCnt", relaySwitchCnt);
    mqttClient.publish(topicRelaySwitchCounter, String(relaySwitchCnt).c_str(), true);  // force retain flag
  }
}

String urlencode(String str) // Function used for encoding the url
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
      // encodedString+=code2;
    }
    yield();
  }
  return encodedString;
}

String checkForNewFW()
{
  HTTPClient http;
  fw_version_new[0] = '\0';
  String res = "";
  url = otaServerAdr + urlencode(MAC) + "?fw_ver=" + urlencode(fw_version);
  Serial.printf("http.begin(%s)\n", url.c_str());
  http.begin(wifiClient, url);
  httpCode = http.GET();
  if (httpCode == 200)
  {
    String response = http.getString();
    #ifdef trace
    Serial.println(response);
    #endif
    DeserializationError err = deserializeJson(doc, response);
    if (!err)
    {
      // no error in deserialization, check if fw_ver is different than the one stored in prefs
      // if it is construct URL for fw DL and put it in res, otherwise res=""
      strncpy(fw_version_new, (const char *) (doc["fw_ver"]), 30-1);
      strncpy(fw_timestamp_new, (const char *) (doc["fw_timestamp"]), 30-1);
      if (strcmp(fw_version, fw_version_new))
        {
          snprintf(msg, sizeof(msg), "New firmware found! Old version: %s (%s). New version: %s (%s).", 
            fw_version, fw_timestamp, fw_version_new, fw_timestamp_new);
          mqttClient.publish(topicMsgToMQTT, msg, true);  // force retain flag
          res = otaServerAdr + fwDlSuffix + (const char *) (doc["dev_group"]) + fwVerParamSuffix + fw_version_new;
        }
        else
          {
            snprintf(msg, sizeof(msg), "No new firmware found!"); 
            mqttClient.publish(topicMsgToMQTT, msg, true);  // force retain flag
          }
    }
    else
    {
      // error in deserialization, print error message to Serial and MQTT topic, set res to ""
      snprintf(msg, sizeof(msg), "Deserialization error! err=%d", err); 
      mqttClient.publish(topicMsgToMQTT, msg, true);  // force retain flag
      #ifdef trace
      Serial.printf("%s\n", msg);
      #endif
    }
  }
  else
    { 
      snprintf(msg, sizeof(msg), "Error getting OTA info. HTTP code=%d", httpCode); 
      mqttClient.publish(topicMsgToMQTT, msg, true);  // force retain flag
      #ifdef trace
      Serial.printf("%s\n", msg);
      #endif
    }
  http.end();
  return res;
}

// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
void updateFirmware(uint8_t *data, size_t len)
{
  static long callNo = 0;
  callNo++;
  Update.write(data, len);
  currentLength += len; 
  // Print dots while waiting for update to finish
  // TODO flash LED ...
  Serial.print('.');
  if (!(callNo%100)) Serial.println("");
  // if current length of written firmware is not equal to total firmware size, repeat
  if(currentLength != totalLength) return;
  bool success = Update.end(true);
  if (success)
  {
    saveStringToFlash("fw_version", fw_version_new);
    saveStringToFlash("fw_timestamp", fw_timestamp_new);
    snprintf(msg, sizeof(msg), "Update success! Total Size: %u. Rebooting...", currentLength); 
    mqttClient.publish(topicMsgToMQTT, msg, true);  // force retain flag
    Serial.printf("\n%s\n", msg);
  }
  else
  {
    snprintf(msg, sizeof(msg), "Update failed! Total Size: %u. Rebooting...", currentLength); 
    mqttClient.publish(topicMsgToMQTT, msg, true);  // force retain flag
    Serial.printf("\n%s\n", msg);
  }
  mqttClient.flush();
  String s;
  preferences.begin(flashNamespace, false);
  s = preferences.getString("fw_version", String(""));
  if (s == "") preferences.putString("fw_version", s);  // run only once when variable is created in prefs
  strncpy(fw_version, s.c_str(), 30-1);
  s = preferences.getString("fw_timestamp", String(""));
  if (s == "") preferences.putString("fw_timestamp", s);  // run only once when variable is created in prefs
  strncpy(fw_timestamp, s.c_str(), 30-1);
  preferences.end();
  Serial.printf("\nfw_version: %s\n", fw_version);
  Serial.printf("fw_timestamp: %s\n", fw_timestamp);

  delay(2000);
  // Restart ESP32 to see changes 
  ESP.restart(); 
}

void doOTA(String fwURL)
{
  // Connect to external web server
  http.begin(wifiClient, fwURL);
  // Get file, just to check if each reachable
  int resp = http.GET();
  // If file is reachable, start downloading
  if (resp == 200)
  {
    // get length of document (is -1 when Server sends no Content-Length header)
    totalLength = http.getSize();
    Serial.printf("totalLength=%d\n", totalLength);
    long len = totalLength;
    // this is required to start firmware update process
    // Update.begin(UPDATE_SIZE_UNKNOWN);  // WON'T WORK ON ESP8266!!!
    Update.begin(totalLength);
    Serial.printf("FW Size: %u\n",totalLength);
    // create buffer for read
    uint8_t buff[128] = { 0 };
    // get tcp stream
    WiFiClient * stream = http.getStreamPtr();
    // read all data from server
    Serial.println("Updating firmware...");
    while (http.connected() && (len > 0 || len == -1)) 
    {
      // get available data size
      size_t size = stream->available();
      if (size) {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        // pass to function
        updateFirmware(buff, c);
        if (len > 0) {
            len -= c;
        }
      }
      delay(1);
    }
  }
}

void checkNVS()
{
  int bootCounter;
  preferences.begin(flashNamespace, false);
  bootCounter = preferences.getUInt("bootCounter", 100);
  Serial.printf("bootCounter before ++: %d\n", bootCounter);
  bootCounter++;
  preferences.putUInt("bootCounter", bootCounter);
  bootCounter = 0;
  bootCounter = preferences.getUInt("bootCounter", 100);
  Serial.printf("bootCounter after ++: %d\n", bootCounter);
  preferences.end();
}

void http_ping()
{
  HTTPClient http;
  url = otaServerAdr + "ping";
  http.begin(wifiClient, url);
  httpCode = http.GET();
  if (httpCode == 200)
  {
    String response = http.getString();
    #ifdef trace
    Serial.println(response);
    #endif
    DeserializationError err = deserializeJson(doc, response);
    if (!err)
    {
      // no error in deserialization, check if fw_ver is different than the one stored in prefs
      // if it is construct URL for fw DL and put it in res, otherwise res=""
//      Serial.printf("Ping sent. Response: %s\n", doc["response"]);
    }
    else
    {
      // error in deserialization, print error message to Serial and MQTT topic, set res to ""
      snprintf(msg, sizeof(msg), "Deserialization error! err=%d", err); 
      #ifdef trace
      Serial.printf("%s\n", msg);
      #endif
    }
  }
  else
    { 
      snprintf(msg, sizeof(msg), "Error getting response to ping. HTTP code=%d", httpCode); 
      #ifdef trace
      Serial.printf("%s\n", msg);
      #endif
    }
  http.end();
}
