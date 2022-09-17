#ifndef _MAIN_HPP_
#define _MAIN_HPP_

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>     // WiFiClient (-> TCPClient)
#include <WiFi.h>
#include <SoftwareSerial.h>
#include <PZEM004Tv30.h>
#include "secrets.hpp"
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <Ticker.h>                                       //For LED status
#include <WiFiManager.h>

#define POWER_METER_MAX_NUM 3

#define LED_H   digitalWrite( LED, HIGH )                 //LED ON
#define LED_L   digitalWrite( LED, LOW )                  //LED OFF
#define LED_T   digitalWrite( LED, !digitalRead(LED) )    //LED Toggle

const uint8_t mac_size = 18;
char MAC_Address[mac_size] = { '\0' };

const char OK_state[] = "[ OK ]";
const char ERROR_state[] = "[ ERROR ]";

const char json_variables[] = {
  "{"
  "\"IP_L\":\"%s\","
  "\"GW\":\"%s\","
  "\"NM\":\"%s\","
  "\"MAC\":\"%s\","
  "\"SW_ver\":\"%s\","
  "\"Started\":\"%s\""
  "}"
};

const char measures_json_frame[] = {
  "{"
  "\"SN\":%hu,"
  "\"Voltage\":%.1f,"
  "\"Current\":%.2f,"
  "\"Power\":%.2f,"
  "\"Energy\":%.3f,"
  "\"Frequency\":%.1f,"
  "\"PF\":%.2f"
  "}"
};

struct PZEM_data {
  uint8_t sn;
  uint8_t error = 0;
  bool isdead = false;
  uint8_t isdead_cntr = 0;
  uint8_t address;
  float voltage;
  float current;
  float power;
  float energy;
  float frequency;
  float pf;
};

void mqttTask( void *pvParameters );
void ConnectionStatus(void);
IPAddress DNS_Resolv(const char* host_p);
void onMqttPublish(const char* topic, uint8_t* payload, int length);
void setClock(void);
const char* getClock(void);
void tick(void);
void RestartESP(void);
void WifiConnect(void);
void configModeCallback(WiFiManager * myWiFiManager);

#endif