#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "SPIFFS.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

/*******************************************************************************/
/*********************** VARIABLES FOR NETWORK MANAGEMENT **********************/
/*******************************************************************************/
#define con_time 600
uint16_t con_tmr = 0;
volatile bool connected;
volatile bool connecting;
volatile bool mqtt_on;

// ssid.c_str() will convert these String to const char*
String ssid, pwd, mqtt_server,mqtt_topic,mqtt_id;
int mqtt_port;

File save_File;

AsyncMqttClient mqttClient;

/*******************************************************************************/
/********************* FUNCTION DECLARATION ************************************/
/*******************************************************************************/
void WiFiEvent(WiFiEvent_t event);
void connectToWifi();
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttSubscribe(uint16_t packetId, uint8_t qos);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);


/*************************  MAIN FUNCTION  *************************************/
void setup() {
  DynamicJsonDocument doc(1024);

  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //  mqttClient.onSubscribe(onMqttSubscribe);
  //  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  //  mqttClient.onMessage(onMqttMessage);


  if(!SPIFFS.begin(true)){
    Serial.println("An error has occured while mainting SPIFFS");
    return;
  }


  File file = SPIFFS.open("/config.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  } else {
    Serial.println("Parsing done");
    
    Serial.println("");
    ssid = doc["ssid"].as<String>();
    pwd = doc["password"].as<String>();
    mqtt_server = doc["mqtt_server"].as<String>();
    mqtt_port = doc["mqtt_port"].as<int>();
    mqtt_topic = doc["mqtt_topic"].as<String>();

    Serial.print("ssid :"); Serial.println(ssid);
    Serial.print("pwd: ");  Serial.println(pwd);
    Serial.print("mqtt_server :"); Serial.println(mqtt_server);
    Serial.print("mqtt_port: ");  Serial.println(mqtt_port);
    Serial.print("mqtt_topic: "); Serial.println(mqtt_topic);
    Serial.println();
  }
  file.close();

  save_File = SPIFFS.open("/save.txt");
  if(!save_File){
    Serial.println("Failed to open saved in file for reading");
    return;
  } else {
    while(save_File.available()){
      Serial.print(save_File.readString());
    }
    Serial.println();
  }
  save_File.close();

}

/************************** LOOP FUNCTION *************************************/
void loop() {
  // put your main code here, to run repeatedly:
  connected = false;
  connecting = false;
  mqtt_on = false;
  con_tmr = 600;

  while (1)
  {
    delay(100);
    if(con_tmr) con_tmr--;
    else{
      con_tmr = con_time;
      if(!connecting){
        if(!connected){
          WiFi.disconnect(true);
          connectToWifi();
          
          connecting = true;
          connected = false;
          mqtt_on = false;
          
          con_tmr = 600;
        } else {
          if(!mqtt_on){
            // Connect to Broker
            connectToMqtt();
            con_tmr = 600;
          }
        }
      }
    }
  }

}

/*******************************************************************************/
/************************ OTHERS FUNCTIONS *************************************/
/*******************************************************************************/
void WiFiEvent(WiFiEvent_t event)
{
  switch(event){
    case SYSTEM_EVENT_STA_GOT_IP:
    {
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      connected = true;
      connecting = false;
      mqtt_on = false;
      //  connectToMqtt();
      con_tmr = 600;
      break;
    }
    case SYSTEM_EVENT_STA_DISCONNECTED:
    {
      Serial.println("WiFi disconnected");
      connected = false;
      connecting = false;
      mqtt_on = false;
      con_tmr = 600;
      break;
    }
    default: break;
  }
}

void connectToWifi() 
{
  WiFi.begin(ssid.c_str(), pwd.c_str());

  connecting=true;
  connected=false;

  con_tmr=600;
}

void connectToMqtt() 
{
  String mq_id;
  mqtt_on=false;
  if(connected)
  {
  Serial.println("Connecting to MQTT...");

  mqttClient.setServer(mqtt_server.c_str() , mqtt_port);

  mqttClient.connect();
  }
}

void onMqttConnect(bool sessionPresent) 
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  
  uint16_t packetIdSub = mqttClient.subscribe(mqtt_topic.c_str(), 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
  
  mqtt_on=true;

}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) 
{
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) 
  {
    //xTimerStart(mqttReconnectTimer, 0);
  }
  mqtt_on=false;
  con_tmr=600;
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) 
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) 
{
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  payload: ");
  Serial.println(payload);
  // message = payload;
  
  /*
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
  */

 // Create another file to save data for each satellite
      save_File = SPIFFS.open("/saved.txt", FILE_WRITE);
      Serial.println("Save operation");
      if(save_File){
        save_File.println(payload);
      //  save_File.println(String(Json_Buffer));
        save_File.close();
        Serial.println("Saved ok");
      //  data_available=true;
      //  dataSaved=false;
      } else {
        Serial.println("Failed to save");
      }
}