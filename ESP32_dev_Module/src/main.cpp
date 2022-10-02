#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "SPIFFS.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

#include <SD.h>

/*******************************************************************************/
/*********************** VARIABLES FOR NETWORK MANAGEMENT **********************/
/*******************************************************************************/
#define con_time 600
uint16_t con_tmr = 0;
volatile bool connected;
volatile bool connecting;
volatile bool mqtt_on;

volatile boolean sd_card = false;

// ssid.c_str() will convert these String to const char*
String ssid, pwd, mqtt_server,mqtt_topic,mqtt_id;
int mqtt_port;

File save_File;

AsyncMqttClient mqttClient;

File dataFile;
boolean sd_on,data_available,file_open,close_sd;
boolean dataToSave;
int chipSelect = 15;
uint8_t last_minute,button_tmr;
uint16_t sd_chk_tmr;
String tx_data;
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



void init_sd()
{
  //data_available=false;
  if (!SD.begin(chipSelect)) 
  {
    Serial.println("Card failed, or not present");
    sd_on=false;
  //  close_sd=true;
  }
  else
  {
    Serial.println("card initialized.");
    sd_card=true;
    sd_on=true;
    close_sd=false;
    file_open=false; // File initialization
    dataFile = SD.open("/datalog.txt");
    if (dataFile) 
    {
      data_available=false;
      if(dataFile.available())
      {
        data_available=true;
        Serial.println("data available for upload");
      }
      dataFile.close();
    }
  }
}
void button_isr()
{
  if(button_tmr == 0)
  {
    button_tmr=50;
//    if (close_sd) close_sd=false;
//    else close_sd=true;
  }

}

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


  pinMode(chipSelect,OUTPUT);
  button_tmr=150;

  init_sd();
/*
  if (!SD.begin(chipSelect)) 
  {
    Serial.println("Card failed, or not present");
    //  show_info(100,90,txt_color3,"NO",'i');
  }
  else
  {
    Serial.println("card initialized.");
    //  show_info(100,90,txt_color4,"Ready",'i');
  }
*/
  delay(10000);

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


    //  dataToSave = true;    // Only activated by item to save (Exp: Sensors, user data, ...)
    
    if(dataToSave && sd_card)
    {
    
      dataFile = SD.open("/datalog.txt", FILE_APPEND);
      Serial.println("File operation");
      if(dataFile)
      {
        tx_data +='\0';
        dataFile.println(tx_data);
        dataFile.close();
        Serial.println("File write ok");
        data_available=true;
        dataToSave=false;
      } 
      else 
      {
        Serial.println("Failed to write");

      }

      // Create another file to save data for each satellite
      dataFile = SD.open("/data.txt", FILE_APPEND);
      Serial.println("Save operation");
      if(dataFile)
      {
        dataFile.println(tx_data);
      //  dataFile.println(String(Json_Buffer));
        dataFile.close();
        Serial.println("Saved ok");
        data_available=true;
        dataToSave=false;
      } 
      else 
      {
        Serial.println("Failed to save");
      }
      if(close_sd)sd_card=false;
    
    }
    else 
    {
      if(close_sd && sd_card)sd_card=false;
    }
    if(sd_chk_tmr)sd_chk_tmr--;
    else 
    {
//      if(!sd_card && !close_sd)
      if(!sd_card )
      {
        init_sd(); 
      }
      sd_chk_tmr=1200;
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