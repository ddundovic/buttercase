#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define CYCLES 20000
#define trace 1         // trace prints on/off
#define test_loc 1      // test location
//#define kitchen 1       // kitchen location
// pin definitions
// #define SSR1 4         // output pin for Solid State Relay 1
// #define SSR2 18        // output pin for Solid State Relay 2
// #define PushButton1 16 // input pin for pushbutton
// #define forcedLED 17   // output pin for LED signaling that forced state is ON

#ifdef test_loc
  #include "test_loc.h"
#elif laganini2
  #include "laganini2.h"
#endif

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
// Preferences preferences;

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
// char *topicForceCmd;                         // topic for command to switch to forced state (on "1")
// char *topicForceStopCmd;                     // topic for command to switch to low tarrif state (on "1")
// char *topicESP32Time;                        // topic for ESP RTC time, reported every minute
// char *topicSetDateTime;                      // topic for setting ESP RTC datetime (ISO format)
char *topicBootCounter;                      // boot counter
char *topicCPU0ResetMsg;                     // CPU0 last reset message
char *topicCPU1ResetMsg;                     // CPU1 last reset message
// char *topicNTP1;                             // NTP1
// char *topicNTP2;                             // NTP2
// char *topicNTP3;                             // NTP3
char *topicMAC;                              // ESP MAC address - unique ID (formatted string)
// char *topicFwVersion;                        // ESP firmware version string in format: "YYYY-MM-DDTHH:MM (<git hash>)"

unsigned long state, cycleCnt, cycleTimeUs, lastMilisSent, triggers = 0, nextState = 0;
//byte outputSSR = LOW, outputForcedLED = 0;
unsigned int bootCounter;  // boot counter saved in flash memory
// uint64_t mac;                       // ESP32 MAC address - unique ID
// uint8_t *mac_b = (uint8_t *) &mac;  // same as above, addressable by bytes 
//String MAC = "";                       // same as above, formated into string
char MAC[20] = {0};                 // same as above, formated into string
// char fw_version[30] = {0};          // firmware version string in format: "YYYY-MM-DDTHH:MM (<git hash>)" auto updated
// const int maxNtpNameLength = 40;
// ... = {"192.168.0.123", ...            // fake NTP server, with 0 TZ offset
// 3 NTP servers saved in flash memory
// char NTPs[3][maxNtpNameLength+1] = {"pool.ntp.org", "time.google.com", "time.cloudflare.com"};  

char resetMsgs[2][128];
int resetCodes[2];
// const int numCmd = 2;
// const char *cmds[numCmd] = {"slow", "fast"};
// [0][] - winter time, [1][] - summer time
// [][0] - start high tariff, [][1] - end high tariff
enum triggerOfs
{
  _t1s,
  _t10s,
  _t1m,
  _t10m,
  _t1h,
  _t12h,
  _t24h
};

void pingServer();
Ticker tmrPing(pingServer, 5000);
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

void pingServer()
{
  // // Serial.println("pingServer");
  // lastMilisSent = millis();
  // mqttClient.publish(topicPing, (byte *)&lastMilisSent, sizeof(long));
  // // doc = sendGETRequest("?q=ping");
  // // if (strcmp(doc["a"], "pong"))
  // // {
  // //   // set flag to restart state machine
  // // }
}

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
  // if (!strcmp(topicPong, topic))
  // {
  //   // pong received
  //   Serial.println(String("Ping RTT: ") + (millis() - lastMilisSent) + "ms");
  // }
  // // else if (!strcmp(topicSrvCmd, topic))
  // // {
  // //   // server command received
  // //   String cmd = "";
  // //   for (int i = 0; i < length; i++)
  // //     cmd += (char)message[i];
  // //   int res = decodeCmd(cmd);
  // //   if (!res)
  // //     mqttClient.publish(topicSrvCmdAck, cmd.c_str());
  // //   else
  // //     mqttClient.publish(topicSrvCmdAck, (String("Unknown command: ") + cmd).c_str());
  // // }
  // else if (!strcmp(topicForceCmd, topic))
  // {
  //   if (*message == 0x31)  // ASCII for "1"
  //   {
  //     if (state == 202) nextState = 203;
  //     mqttClient.publish(topicForceCmd, "0");
  //   }
  // }
  // else if (!strcmp(topicForceStopCmd, topic))
  // {
  //  if (*message == 0x31)  // ASCII for "1"
  //   {
  //     if (state == 203) nextState = 201;
  //     mqttClient.publish(topicForceStopCmd, "0");
  //   }
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
  // else if (!strcmp(topicBootCounter, topic))
  // {
  //   char buf[30] = {0};
  //   unsigned int new_value, cur_value;
  //   cur_value = bootCounter;
  //   strncpy(buf, (char *) message, length);
  //   if (sscanf(buf, "%d", &new_value))
  //   {
  //     if (cur_value != new_value)
  //     {
  //       saveUIntToFlash("bootCounter", new_value);
  //       bootCounter = new_value;
  //       mqttClient.publish(topicBootCounter, String(bootCounter).c_str(), true);  // force retain flag
  //     }
  //   }
  //   // if input was garbage, write last good value back
  //   if (String(bootCounter) != String(buf)) mqttClient.publish(topicBootCounter, String(bootCounter).c_str(), true);
  // }
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
  // else
  // {
  //   Serial.print("Message arrived on topic: ");
  //   Serial.print(topic);
  //   Serial.print(". Message: ");
  //   String messageTemp;

  //   for (int i = 0; i < length; i++)
  //   {
  //     Serial.print((char)message[i]);
  //     messageTemp += (char)message[i];
  //   }
  //   Serial.println();

  //   // Feel free to add more if statements to control more GPIOs with MQTT

  //   // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  //   // Changes the output state according to the message
  //   // if (String(topic) == "esp32/output") {
  //   //   Serial.print("Changing output to ");
  //   //   if(messageTemp == "on"){
  //   //     Serial.println("on");
  //   //     digitalWrite(ledPin, HIGH);
  //   //   }
  //   //   else if(messageTemp == "off"){
  //   //     Serial.println("off");
  //   //     digitalWrite(ledPin, LOW);
  //   //   }
  //   //}
  // }
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
    // TODO create client name from boardtype + MAC
    if (mqttClient.connect("ESP8266Client", mqttUser, mqttPassword))
    {
      Serial.println("connected");
      // initial publish
      // mqttClient.publish(topicSetDateTime, timestampFmt, true);
      mqttClient.publish(topicBootCounter, String(bootCounter).c_str(), true);
      // mqttClient.publish(topicNTP1, NTPs[0], true);
      // mqttClient.publish(topicNTP2, NTPs[1], true);
      // mqttClient.publish(topicNTP3, NTPs[2], true);
      mqttClient.publish(topicMAC, MAC, true);
      // mqttClient.publish(topicFwVersion, fw_version, true);
      // Subscribe
      mqttClient.subscribe(topicPong);
      mqttClient.subscribe(topicSrvCmd);
      // mqttClient.subscribe(topicForceCmd);
      // mqttClient.subscribe(topicForceStopCmd);
      // mqttClient.subscribe(topicSetDateTime);
      mqttClient.subscribe(topicBootCounter);
      // mqttClient.subscribe(topicNTP1);
      // mqttClient.subscribe(topicNTP2);
      // mqttClient.subscribe(topicNTP3);
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
  const int MAX_CYCLES = 10; // max number of 500ms cycles to wait for a wifi connection
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

// disabled before
  // pinMode(4, INPUT);
  // digitalWrite(4, LOW);
  // rtc_gpio_hold_dis(GPIO_NUM_4);

  // pinMode(SSR1, OUTPUT);
  // digitalWrite(SSR1, LOW);
  // pinMode(SSR2, OUTPUT);
  // digitalWrite(SSR2, LOW);
  // pinMode(forcedLED, OUTPUT);
  // digitalWrite(forcedLED, LOW);
  // preferences.begin(flashNamespace, false);
  // bootCounter = preferences.getUInt("bootCounter", 0);
  // bootCounter++;
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
  // preferences.putUInt("bootCounter", bootCounter);
  // preferences.end();
  delay(1000);

  cycleCnt = 0;
  nextState = 100; // start position

  typedef struct 
  {
    const char *base;
    char **topic;
  } Topic;
  Topic topics[] = {{"ping", &topicPing}, {"pong", &topicPong}, {"srv_cmd", &topicSrvCmd}, 
  {"srv_cmd_ack", &topicSrvCmdAck}, {"state", &topicState}, {"stateStr", &topicStateStr}, 
  {"cycleTimeUs", &topicCycleUs}, {"MAC", &topicMAC}, {"bootCounter", &topicBootCounter}, 
  {"CPU0ResetMsg", &topicCPU0ResetMsg}, {"CPU1ResetMsg", &topicCPU1ResetMsg}, 
  {"temp", &topicTemp}, {"", NULL}};
  
  // {"lastNTPSync", &topicLastNTPSync}, {"forceCmd", &topicForceCmd}, 
  // {"forceStopCmd", &topicForceStopCmd}, {"ESP32Time", &topicESP32Time}, 
  // {"setDateTimeCmd", &topicSetDateTime}, {"NTP1", &topicNTP1}, {"NTP2", &topicNTP2}, {"NTP3", &topicNTP3}, 
  //  {"fwVersion", &topicFwVersion}, {"", NULL}};

  int i = 0;
  while (strcmp(topics[i].base, ""))
  {
    s = String(String(location) + "/" + device + "/" + topics[i].base);
    *topics[i].topic = (char *)malloc(s.length() + 1);
    s.toCharArray(*topics[i].topic, s.length() + 1);
    i++;
  }

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
//  outputSSR = LOW;  // open SSR at the end of cycle
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
  int res = 0, totRes = 0;
  if (tmrPing.state() == STOPPED)
    tmrPing.start();
  // TODO regulate temp.
}

void updateTimers()
{
  tmrPing.update();
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
    Serial.println("state: " + String(state));
    #endif
    mqttClient.publish(topicState, String(state).c_str());
    mqttClient.publish(topicStateStr, stateStr(state).c_str());
  }
  if (triggered(_t1m))
  {
    #ifdef trace
    // Serial.println(rtc.getTime("%Y-%m-%dT%H:%M:%S"));
    // printLocalTime();
    #endif
    mqttClient.publish(topicCPU0ResetMsg, resetMsgs[0]);
    mqttClient.publish(topicCPU1ResetMsg, resetMsgs[1]);
    // // message_to_whatsapp("ESP32: " + rtc.getTime("%Y-%m-%dT%H:%M:%S"));
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
  // digitalWrite(SSR1, outputSSR);
  // digitalWrite(SSR2, outputSSR);
  // digitalWrite(forcedLED, outputForcedLED);
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