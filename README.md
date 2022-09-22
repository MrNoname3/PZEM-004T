# __PZEM-004T__
This project was designed to read 1-3 __PZEM-004T 100A V3.0__ power meters with ESP32 microcontroller and send the measured data to a Mosquitto MQTT server.

## __SSL setup:__
SSL encrypted communication with the MQTT server is enabled by default. For encryption, you must specify the server certificate in the ___secrets.hpp___ file. If you don't need SSL, comment out or delete the line marked ___-D USE_SSL___ under ___build_flags___ in ___platformio.ini___ file:
```
build_flags =     
    -DCORE_DEBUG_LEVEL=1                ; Debug level -> 0:none, 1:error, 2:warn, 3:info, 4:debug, 5:verbose
    -D PZEM004_SOFTSERIAL               ; Enable software serial in PZEM004 driver class.
    -D USE_SSL                          ; Enable SSL communication. (Certification required!)
```

## __MQTT setup:__

To use the software you need to provide the ___secrets.hpp___ file. You can rename the ___examples_secrets.hpp___ file to ___secrets.hpp___ and fill it with your MQTT data.

```cpp
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

                                                                        // Certification for SSL communication.
const char CACertificate[] PROGMEM = \
"-----BEGIN CERTIFICATE-----\n" \
"first line of your certificate\n" \
"second line of your certificate\n" \
"the other lines................\n" \
"...............................\n" \
"last line of your certificate\n" \
"-----END CERTIFICATE-----\n";

#endif
```
