#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <WiFi.h>
#include "SPIFFS.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>
#include "PCF8583.h"
#include <SD.h>



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

AsyncMqttClient mqttClient;

/*******************************************************************************/
/*********************** VARIABLES FOR STORAGE MANAGEMENT **********************/
/*******************************************************************************/

volatile boolean sd_card = false;

File save_File;
File dataFile;
boolean sd_on,data_available,file_open,close_sd;
boolean dataToSave;
int chipSelect = 15;
uint8_t last_minute,button_tmr;
uint16_t sd_chk_tmr;
String tx_data;

/*******************************************************************************/
/*********************** VARIABLES FOR TIME MANAGEMENT **********************/
/*******************************************************************************/
PCF8583 rtc(0xA0);
/*
#define sens1_time  100
#define sens2_time  120
#define sens3_time  150
#define sens4_time  170
#define sens5_time  200
*/
uint8_t year,month,day,hour,minute,second;

String now_date,last_date,tx_date;
String old_time, old_date;


uint16_t  hb_tmr, date_tmr;
uint16_t sens1_tmr, sens2_tmr, sens3_tmr, sens4_tmr, sens5_tmr; // timers used for events trigger


/*******************************************************************************/
/*********************** VARIABLES FOR DISPLAY MANAGEMENT **********************/
/*******************************************************************************/
const int16_t line_step = 18,first_line= 0;
#define bg_color  ST77XX_BLACK
#define bg_color2   ST77XX_CYAN
#define txt_color1 ST77XX_ORANGE
#define txt_color2 ST77XX_YELLOW
#define txt_color3 ST77XX_RED
#define txt_color4 ST77XX_GREEN
#define txt_color5  ST77XX_WHITE
#define con_color  ST77XX_GREEN

// These pins are setting for connection with 1.44" TFT_LCD module.
#define TFT_CS         5
#define TFT_RST        4 
#define TFT_DC         2

// Initialize the 1.44" TFT_LCD.
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

/*******************************************************************************/
/*********************** VARIABLES OF HARDWARE MANAGEMENT **********************/
/*******************************************************************************/
#define hb_time     35
// #define date_tmr  10
#define con_led    25
#define button     32   //  TO REMOVE LATER
#define pin_bat    34   //  TO CHANGE LATER

float vbat=3.6;
int bat_tmr;

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

String get_time();
void show_date();

void init_sd();
void button_isr();

void splash_screen();
//  void show_masq();
void show_masq_2(); // Show screen for files checking
void show_masq_3(); // Show screen for info required
//  void show_time(int16_t c_cord,uint16_t l_cord,int16_t couleur,String);
void disp_masq();

void show_info(int16_t c_cord,uint16_t l_cord,int16_t couleur,String info,char type);
void show_data(int16_t c_cord,uint16_t l_cord,int16_t couleur,float dat,char type);
void clr_data(int16_t c_cord,uint16_t l_cord);
void show_star(int16_t c_cord,uint16_t l_cord,int16_t couleur,char dat,boolean oui );
void show_battery(int x0,int y0,int w,int h,float vb);

/*
void load_credentials();
void save_credentials();
void init_amg88();
void disp_masq();
void show_battery(int x0,int y0,int w,int h,float vb);
void show_eco2(int x,int y);
void show_humidity(int x,int y);
void show_temperature(int x,int y);
void show_tvoc(int x,int y);
void show_particulate(int x,int y,float part);
void get_bme_data();
void read_coef();
boolean check_i2c_adr(byte adr);
void read_credentials();
void i2c_scanner();
*/



/*************************  MAIN FUNCTION  *************************************/
void setup() {
  DynamicJsonDocument doc(1024);

  // put your setup code here, to run once:
  Serial.begin(115200);

  tft.initR(INITR_144GREENTAB); // Init ST7735R chip, green tab
  delay(100);
  splash_screen();
  delay(5000);

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //  mqttClient.onSubscribe(onMqttSubscribe);
  //  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  //  mqttClient.onMessage(onMqttMessage);


  // Screen for checking all configuration files
  show_masq_2();
  delay(1000);
  
  if(!SPIFFS.begin(true)){
    Serial.println("An error has occured while mainting SPIFFS");
    return;
  }


  File file = SPIFFS.open("/config.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    show_info(100,18,txt_color3,"NO",'i');
    return;
  } else {
    Serial.println("Config file is present");
    show_info(100,18,txt_color4,"OK",'i');

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
  }

  

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

  delay(10000);
  

  // Screen for checking all component available
  show_masq_3();
  delay(1000);

  // Initiale the rtc
  rtc.setMode(MODE_CLOCK_32KHZ);
  
  pinMode(chipSelect,OUTPUT);
  pinMode(con_led,OUTPUT);
  digitalWrite(con_led,HIGH);
  button_tmr=150;
  pinMode(button,INPUT_PULLUP);
  pinMode(pin_bat,INPUT);
  //  i2c_scanner();
  

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

/*
  sens1_tmr=50;
  sens2_tmr=60;
  sens3_tmr=70;
  sens4_tmr=80;
  sens5_tmr=100;
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

  //  Display masq
  disp_masq();

  while (1)
  {
    delay(100);

    digitalWrite(con_led,HIGH);
    show_star(78,0,ST77XX_MAGENTA,'*',false);

    // Get updated time for any information to transfert
    now_date=get_time();
    if(last_date != now_date)
    { 
      show_date();
      last_date=now_date;
    }

    // Refresh screen zone for connection management 
    if(hb_tmr)hb_tmr--;
    else
    {
      hb_tmr=hb_time;
      show_star(78,0,ST77XX_MAGENTA,'*',true);
      if(!connected)hb_tmr=15;
    }

    if(bat_tmr)bat_tmr--;
    else
    {
      bat_tmr=100;
      vbat = analogRead(pin_bat)*3.2/4095+0.2;
      vbat *= 2;
      Serial.println("vbat = ");
      Serial.println(vbat);
      show_battery(64,0,10,16,vbat);
      /*
      if(vbat<4.2)vbat += 0.1;
      else vbat = 3.6;
      show_date();
      */

      /*

        tx_data = "{\""+sat_name+"\":{\"date\":\""+now_date+"\",";
        tx_data += "\"vBAT\":{\"value\":";
        tx_data += vbat;
        tx_data += ",\"unit\":\"V\"}}}";

        Serial.println(tx_data);
        
        if(mqtt_on)
        {
          digitalWrite(con_led,LOW);
          packetIdPub1 = mqttClient.publish(mqtt_topic.c_str(), 1, true, tx_data.c_str());
          packetIdPub2=packetIdPub1;
        }
        else
        {
          dataToSave = true;
        }
      */
    }


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

/*    
    if(mqtt_on && data_available && sd_card)
    {
      
      dataFile = SD.open("/datalog.txt");
      if (dataFile) 
      {
        Serial.println("Uploading datalog.txt");
        while (dataFile.available()) 
        {
          texte=dataFile.readStringUntil('\0');
          packetIdPub2 = mqttClient.publish(mqtt_topic.c_str(), 1, true,texte.c_str());
          Serial.println("Information sent");
          Serial.println(texte);
          if(packetIdPub2);
        }
        dataFile.flush();
        dataFile.close();
      //  dataFile = SD.open("/datalog.txt",FILE_APPEND);
        data_count=0;
        data_available=false;
        if(close_sd)sd_card=false;
      }     
    
    }
*/

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

String get_time()
{
  String date;

  year=rtc.getYear();
  month=rtc.getMonth();
  day=rtc.getDay();
  hour=rtc.getHour();
  minute=rtc.getMinute();
  second=rtc.getSecond();
  if(year<10)date +='0';
  date += String(year);
  date +='-';
  if(month<10)date +='0';
  date += String(month);
  date +='-';
  if(day<10)date +='0';
  date += String(day);
  date +=' ';
  if(hour<10)date +='0';
  date += String(hour);
  date +=':';
  if(minute<10)date +='0';
  date += String(minute);
  date +=':';
  if(second<10)date +='0';
  date += String(second);
  return date;  
}

// Initialize the SD card for any storage or data transfert
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


// Wellcome screen 
void splash_screen() 
{
  uint16_t col,lin;
  col=0;
  lin=0;
  tft.setTextWrap(false);
  tft.fillScreen(bg_color);
  tft.setTextColor(txt_color4);
  tft.setTextSize(2);

  tft.setCursor(col, lin);
//         ("01234567890");
  tft.print("AIR QUALITY");
  lin +=18;
  col=5;
  tft.setCursor(col, lin);
  tft.print(" REGENORD  ");
  lin +=18;
  col=0;
  tft.setCursor(col, lin);
  //       ("01234567890");
  tft.print("     & ");
  col=10;
  lin +=18;
  tft.setCursor(col, lin);
  tft.print(" MEGATEC ");
  lin +=18;
  lin +=18;
  lin +=18;
  //lin +=18;
  col=0;
  //tft.setFont()
  tft.setTextColor(txt_color5);
  tft.setCursor(col, lin);
  tft.print(" (C)  2021 ");

  delay(5000);
}

/*
void show_masq()
{
  int16_t col_cord,line_cord;
  tft.fillScreen(bg_color);
  col_cord=0;
  line_cord=first_line;
  tft.setCursor(col_cord,line_cord);
  tft.setTextSize(1,1);
  tft.setTextColor(txt_color2);
  tft.setTextSize(2,2);

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("T : ");

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("RH : ");   

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("CO2 : ");  
  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("VOC : ");

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("P10 : ");

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("P25 : ");   
}
*/

void show_masq_2()
{
  int16_t col_cord,line_cord;
  tft.fillScreen(bg_color);
  col_cord=0;
  line_cord=first_line;
  tft.setCursor(col_cord,line_cord);
  tft.setTextSize(1,1);

  tft.setTextColor(txt_color5);
  tft.setTextSize(2,2);

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("WiFi : ");

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("MQTT: ");   

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("Limit : ");  

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("Tempo : ");

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("Time : ");

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("Bat : ");   
}

void show_masq_3()
{
  int16_t col_cord,line_cord;
  tft.fillScreen(bg_color);
  col_cord=0;
  line_cord=first_line;
  tft.setCursor(col_cord,line_cord);
  tft.setTextSize(1,1);

  tft.setTextColor(txt_color5);
  tft.setTextSize(2,2);

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("AHT21 : ");
  
  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("CCS811: ");   
  
  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("SDS011: ");  
  
  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("RTC : ");
  

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("SD : ");

  line_cord +=line_step;
  tft.setCursor(col_cord,line_cord);
  tft.print("... : ");   
}

/*
  This function displays sensors data on the screen
*/
void show_info(int16_t c_cord,uint16_t l_cord,int16_t couleur,String info,char type)
{
    tft.setTextColor(couleur);
    tft.setTextSize(1,2);
    tft.setCursor(c_cord,l_cord);
    tft.print(info);
    
}

void clr_data(int16_t c_cord,uint16_t l_cord)
{
  uint16_t l;
  l=c_cord+50;
  if(127<l)l=126-c_cord;
  else l=50;
  tft.fillRect(c_cord,l_cord,l,line_step-2,bg_color);
  return;
}
/*
  This function displays sensors data on the screen
*/
void show_data(int16_t c_cord,uint16_t l_cord,int16_t couleur,float dat,char type)
{
    tft.setTextColor(couleur);
//    tft.setTextSize(1,2);  //modifie ce 16-05-2022
    tft.setTextSize(1,2);
    tft.setCursor(c_cord,l_cord);
    {
      if(type=='t')tft.print(String(dat,1));
      else tft.print(String(dat,0));
      switch(type)
      {
        case 't':
        {
          tft.setTextSize(1,1);
          tft.print('*');
          tft.setTextSize(1,2);
          tft.print('C');
          break;
        }
        case 'h':
        {
          tft.print("%");
          break;
        }
        case 'v':
        {
          tft.print("ppb");
          break;
        }
        case 'c':
        {
          tft.print("ppm");
          break;
        }
        case 'p':
        {
          tft.print("hPa");
          break;
        }
      
      }
    }
}

/*
  This function show current time
/*/
void show_time(int16_t c_cord,uint16_t l_cord,int16_t couleur,String time)
{
  tft.setTextSize(1);
  tft.setTextColor(couleur);
  tft.setCursor(c_cord,l_cord);
  tft.print(time);
  
  
}
/*
  This function blinks a purple or green start according to the
  WIFI connection status 
*/
void show_star(int16_t c_cord,uint16_t l_cord,int16_t couleur,char dat,boolean oui )
{
  uint16_t coulr;
  if(oui)
  {
    if(connected) coulr=con_color;
    else coulr= couleur;
  }
  else coulr=bg_color;
  tft.setTextSize(1);
  tft.setTextColor(coulr);
  tft.setCursor(c_cord,l_cord+4);
  tft.print(dat);
  if(mqtt_on) coulr=txt_color4;
  else coulr=txt_color3;
  if(!oui) coulr=bg_color;
  tft.setTextSize(1);
  tft.setTextColor(coulr);
  tft.setCursor(c_cord+8,l_cord+4);
  tft.print("mqtt");
  // For microSD
  if(sd_card) coulr=txt_color4;
  else coulr=txt_color3;
  if(!oui) coulr=bg_color;
  if(close_sd) coulr=bg_color;
  tft.setTextSize(1);
  tft.setTextColor(coulr);
  tft.setCursor(c_cord+36,l_cord+4);
  tft.print("SD");
  
}

void disp_masq()
{
  tft.fillScreen(bg_color);
  tft.fillRect(0,118,32,10,ST77XX_BLUE);
  tft.fillRect(32,118,32,10,ST77XX_GREEN);
  tft.fillRect(64,118,32,10,ST77XX_YELLOW);
  tft.fillRect(96,118,32,10,ST77XX_RED);

  tft.setTextSize(1);

  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(37,120);
  tft.print("Good");
  tft.setCursor(69,120);
  tft.print("High");

  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(5,120);
  tft.print("Low");
  tft.setCursor(101,120);
  tft.print("Bad");

  tft.drawRoundRect(0,18,128,32,3,ST7735_WHITE);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(5,20);
  tft.print("Temp.");
  tft.setCursor(48,20);
  tft.print("Hum.");
  tft.setCursor(80,20);
  tft.print("Pressure");

  tft.drawRoundRect(0,52,128,32,3,ST7735_WHITE);

  //tft.setTextColor(ST7735_WHITE);
  tft.setCursor(88,55);
  tft.print("eCO2");
  tft.setCursor(25,55);
  tft.print("TVOC");

  tft.drawRoundRect(0,86,128,32,3,ST77XX_WHITE);

  tft.setCursor(90,90);
  tft.print("PM10");

  tft.setCursor(25,90);
  tft.print("PM2.5");
       
}

void show_date()
{
  String C_Date, C_Time;

      uint8_t yr = rtc.getYear();
      uint8_t mm = rtc.getMonth();
      uint8_t dd = rtc.getDay();
      uint8_t hr = rtc.getHour();
      uint8_t min = rtc.getMinute();
      uint8_t sec = rtc.getSecond();
      if(yr>99)
      {
        yr=22;
        mm=1;
        dd=1;
      }
      if(hr>23)
      {
        hr=0;
        min=0;
        sec=0;
      }
      if(yr<10)C_Date += '0';
      C_Date += String(yr);
      C_Date +='-';
      if(mm<10)C_Date += '0';
      C_Date += String(mm);
      C_Date +='-';
      if(dd<10)C_Date += '0';
      C_Date += String(dd);

      // show new date
      if(old_date != C_Date){
        show_time(4,2,bg_color,old_date);
        show_time(4,2,ST77XX_CYAN,C_Date);
        old_date=C_Date;
      }
    
      if(hr<10)C_Time+='0';
      C_Time += String(hr);
      C_Time +=':';
      if(min<10)C_Time +='0';
      C_Time += String(min);
      C_Time +=':';
      if(sec<10)C_Time +='0';
      C_Time += String(sec);
    
      // show new time
      if(old_time != C_Time){
        show_time(4,10,bg_color,old_time);
        show_time(4,10,ST77XX_CYAN,C_Time);
        old_time=C_Time;
      }
      now_date=C_Date;
      now_date +=' ';
      now_date +=C_Time; 

//      Serial.printf("Date: %s & Time: %s \n", C_Date, C_Time);
}

void show_battery(int x0,int y0,int w,int h,float vb)
{
  int vx,y,v,col;
  float vy,vz;
  vy=vb-3.6;
  vy=vy/0.6;
  vy=vy*100.0;
  vx=(int)vy;
  col=ST77XX_GREEN;
  if(vx<70) col=ST77XX_YELLOW;
  if(vx<40) col=ST77XX_RED;
  if(vx<20) col=ST77XX_MAGENTA;

  y=(100-vx)*h;
  y = y/100;
  tft.fillRect(x0,y0+2,w,h,bg_color);
  tft.drawRect(x0,y0+2,w,h,col);
  tft.drawRect(x0+w/4,y0,w/2,2,col);
  tft.fillRect(x0+w/4+1,y0+1,w/2-2,4,bg_color);
  if(96<vx)
  {
    tft.fillRect(x0,y0+2,w,h,ST77XX_GREEN);
    tft.fillRect(x0+w/4,y0,w/2,2,ST77XX_GREEN);
  }
  else
  {
    if(vx<10) col=bg_color;
    //tft.drawRect(x0,y0+2,w,h,col);
      tft.fillRect(x0+1,y0+2,w-2,h-y+1,col);
  }
}