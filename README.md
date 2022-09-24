# __PZEM-004T__
This project was designed to read 1-3 __PZEM-004T 100A V3.0__ power meters with ESP32 microcontroller and send the measured data to a Mosquitto MQTT server.

## __SSL setup:__
SSL encrypted communication with the MQTT server is __enabled by default!__ For encryption, you must specify the server certificate in the ___secrets.hpp___ file. If you don't need SSL, comment out or delete the line marked ___-D USE_SSL___ under ___build_flags___ in ___platformio.ini___ file:
```
build_flags =     
    -DCORE_DEBUG_LEVEL=1                ; Debug level -> 0:none, 1:error, 2:warn, 3:info, 4:debug, 5:verbose
    -D PZEM004_SOFTSERIAL               ; Enable software serial in PZEM004 driver class.
    -D USE_SSL                          ; Enable SSL communication. (Certificate required!)
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
```

## __Sensor and pin setup:__

This section is at the beginning of the ___main.cpp___ file. 

```cpp
#define LED                   2                                 // Pin number of the status LED.
#define WIFI_RST_BTN          4                                 // Pin number of the Wifi credentials reset button.
#define POWER_METER_RST_BTN   5                                 // Pin number of the energy value reset button.
```
You can configure the pins for ___LED___, ___WIFI_RST_BTN___ and ___POWER_METER_RST_BTN___. 
* ___LED:___ Status LED output pin. Flashes during ___setup()___. If everything is OK, it will turn off after the ___setup()___ phase.
* ___WIFI_RST_BTN:___ Input pin. If this pin is pulled low during ___setup()___, it will clear the saved Wifi credentials and Wifimanager will start in AP mode to configure a new Wifi connection.
* ___POWER_METER_RST_BTN:___ Input pin. If this pin is pulled low during the ___setup()___ phase, it clears the stored energy value of the power sensors.

```cpp
#define POWER_METER_NUM       3                                 // Define the maximum number of power meter devices.
const uint8_t rxPins[POWER_METER_NUM] = { 16, 18, 22 };         // Power meters software serial RX pins.
const uint8_t txPins[POWER_METER_NUM] = { 17, 19, 23 };         // Power meters software serial TX pins.
```
The number of power meters should be entered in the ___POWER_METER_NUM___ field. You must then specify the RX and TX pins of the sensors in the ___rxPins___ and ___txPins___ arrays. In this example, pin 16/17 is the RX/TX pin of sensor 0, pin 18/19 is the RX/TX pin of sensor 1, and so on.

```cpp
#define MEASURE_TIME          10000                             // Measure time of the power meters in ms.
```
You can define the measurement time of the sensors in _ms_ using ___MEASURE_TIME___.

```cpp
#define TOPIC_NAME_SIZE       50                                // MQTT topics name sizes.
```

```cpp
char mqtt_client_name[TOPIC_NAME_SIZE] = { '\0' };              // Storing the MQTT client name.
char mqtt_log[TOPIC_NAME_SIZE] = { '\0' };                      // Storing the name of the MQTT logging topic.
char mqtt_power[TOPIC_NAME_SIZE] = { '\0' };                    // Storing the name of the MQTT power data topic.
```
Use ___TOPIC_NAME_SIZE___ to specify the size of the MQTT topic strings, which are filled in the ___setup()___ section:

```cpp
// The MQTT topics used by devices include the MAC address of the devices
// to distinguish between devices with the same firmware.
sprintf(mqtt_client_name, "%s_%s", mqtt_client, MAC_Address);                   // Example: "PowerMeter_macaddress"
sprintf(mqtt_log, "%s/%s/%s", mqtt_base_topic, MAC_Address, mqtt_pub_log);      // Example: "powermeter/macaddress/log"
sprintf(mqtt_power, "%s/%s/%s", mqtt_base_topic, MAC_Address, mqtt_pub_power);  // Example: "powermeter/macaddress/power"
```
## __Donation__
If this project has helped reduce development time and you would like to donate, you can do so here:

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/donate/?hosted_button_id=H9U2Y9SHNY6MQ)

## __License__
Licensed under the [MIT License](LICENSE).

