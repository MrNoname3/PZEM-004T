#include "main.hpp"

//************* Constants *************//
const char sw_ver[] = "V0.0.1";                                 // Software version number.
const uint8_t LED = 2;                                          // Pin number of the status LED.
const uint8_t WiFiRstBtn = 4;                                   // Pin number of the Wifi credentials reset button.
const uint8_t PowerMeterRstBtn = 5;                             // Pin number of the energy value reset button.
const uint8_t rxPins[POWER_METER_MAX_NUM] = { 16, 18, 22 };     // Power meters software serial RX pins.
const uint8_t txPins[POWER_METER_MAX_NUM] = { 17, 19, 23 };     // Power meters software serial TX pins.
const uint16_t measure_time = 10000;                            // Measure time of the power meters in ms.
const uint8_t topic_name_size = 50;                             // MQTT topics name sizes.

//************* MQTT string variables. *************//
char mqtt_client_name[topic_name_size] = { '\0' };              // Variable storing the MQTT client name.
char mqtt_log[topic_name_size] = { '\0' };                      // Variable storing the name of the MQTT logging topic.
char mqtt_power[topic_name_size] = { '\0' };                    // Variable storing the name of the MQTT power data topic.

//************* Objects and structures. *************//
SoftwareSerial swSerial[POWER_METER_MAX_NUM];                   // Software serial objects for the power meters.
PZEM004Tv30 pzem[POWER_METER_MAX_NUM];                          // Driver objects for power meters.
PZEM_data pzem_data[POWER_METER_MAX_NUM];                       // Measurement data structures for power meters.

#ifdef USE_SSL                                                  // Choose between encrypted and unencrypted TCP connection.
WiFiClientSecure tcp_client;                                    // Object of encrypted TCP connection.
#else
WiFiClient tcp_client;                                          // Object of unencrypted TCP connection.
#endif
PubSubClient mqtt(tcp_client);                                  // Object of MQTT client.
Ticker ticker;                                                  // Object of the timer interrupt handler.

WebServer httpServer(28080);                                    // Object of the HTTP server.
HTTPUpdateServer httpUpdater;                                   // Object of the HTTP OTA update.

//************* RTOS variables. *************//
SemaphoreHandle_t mqttMutex;                                    // Variable of the MQTT mutex. 
TaskHandle_t loopHandle = NULL;                                 // Variable of the loop task.
TaskHandle_t mqttTaskHandle = NULL;                             // Variable of the MQTT task.
QueueHandle_t mqttQueue;                                        // Queue of MQTT messages.
const byte mqttQueueSize = 3;                                   // Queue size.

//************* Setup section. *************//
void setup() {  
  Serial.begin(115200);                                                         // Init the debug serial port.

  for( uint8_t i = 0; i < POWER_METER_MAX_NUM; i++ ) {                          // Setup sensors.
    pzem[i] = PZEM004Tv30( swSerial[i] );                                       // Power meters setup.
    swSerial[i].begin( PZEM_BAUD_RATE, SWSERIAL_8N1, rxPins[i], txPins[i] );    // Software serial ports setup.
    pzem_data[i].sn = i;                                                        // Gives a number for the sensors.
  }

  pinMode(LED, OUTPUT);                                               // Sets LED pin as output.
  pinMode(WiFiRstBtn, INPUT_PULLUP);                                  // Sets Wifi credentials reset button as input pullup.
  pinMode(PowerMeterRstBtn, INPUT_PULLUP);                            // Sets energy reset button as a pullup input.
  Serial.println("\r\n***************************************");      // Debug prints.
  Serial.printf("[%lu] Setup starting...\r\n", millis());                                               
  Serial.printf("[%lu] Software version: %s\r\n", millis(), sw_ver);
  ticker.attach(0.2, tick);                                           // Toggle LED with 200ms time.

  if( digitalRead(PowerMeterRstBtn) == LOW ) {                        // If energy reset button is active...
    Serial.println("Resetting energy meters!");
    for( uint8_t i = 0; i < POWER_METER_MAX_NUM; i++ ) {
      pzem[i].resetEnergy();                                          // Reset total energy stored by power meters.
      delay(50);
    }
  }

  WifiConnect();                                                      // Call function for Wifi connection.

  Serial.printf("[%lu] Connecting to router", millis());
  uint8_t timeout = 0;
  while ( WiFi.localIP() == IPAddress( 0, 0, 0, 0 ) ) {               // Wait until the device receives an IP address.
    Serial.print(".");
    delay(100);
    timeout++;
    if (timeout >= 100) {                                             // If the device does not receive an IP address 
      timeout = 0;                                                    // by the specified time, it restarts the ESP.
      Serial.printf(" %s\r\n", ERROR_state); 
      RestartESP();
      break;
    }
  }
  Serial.printf(" %s\r\n", OK_state);

  uint8_t mac[6];                                                     // Store the MAC address of the device
  WiFi.macAddress(mac);                                               // in the specified format.
  sprintf(MAC_Address, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); 

  // The MQTT topics used by devices include the MAC address of the devices
  // to distinguish between devices with the same firmware.
  sprintf(mqtt_client_name, "%s_%s", mqtt_client, MAC_Address);                   // Example: "PowerMeter_macaddress"
  sprintf(mqtt_log, "%s/%s/%s", mqtt_base_topic, MAC_Address, mqtt_pub_log);      // Example: "powermeter/macaddress/log"
  sprintf(mqtt_power, "%s/%s/%s", mqtt_base_topic, MAC_Address, mqtt_pub_power);  // Example: "powermeter/macaddress/power"

  // Printing network connection details.
  Serial.printf(" IP_L: %s\r\n", WiFi.localIP().toString().c_str());
  Serial.printf(" GW: %s\r\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf(" NM: %s\r\n", WiFi.subnetMask().toString().c_str());
  Serial.printf(" MAC: %s\r\n", MAC_Address);

  setClock();                                                               // Call the time synchronisation function.
  DNS_Resolv(host);                                                         // Call function for DNS resolvation.
  #ifdef USE_SSL
  tcp_client.setCACert(CACertificate);                                      // Set up a certificate for SSL connection.
  #endif
  tcp_client.setTimeout(10);                                                // Setting the TCP connection timeout.      

  Serial.printf("[%lu] Connecting to server ", millis());                   // Establishing a TCP connection with the server.
  if ( tcp_client.connect(host, mqtt_port) == true ) {
    Serial.println(OK_state);
  }
  else {
    Serial.println(ERROR_state);
  }

  Serial.printf("[%lu] Connecting to MQTT broker ", millis());              // Connecting to the MQTT broker.
  if (mqtt.connect(mqtt_client_name, mqtt_user, mqtt_pass) == true) {
    Serial.println(OK_state);
  }
  else {
    Serial.printf("ERROR State: %d\r\n", mqtt.state()); 
  }

  Serial.printf("MQTT publish:\r\n %s\r\n %s\r\n", mqtt_log, mqtt_power);   // Printing used MQTT topics.

  httpUpdater.setup(&httpServer);                                           // Set up and start an HTTP OTA server.
  httpUpdater.updateCredentials(mqtt_user, update_passwd);                  // Setup login information.
  httpServer.begin();

  char system_info_json[320] = { '\0' };                            // Create a system information string in JSON format.

  sprintf(system_info_json, init_log_json_frame, 
    WiFi.localIP().toString().c_str(),
    WiFi.gatewayIP().toString().c_str(),
    WiFi.subnetMask().toString().c_str(),
    MAC_Address,
    sw_ver,
    getClock()
  );

  mqtt.publish(mqtt_log, system_info_json);                         // Publishing the system information string.
  mqtt.setCallback(onMqttPublish);                                  // Set callback when receiving MQTT messages.

  mqttQueue = xQueueCreate( mqttQueueSize, sizeof(PZEM_data) );     // Create a queue for communication between tasks.
  if( mqttQueue == NULL ) {
    Serial.println("Error creating the MQTT queue!");
  }

  mqttMutex = xSemaphoreCreateMutex();                              // Create the mqtt mutex.
  xSemaphoreGive( mqttMutex );

  loopHandle = xTaskGetCurrentTaskHandle();                         // Save the loop task handler.
  vTaskPrioritySet( loopHandle, 10 );                               // Set the priority of the loop task.

  // Create a task for MQTT communication and network management.
  if( xTaskCreateUniversal( mqttTask, "mqttTask", 4096, NULL, 10, &mqttTaskHandle, 0 ) != pdTRUE ) {
    Serial.println("Error creating the MQTT task!");
  }

  ticker.detach();                                                  // Status LED toggle off.

  Serial.println("***************************************");        // Debug prints.
  Serial.printf("[%lu] Loop(s) starting...\r\n", millis());
  LED_L;                                                            // Turn status LED off, just to make sure.

}

//************* Loop1 section. *************//
void loop() {

  static uint32_t measure_timer = millis();                         // Timer variable for timing measurements.

  if( millis() - measure_timer >= measure_time ) {                  // Timer check.
    measure_timer = millis();                                       // Timer reload.

    bool isalldown = true;                                          // Variable for checking the status of power sensors.

    for( uint8_t i = 0; i < POWER_METER_MAX_NUM; i++ ) {            // Check all power sensors.

      isalldown &= pzem_data[i].isdown;                             // Health check.

      // If the last sensor is down too, it restarts the ESP.
      if( ( i == (POWER_METER_MAX_NUM - 1) ) && ( isalldown == true ) ) {
        Serial.printf( "[%lu] All sensor is down!\r\n", millis() );
        RestartESP();
      }

      // If the current sensor is not working, it will skip the reading.
      if( pzem_data[i].isdown == true ) {                           
        continue;
      }

      // Read the measured data from the sensor object into the sensor structure.
      pzem_data[i].address = pzem[i].readAddress();
      pzem_data[i].voltage = pzem[i].voltage();
      pzem_data[i].current = pzem[i].current();
      pzem_data[i].power = pzem[i].power();
      pzem_data[i].energy = pzem[i].energy();
      pzem_data[i].frequency = pzem[i].frequency();
      pzem_data[i].pf = pzem[i].pf();

      // Check the validity of the scanned data and generate error codes.
      bitWrite( pzem_data[i].error, 0, !!isnan(pzem_data[i].voltage) );
      bitWrite( pzem_data[i].error, 1, !!isnan(pzem_data[i].current) );
      bitWrite( pzem_data[i].error, 2, !!isnan(pzem_data[i].power) );
      bitWrite( pzem_data[i].error, 3, !!isnan(pzem_data[i].energy) );
      bitWrite( pzem_data[i].error, 4, !!isnan(pzem_data[i].frequency) );
      bitWrite( pzem_data[i].error, 5, !!isnan(pzem_data[i].pf) );

      // If an error has occurred...
      if( pzem_data[i].error > 0 ) {
        Serial.printf( "Error occured on sensor [%hu] with code [%hu]!\r\n", pzem_data[i].sn, pzem_data[i].error );
        
        // Increases the error counter.
        pzem_data[i].isdown_cntr++;

        // If the error counter value is more than 3...
        if( pzem_data[i].isdown_cntr >= 3 ) {

          // Disable sensor reading, because it is not connected or broken...
          Serial.printf( "Sensor [%hu] reading is disabled!\r\n", pzem_data[i].sn );
          // And mark it as broken.
          pzem_data[i].isdown = true;

        }
        
      }
      else {

        // If the read is successful, clear the error counter....
        pzem_data[i].isdown_cntr = 0;

        // And send the power data structure to the queue for processing.
        if( xQueueSend( mqttQueue, &pzem_data[i], 10 ) != pdTRUE ) {
          Serial.println( "Sendig data to queue failed!" );
        }

      }

      vTaskDelay(10);                 // Gives a little break for the task.
    }  // End of for loop.
  }  // End of timer check.
  
  vTaskDelay(5);                      // Gives a little break for the task.
}  // End of infinite loop.

//************* Loop2 section. *************//
void mqttTask( void *pvParameters ) {

  uint32_t dns_timer = millis();                                    // Timer variable for timing DNS resolvation.

  while(1) {                                                        // Infinite loop.

    yield();                                                        // Maintaining network hardware.
    ConnectionStatus();                                             // Call TCP status check function.

    // Receive the power data structure from the queue and store it in a local structure.
    PZEM_data pzem_data_to_send;
    if( xQueueReceive( mqttQueue, &pzem_data_to_send, 0 ) == pdTRUE ) {

      // Create a data string from power data structure in JSON format.
      char measures_json_string[128] = { '\0' };

      sprintf( measures_json_string, measures_json_frame,
        pzem_data_to_send.sn, 
        pzem_data_to_send.voltage,
        pzem_data_to_send.current,
        pzem_data_to_send.power,
        pzem_data_to_send.energy,
        pzem_data_to_send.frequency,
        pzem_data_to_send.pf
      );
      
      if( xSemaphoreTake( mqttMutex, 100 ) == pdTRUE ) {            // Take resource.
        mqtt.publish( mqtt_power, measures_json_string );           // Send string to MQTT publish buffer.
        xSemaphoreGive( mqttMutex );                                // Give resource.
      }
      else {                
        Serial.println("MQTT mutex error!");
      }

      // Debug prints.
      Serial.printf( "[%lu] JSON data: %s\r\n", millis(), measures_json_string );     

    }  // End of the if statement.

    if( xSemaphoreTake( mqttMutex, 100 ) == pdTRUE ) {              // Take resource.
      mqtt.loop();                                                  // Spin MQTT loop.
      xSemaphoreGive( mqttMutex );                                  // Give resource.
    }
    else {                
      Serial.println("MQTT mutex error!");
    }

    if( millis() - dns_timer >= 6 * 60 * 60 * 1000 ) {              // Timer check.
      dns_timer = millis();                                         // Timer reload.
      yield();                                                      // Maintaining network hardware.
      DNS_Resolv(host);                                             // Call function for DNS resolvation.
    }

    yield();                                                        // Maintaining network hardware.
    // Handling the HTTP OTA server.
    // Sample update URL: http://192.168.51.120:28080/update
    httpServer.handleClient();

    vTaskDelay(5);                    // Gives a little break for the task.
  }  //End of loop.

  vTaskDelete( NULL );                // Deletes the task if the processing somehow reaches this line.
}

//************* Function section. *************//
void ConnectionStatus( void ) {
  const uint16_t checking_time = 100;                               // Time value for a timer.
  static uint32_t checking_timer = 0;                               // Timer variable to check the connection.

  if( millis() - checking_timer >= checking_time ) {                // Regularly checks the TCP connection to the server.
    checking_timer = millis();                                      // Timer reload.
    
    if( mqtt.connected() == false ) {                               // Checks the connection.
      Serial.printf("[%lu] MQTT %s\r\n", millis(), ERROR_state);
      RestartESP();                                                 // If the connection fails, restart the ESP.
    }
    
  }
}

void onMqttPublish(const char* topic, uint8_t* payload, int length) {
  // There is nothing here yet.
}

IPAddress DNS_Resolv(const char* host_p) {
  Serial.printf("[%lu] Resolving DNS ", millis());
  IPAddress remote_addr;                                            // Variable to store the resolved IP address.
  if( WiFi.hostByName(host_p, remote_addr) ) {                      // Resolve domain name.
    Serial.println(OK_state);
  }
  else {
    Serial.println(ERROR_state);
  }
  return remote_addr;                                               // Returns with the IP address.
}

void WifiConnect(void) {
  
  WiFiManager wm;                                                   // Local WiFiManager object.

  // If you press this button on startup, the Wifi credentials will be removed.
  if( digitalRead(WiFiRstBtn) == LOW)  {    
    wm.resetSettings();                                             // Clear the saved Wifi credentials.
    Serial.println("Wifi credentials cleared!");                    // Debug.
  }

  Serial.printf("[%lu] Connecting to Wifi ", millis());
  yield();                                                          // Maintaining network hardware.

  WiFi.mode(WIFI_STA);                                              // Sets the Wifi mode, because the default is STA+AP.

  wm.setDebugOutput(false);                                         // Disable debugging prints.
  wm.setConnectTimeout(30);                                         // Tries to connect to the Wifi until 30s before continuing.
  wm.setConfigPortalTimeout(90);                                    // Automatic closing of the configuration portal after n seconds.
  wm.setCaptivePortalEnable(true);                                  // false:disable / true:enable captive portal redirection.
  wm.setAPClientCheck(true);                                        // Avoid timeout when client connects to the softap.
  wm.setShowPassword(false);                                        // Hide Wifi password in the debug log.
  // Set AP mode IP addresses.
  wm.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
  // Sets callback that is called when a previous connection to WiFi fails and enters Access Point mode.
  wm.setAPCallback(configModeCallback);
  yield();                                                          // Maintaining network hardware.

  // Takes the ssid and password and tries to connect.
  // If it fails to connect, it will start an access point with the specified name
  // and goes into a blocking loop awaiting configuration.
  if( wm.autoConnect("PowerMeterConfig") == false ) {
    Serial.println(ERROR_state);
    RestartESP();                                                   // Restart ESP and try again.
  }
  
  Serial.println(OK_state);                                         // When you get here, you're connected to WiFi.  
  WiFi.mode(WIFI_STA);                                              // Wifi station mode.

}


void configModeCallback(WiFiManager *myWiFiManager) {
  //  Called when WiFiManager enters configuration mode.
  Serial.println(ERROR_state);
  // Print AP mode information.
  Serial.printf("[%lu] Entered config mode!\r\n APIP: %s\r\n SSID: %s\r\n", 
    millis(), 
    WiFi.softAPIP().toString().c_str(), 
    myWiFiManager -> getConfigPortalSSID().c_str() 
  );
  Serial.printf("[%lu] Waiting for config portal setup ", millis());
}


void setClock(void) {  
  // Set the time required for x.509 validation via NTP.
  configTime(2 * 3600, 0, "0.hu.pool.ntp.org", "1.hu.pool.ntp.org", "2.hu.pool.ntp.org");

  Serial.printf("[%lu] Waiting for NTP time sync", millis());
  time_t nowSecs = time(nullptr);                                   // Create date-time structure.

  uint8_t timeout = 0;                                              // Timeout counter.

  while (nowSecs < 8 * 3600 * 2) {                                  // Converts the time.
    delay(500);
    Serial.print(F("."));
    yield();                                                        // Maintaining network hardware.
    nowSecs = time(nullptr);
    timeout++;
    if (timeout >= 20) {                                            // If a timeout has occurred...
      timeout = 0;    
      Serial.printf(" %s\r\n", ERROR_state);   
      RestartESP();                                                 // Restarts the ESP.
      return;
    }
  }

  Serial.printf(" %s\r\n", OK_state);  
  struct tm timeinfo;                                               // Structure for storing the date-time.
  getLocalTime(&timeinfo);                                          // Get the local time.
  Serial.printf(" Current time: %s", asctime(&timeinfo));           // Print the local date-time string.
}

const char* getClock(void) {

  static char actualTime[ 75 ] = { '\0' };                          // Array to store formatted date-time string.
	memset(actualTime, '\0', sizeof(actualTime));                     // Empty the array.

	time_t t = time(NULL);                                            // Date-time structure.
	struct tm tm = *localtime(&t);	                                  // Get actual time.

  // Copy date-time structure to a formatted string.
	sprintf(actualTime, "%04d.%02d.%02d. %02d:%02d:%02d", 
    tm.tm_year + 1900, 
    tm.tm_mon + 1, 
    tm.tm_mday, 
    tm.tm_hour, 
    tm.tm_min, 
    tm.tm_sec
  );

	return actualTime;                                                // Returns with the pointer of datetime string.
}

void tick() {                                                       // Toggle state
  LED_T;                                                            // Set pin to the opposite state.
}

void RestartESP(void) {
  Serial.println("Restarting...");
  Serial.flush();                                                   // Flush serial buffer, before restart.
  ESP.restart();                                                    // Restarting ESP.
  delay(10000);                                                     // 10s delay.
}