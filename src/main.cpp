#include "main.hpp"

const char sw_ver[] = "V0.0.1";
const uint8_t LED = 2;
const uint8_t WiFiRstBtn = 4;
const uint8_t PowerMeterRstBtn = 5;
const uint8_t rxPins[POWER_METER_MAX_NUM] = { 16, 18, 22 };
const uint8_t txPins[POWER_METER_MAX_NUM] = { 17, 19, 23 };
const uint16_t measure_time = 10000;  //ms
const uint8_t topic_name_size = 50;

char mqtt_client_name[topic_name_size] = { '\0' };
char mqtt_log[topic_name_size] = { '\0' };
char mqtt_power[topic_name_size] = { '\0' };
//char mqtt_cmd[topic_name_size] = { '\0' };

SoftwareSerial swSerial[POWER_METER_MAX_NUM];
PZEM004Tv30 pzem[POWER_METER_MAX_NUM];
PZEM_data pzem_data[POWER_METER_MAX_NUM];

#ifdef USE_SSL
WiFiClientSecure tcp_client;
#else
WiFiClient tcp_client;
#endif
PubSubClient mqtt(tcp_client);
Ticker ticker;

WebServer httpServer(28080);
HTTPUpdateServer httpUpdater;

SemaphoreHandle_t mqttMutex;
TaskHandle_t loopHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
QueueHandle_t mqttQueue;                                    //Queue of the MQTT messages
const byte mqttQueueSize = 3;                               //Queue size

void setup() {
  // Debugging Serial port
  Serial.begin(115200);

  for( uint8_t i = 0; i < POWER_METER_MAX_NUM; i++ ) {
    pzem[i] = PZEM004Tv30( swSerial[i] );
    swSerial[i].begin( PZEM_BAUD_RATE, SWSERIAL_8N1, rxPins[i], txPins[i] );
    pzem_data[i].sn = i;
  }

  pinMode(LED, OUTPUT); 
  pinMode(WiFiRstBtn, INPUT_PULLUP);
  pinMode(PowerMeterRstBtn, INPUT_PULLUP);
  Serial.println("\r\n***************************************");
  Serial.printf("[%lu] Setup starting...\r\n", millis());
  ticker.attach(0.2, tick);
  Serial.printf("[%lu] Software version: %s\r\n", millis(), sw_ver);

  if( digitalRead(PowerMeterRstBtn) == LOW ) {
    Serial.println("Reset enregy counters!");
    for( uint8_t i = 0; i < POWER_METER_MAX_NUM; i++ ) {
      pzem[i].resetEnergy();
      delay(50);
    }
  }

  // todo Wifi connection code needed here -> Wifimanager.
  WifiConnect();

  Serial.printf("[%lu] Connecting to router", millis());
  uint8_t timeout = 0;
  while ( WiFi.localIP() == IPAddress( 0, 0, 0, 0 ) ) {
    Serial.print(".");
    delay(100);
    timeout++;
    if (timeout >= 100) {
      timeout = 0;
      Serial.printf(" %s\r\n", ERROR_state); 
      RestartESP();
      break;
    }
  }
  Serial.printf(" %s\r\n", OK_state);

  uint8_t mac[6];  
  WiFi.macAddress(mac); 
  sprintf(MAC_Address, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);  

  sprintf(mqtt_log, "%s/%s/%s", mqtt_base_topic, MAC_Address, mqtt_pub_log);
  //sprintf(mqtt_cmd, "%s/%s/%s", mqtt_base_topic, MAC_Address, mqtt_sub_cmd);
  sprintf(mqtt_client_name, "%s_%s", mqtt_client, MAC_Address);
  sprintf(mqtt_power, "%s/%s/%s", mqtt_base_topic, MAC_Address, mqtt_pub_power);

  Serial.printf(" IP_L: %s\r\n", WiFi.localIP().toString().c_str());
  Serial.printf(" GW: %s\r\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf(" NM: %s\r\n", WiFi.subnetMask().toString().c_str());
  Serial.printf(" MAC: %s\r\n", MAC_Address);

  setClock();
  DNS_Resolv(host);
  #ifdef USE_SSL
  tcp_client.setCACert(CACertificate);
  #endif
  tcp_client.setTimeout(10000);

  Serial.printf("[%lu] Connecting to server ", millis());
  if ( tcp_client.connect(host, mqtt_port) == true ) {
    Serial.println(OK_state);
  }
  else {
    Serial.println(ERROR_state);
  }

  Serial.printf("[%lu] Connecting to MQTT broker ", millis());
  if (mqtt.connect(mqtt_client_name, mqtt_user, mqtt_pass) == true) {                      //Soros Debug.
    Serial.println(OK_state);
  }
  else {
    Serial.printf("ERROR State: %d\r\n", mqtt.state()); 
  }

  Serial.printf("MQTT publish:\r\n %s\r\n %s\r\n", mqtt_log, mqtt_power);

  httpUpdater.setup(&httpServer);
  httpUpdater.updateCredentials(mqtt_user, update_passwd);
  httpServer.begin();

  char system_info_json[320] = { '\0' };

  sprintf(system_info_json, init_log_json_frame, 
    WiFi.localIP().toString().c_str(),
    WiFi.gatewayIP().toString().c_str(),
    WiFi.subnetMask().toString().c_str(),
    MAC_Address,
    sw_ver,
    getClock()
  );

  mqtt.publish(mqtt_log, system_info_json);
  mqtt.setCallback(onMqttPublish);
  //mqtt.subscribe(mqtt_cmd);

  mqttQueue = xQueueCreate( mqttQueueSize, sizeof(PZEM_data) );
  if( mqttQueue == NULL ) {
    Serial.println("Error creating the MQTT queue!");
  }

  mqttMutex = xSemaphoreCreateMutex();
  xSemaphoreGive( mqttMutex );

  loopHandle = xTaskGetCurrentTaskHandle();
  vTaskPrioritySet( loopHandle, 10 );

  if( xTaskCreateUniversal( mqttTask, "mqttTask", 4096, NULL, 10, &mqttTaskHandle, 0 ) != pdTRUE ) {
    Serial.println("Error creating the MQTT task!");
  }

  ticker.detach();
  Serial.println("***************************************");
  Serial.printf("[%lu] Loop(s) starting...\r\n", millis());
  LED_L;

}

void loop() {

  static uint32_t measure_timer = millis();
  
  if( millis() - measure_timer >= measure_time ) {

    measure_timer = millis();

    // Read the data from the sensor
    for( uint8_t i = 0; i < POWER_METER_MAX_NUM; i++ ) {

      if( pzem_data[i].isdead == true ) {
        continue;
      }

      pzem_data[i].address = pzem[i].readAddress();
      pzem_data[i].voltage = pzem[i].voltage();
      pzem_data[i].current = pzem[i].current();
      pzem_data[i].power = pzem[i].power();
      pzem_data[i].energy = pzem[i].energy();
      pzem_data[i].frequency = pzem[i].frequency();
      pzem_data[i].pf = pzem[i].pf();

      bitWrite( pzem_data[i].error, 0, !!isnan(pzem_data[i].voltage) );
      bitWrite( pzem_data[i].error, 1, !!isnan(pzem_data[i].current) );
      bitWrite( pzem_data[i].error, 2, !!isnan(pzem_data[i].power) );
      bitWrite( pzem_data[i].error, 3, !!isnan(pzem_data[i].energy) );
      bitWrite( pzem_data[i].error, 4, !!isnan(pzem_data[i].frequency) );
      bitWrite( pzem_data[i].error, 5, !!isnan(pzem_data[i].pf) );

      if( pzem_data[i].error > 0 ) {
        Serial.printf( "Error occured on sensor [%hu] with code [%hu]!\r\n", pzem_data[i].sn, pzem_data[i].error );
        pzem_data[i].isdead_cntr++;
        if( pzem_data[i].isdead_cntr >= 3 ) {
          Serial.printf( "Sensor [%hu] reading is disabled!\r\n", pzem_data[i].sn );
          pzem_data[i].isdead = true;
        }
        
      }
      else {
        pzem_data[i].error = 0;

        if( xQueueSend( mqttQueue, &pzem_data[i], 10 ) != pdTRUE ) {
          Serial.println( "Sendig data to queue failed!" );
        }

      }

      vTaskDelay(10);
    }

  }
  
  vTaskDelay(5);
}

void mqttTask( void *pvParameters ) {

  uint32_t dns_timer = millis();

  while(1) {

    yield();
    ConnectionStatus();

    PZEM_data pzem_data_to_send;
    if( xQueueReceive( mqttQueue, &pzem_data_to_send, 0 ) == pdTRUE ) {

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
      
      
      // todo Send data string to MQTT server.
      if( xSemaphoreTake( mqttMutex, 100 ) == pdTRUE ) {
        mqtt.publish( mqtt_power, measures_json_string );
        xSemaphoreGive( mqttMutex );
      }
      else {                
        Serial.println("MQTT mutex error!");
      }

      Serial.printf( "[%lu] JSON data: %s\r\n", millis(), measures_json_string );
      

    }

    if( xSemaphoreTake( mqttMutex, 100 ) == pdTRUE ) {
      mqtt.loop();
      xSemaphoreGive( mqttMutex );
    }
    else {                
      Serial.println("MQTT mutex error!");
    }

    if( millis() - dns_timer >= 6 * 60 * 60 * 1000 ) {
      dns_timer = millis();
      yield();
      DNS_Resolv(host);
    }

    yield();
    httpServer.handleClient();                    //Sample update URL: http://192.168.51.120:28080/update

    vTaskDelay(5);
  }
  vTaskDelete( NULL );
}

void ConnectionStatus( void ) {
  const uint16_t checking_time = 100;
  static uint32_t checking_timer = 0; 

  if( millis() - checking_timer >= checking_time ) {
    checking_timer = millis();
    
    if( mqtt.connected() == false ) {
      Serial.printf("[%lu] MQTT %s\r\n", millis(), ERROR_state);
      RestartESP();
    }
    
  }
}

void onMqttPublish(const char* topic, uint8_t* payload, int length) {

}

IPAddress DNS_Resolv(const char* host_p) {
  Serial.printf("[%lu] Resolving DNS ", millis());
  IPAddress remote_addr;
  if( WiFi.hostByName(host_p, remote_addr) ) {
    Serial.println(OK_state);
  }
  else {
    Serial.println(ERROR_state);
  }
  return remote_addr;
}

void WifiConnect(void) {

  //Serial.println("Connecting to wifi...");
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  //ClearWifiSettings(&wm);
  if( digitalRead(WiFiRstBtn) == LOW)  {    
    wm.resetSettings();                       //Wifi beallitasok torlese.
    Serial.println("Wifi credentials cleared!"); //Soros debugok.
    RestartESP();
  }

  Serial.printf("[%lu] Connecting to Wifi ", millis());
  yield();

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  wm.setDebugOutput(false);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  wm.setConnectTimeout(30);        //How long to try to connect for before continuing
  wm.setConfigPortalTimeout(90);   //Auto close configportal after n seconds
  wm.setCaptivePortalEnable(true); //false:disable / true:enable captive portal redirection
  wm.setAPClientCheck(true);       //Avoid timeout if client connected to softap
  wm.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
  wm.setShowPassword(false);

  yield();

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if( wm.autoConnect("PowerMeterConfig") == false ) {
    //Serial.println("Failed to connect and hit timeout!");
    Serial.println(ERROR_state);
    //reset and try again.
    RestartESP();
  }
  //if you get here you have connected to the WiFi
  Serial.println(OK_state);
  
  WiFi.mode(WIFI_STA);   //Wifi station mode.

}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println(ERROR_state);
  Serial.printf("[%lu] Entered config mode!\r\n APIP: %s\r\n SSID: %s\r\n", 
    millis(), 
    WiFi.softAPIP().toString().c_str(), 
    myWiFiManager -> getConfigPortalSSID().c_str() 
  );
  Serial.printf("[%lu] Waiting for config portal setup ", millis());
}

// Set time via NTP, as required for x.509 validation
void setClock(void) {  
  configTime(2 * 3600, 0, "0.hu.pool.ntp.org", "1.hu.pool.ntp.org", "2.hu.pool.ntp.org");

  Serial.printf("[%lu] Waiting for NTP time sync", millis());
  time_t nowSecs = time(nullptr);

  uint8_t timeout = 0;  

  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
    timeout++;
    if (timeout >= 20) {
      timeout = 0;    
      Serial.printf(" %s\r\n", ERROR_state);   
      RestartESP();
      return;
    }
  }

  Serial.printf(" %s\r\n", OK_state);  
  struct tm timeinfo;
  //gmtime_r(&nowSecs, &timeinfo);
  getLocalTime(&timeinfo);
  Serial.printf(" Current time: %s", asctime(&timeinfo));
}

const char* getClock(void) {

	const uint8_t time_array_size = 75;

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	static char actualTime[time_array_size] = {'\0'};
	memset(actualTime, 0, sizeof(actualTime));

	sprintf(actualTime, "%04d.%02d.%02d. %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return actualTime;
}

void tick() {                                         // Toggle state
  LED_T;                                              // Set pin to the opposite state
}

void RestartESP(void) {
  Serial.println("Restarting...");
  Serial.flush();                                     // 1s-es delay.
  ESP.restart();                                      // ESP ujrainditasa.
  delay(10000);                                       // 10s-es delay, hogy a restart elott mar ne csinaljon semmit.
}