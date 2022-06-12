#include <Arduino.h>
#include <WiFi.h>

/*******************************************************************************/
/*********************** VARIABLES FOR NETWORK MANAGEMENT **********************/
/*******************************************************************************/
#define con_time 600
uint16_t con_tmr = 0;
volatile bool connected;
volatile bool connecting;
// ssid.c_str() will convert these String to const char*
String ssid = "...";
String pwd = "...";

/*******************************************************************************/
/********************* FUNCTION DECLARATION ************************************/
/*******************************************************************************/
void WiFiEvent(WiFiEvent_t event);
void connectToWifi();

/*************************  MAIN FUNCTION  *************************************/
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFi.onEvent(WiFiEvent);
}

/************************** LOOP FUNCTION *************************************/
void loop() {
  // put your main code here, to run repeatedly:
  connected = false;
  connecting = false;

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
          
          con_tmr = 600;
        } else {
          
        }
      }
    }
  }

}

/*******************************************************************************/
/************************ OTHERS FUNCTIONS *************************************/
/*******************************************************************************/
void WiFiEvent(WiFiEvent_t event){
  switch(event){
    case SYSTEM_EVENT_STA_GOT_IP:
    {
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      connected = true;
      connecting = false;
      
      con_tmr = 600;
      break;
    }

    case SYSTEM_EVENT_STA_DISCONNECTED:
    {
      Serial.println("WiFi disconnected");

      connected = false;
      connecting = false;
      
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