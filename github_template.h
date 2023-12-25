#include <Arduino.h>
const char *mqttUser = "mqttUser";
const char *mqttPassword = "mqttPassword";
char mqttClientName[50] = {0};                  // name used to connect to MQTT broker
const char *location = "kitchen";               // location part of topic name

const int NUM_NETWORKS = 2;
const char *ssid[NUM_NETWORKS] = {"SSID1", "SSID2"};
const char *password[NUM_NETWORKS] = {"pass1", "pass2"};

// TODO define array of domains, use several dynamic DNS aliases (more than 1 provider)
String mqttBrokerAdr = "mqttserver.com";

// TODO define array of domains, use several dynamic DNS aliases (more than 1 provider)
String otaServerAdr = "http://mqttserver.com/ota/";

// suffix to add on otaServerAdr for firmware download endpoint (+ "<device group>" + "?fw_ver=<new_fw_str>")
String fwDlSuffix = "fw_dl/";

// suffix to add on otaServerAdr + fwDlSuffix + "<device group>" (+ <new_fw_str>")
String fwVerParamSuffix = "?fw_ver=";
