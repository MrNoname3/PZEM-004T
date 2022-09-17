#ifndef _MAIN_HPP_
#define _MAIN_HPP_

#include <Arduino.h>                  /// Needed for Arduino core functions.
#include <WiFiClient.h>               /// TCP client.
#include <WiFiClientSecure.h>         /// TCP client with SSL encryption support.
#include <WiFi.h>                     /// Header to use WiFi functions.
#include <SoftwareSerial.h>           /// Driver to use software serial.
#include <PZEM004Tv30.h>              /// Driver to interact with PZEM004TV30 power meter(s).
#include "secrets.hpp"                /// Secrets file, to store MQTT settings and credentials.
#include <WebServer.h>                /// Web server libraries to HTTP OTA update.
#include <HTTPUpdateServer.h>         /// HTTP OTA update.
#include <PubSubClient.h>             /// MQTT client library.
#include <Ticker.h>                   /// Ticker for LED status.
#include <WiFiManager.h>              /// Intelligent WiFi connection manager.

#define POWER_METER_MAX_NUM 3                         /// Define the maximum number of power meter devices.

#define LED_H digitalWrite( LED, HIGH )               /// Status LED ON state.
#define LED_L digitalWrite( LED, LOW )                /// Status LED OFF state.
#define LED_T digitalWrite( LED, !digitalRead(LED) )  /// LED Toggle state.

const uint8_t mac_size = 18;                          /// MAC address size.
char MAC_Address[mac_size] = { '\0' };                /// Variable to store the formatted MAC address string.
const char OK_state[] = "[ OK ]";                     /// OK state string for serial debugging.
const char ERROR_state[] = "[ ERROR ]";               /// ERROR state string for serial debugging.

const char init_log_json_frame[] = {                  /// This is a JSON frame for init logging that is sent to the MQTT server.
  "{"
  "\"IP_L\":\"%s\","
  "\"GW\":\"%s\","
  "\"NM\":\"%s\","
  "\"MAC\":\"%s\","
  "\"SW_ver\":\"%s\","
  "\"Started\":\"%s\""
  "}"
};

const char measures_json_frame[] = {                  /// This is a JSON frame for data that is sent to the MQTT server.
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

struct PZEM_data {                                    /// This structure stores data readed from PZEM power meter.
  uint8_t sn;                                         /// Sensor number.
  uint8_t error = 0;                                  /// Reading error codes.
  bool isdead = false;                                /// It stores whether the sensor is alive or dead.
  uint8_t isdead_cntr = 0;                            /// Counter for dead times.
  uint8_t address;                                    /// Sensor address.
  float voltage;                                      /// Measured voltage in [V].
  float current;                                      /// Measured current in [A].
  float power;                                        /// Measured power [W].
  float energy;                                       /// Measures energy [Wh]
  float frequency;                                    /// Measured frequency [Hz].
  float pf;                                           /// Measured power factor.
};

/// Dedicated task for MQTT communication.
///
/// @brief This task handles the MQTT communication and other network-based operations.
/// @param pvParameters Tasks can be started with the specified parameters. This is not used in this project.
void mqttTask( void *pvParameters );

/// Checks the status of the connection.
///
/// @brief This function is called periodically to check the connection with to the server.
/// If the connection fails, restarts the ESP.
/// @param  -
void ConnectionStatus(void);

/// DNS resolv function.
///
/// @brief This function is called periodically and resolvs the specified domain name.
/// This is necessary for a stable SSL connection. ( Without it I sometimes get SSL errors. )
/// @param host_p The domain name string to be resolved. 
/// @return Returns the resolved IP address of the specified domain name.
IPAddress DNS_Resolv(const char* host_p);

/// Management of MQTT messages.
///
/// @brief This function is only called by the MQTT message loop manager.
/// @param topic Topic from which the message originated.
/// @param payload The message.
/// @param length Length of the message.
void onMqttPublish(const char* topic, uint8_t* payload, int length);

/// DateTime setup.
///
/// @brief This function synchronizes the time with an NTP server. 
/// @param -
void setClock(void);

/// Get DateTime.
///
/// @brief This function returns the pointer to the DateTime string.
/// @param -
/// @return Returns the pointer to the formatted DateTime string. 
const char* getClock(void);

/// Ticker interrupt.
///
/// @brief This function toggles the status LED at the specified time. 
/// @param -
void tick(void);

/// Restarts ESP.
///
/// @brief This function restarts the ESP. It flushes the serial buffer before restarting.
/// @param -
void RestartESP(void);

/// Connecting to Wifi.
///
/// @brief This function uses the Wifimanager for smart Wifi configuration and connection.
/// @param -
void WifiConnect(void);

/// Wifimanager callback.
///
/// @brief If the Wifi connection with the saved credentials failed, this function is called by the Wifimanager.
/// @param myWiFiManager This function needs a Wifimanager object to interact with it.
void configModeCallback(WiFiManager * myWiFiManager);


#endif