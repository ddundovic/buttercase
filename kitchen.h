#include <Arduino.h>
const char *mqttUser = "dundo";
const char *mqttPassword = "YEGv7UGPjqM30P4bfkZ9";
const char *location = "kitchen";           // location part of topic name

const int NUM_NETWORKS = 2; // 3
// const char *ssid[NUM_NETWORKS] = {"Catch-22", "1KWLAN", "1KB_WLAN"};
// const char *password[NUM_NETWORKS] = {"dUnDoViC", "Dundo.1234", "belajec123"};
const char *ssid[NUM_NETWORKS] = {"Catch-22", "MCT-INT"};
const char *password[NUM_NETWORKS] = {"dUnDoViC", "R0JM3KT4B"};

// Catch-22 (DHCP)
// String mqttBrokerAdr = "192.168.43.202";

// doma
// String mqttBrokerAdr = "192.168.0.106";

// Cres (DHCP)
// String mqttBrokerAdr = "192.168.0.101";

// dundovic.com
// TODO define array of domains, use several dynamic DNS aliases (more than 1 provider)
String mqttBrokerAdr = "dundovic.com";

// WhatsApp section
String apiKey = "114137";              // Add your Token number that bot has sent you on WhatsApp messenger
String phone_number = "+385915186134"; // Add your WhatsApp app registered phone number (same number that bot send you in url)
// String apiKey = "475745";              //Add your Token number that bot has sent you on WhatsApp messenger
// String phone_number = "+385914059880"; //Add your WhatsApp app registered phone number (same number that bot send you in url)
