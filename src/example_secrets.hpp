// This file is an example, how your secrets.hpp file should look like.

#ifndef _SECRETS_HPP_
#define _SECRETS_HPP_

#include <Arduino.h>                                                    // Arduino header to use PROGMEM.

const char mqtt_user[] PROGMEM       = "your_mqtt_user";                // MQTT username.
const char mqtt_pass[] PROGMEM       = "your_mqtt_password";            // MQTT password.
const char host[] PROGMEM            = "your_mqtt.server.com";          // MQTT server hostname or IP address.
const uint16_t mqtt_port PROGMEM     = 8883;                            // MQTT server port number.
const char update_passwd[] PROGMEM   = "your_ota_password";             // Password for web based OTA. Username is the MQTT user.

const char mqtt_client[] PROGMEM     = "PowerMeter";                    // MQTT client name: "PowerMeter_macaddress".
const char mqtt_base_topic[] PROGMEM = "powermeter";                    // Base topic for MQTT messages: "powermeter/macaddress".
const char mqtt_pub_log[] PROGMEM    = "log";                           // Topic for logging: "powermeter/macaddress/log".
const char mqtt_pub_power[] PROGMEM  = "power";                         // Topic for power data: "powermeter/macaddress/power".

                                                                        // Certificate for SSL communication.
const char CACertificate[] PROGMEM = \
"-----BEGIN CERTIFICATE-----\n" \
"first line of your certificate\n" \
"second line of your certificate\n" \
"the other lines................\n" \
"...............................\n" \
"last line of your certificate\n" \
"-----END CERTIFICATE-----\n";

#endif