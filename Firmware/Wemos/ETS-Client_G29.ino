/*  Used pins on wemos D1
 *   
 *  set board in IDE to: LOLIN(WEMOS)D1 R2 & mini
 *   
 *   MOSI           D7
 *   CLK            D5
 *   DASHBOARD_LOAD D4
 *   LED_CS         D0
*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include "max7219.h"
#include "led.h"
#include "credentials.h"

#define DASHBOARD_CS    D4
#define LED_CS          D0
#define DIMMER          D3

ESP8266WiFiMulti WiFiMulti;
boolean isETS2 = true;
uint16_t led_oldstate = 0;
//#define USE_SERIAL            Serial

/********************************************
 * SPI                                      *
 *******************************************/
void max7219_write(char command, char data)
{
  digitalWrite(DASHBOARD_CS, LOW);
  SPI.transfer(command);
  SPI.transfer(data);
  digitalWrite(DASHBOARD_CS, HIGH);
}

void led_write(uint16_t data)
{
  digitalWrite(LED_CS, LOW);
  SPI.transfer(data>>8);
  SPI.transfer(data & 0xFF);
  digitalWrite(LED_CS, HIGH);
}

/********************************************
 * parse json                               *
 *******************************************/
void SpeedLimit(String data)
{
  static int oldLimit = 0;
  unsigned long currentMillis = millis();
  static unsigned long previousMillis = 0;
  static uint8_t ledState = 0;
  static uint8_t cnt = 0;
  String SpeedLimit = GetVal(data, "navigation", "speedLimit");
  int actLimit = SpeedLimit.toFloat();
  if(!isETS2) actLimit = round(SpeedLimit.toFloat() * 0.621);

  int speed_01 = 0;
  int speed_10 = 0;

  speed_10 = actLimit / 10;
  speed_01 = actLimit % 10;

  if ((currentMillis - previousMillis >= 100) && cnt)
  {
    previousMillis = currentMillis;
    if (ledState == LOW) 
    {
      max7219_write(DIGIT_0, speed_01);
      max7219_write(DIGIT_4, speed_10);
      ledState = HIGH;
    } 
    else
    {
      max7219_write(DIGIT_0, BLANK);
      max7219_write(DIGIT_4, BLANK);
      ledState = LOW;
    }
    cnt--;
  }
  
  if(actLimit != oldLimit) 
  {
    oldLimit = actLimit;
    cnt = 10;
    ledState = HIGH;
  }
}

void CruiseControl(String data)
{
  String CruiseControlOn = GetVal(data, "truck", "cruiseControlOn");
  String CruiseControlSpeed = GetVal(data, "truck", "cruiseControlSpeed");
  String EngineRPM = GetVal(data, "truck", "engineRpm");
  int rpm = 0;
  int rpm_01 = 0;
  int rpm_10 = 0;
  int Speed = 0;
  int speed_01 = 0;
  int speed_10 = 0;

  if(CruiseControlOn == "true")
  {
    Speed = CruiseControlSpeed.toFloat();
    if(!isETS2) Speed = round(CruiseControlSpeed.toFloat() * 0.621);
    speed_10 = Speed / 10;
    speed_01 = Speed % 10;
    max7219_write(DIGIT_1, speed_10);
    max7219_write(DIGIT_5, speed_01);
  }
  else
  {
    rpm = EngineRPM.toFloat() / 100;
    rpm_10 = rpm / 10;
    rpm_01 = rpm % 10;

    max7219_write(DIGIT_1, rpm_10);
    max7219_write(DIGIT_5, rpm_01);
  }
}

void Speed(String data)
{
  String Speed = GetVal(data, "truck", "speed");
  int TruckSpeed = Speed.toFloat();
  if(!isETS2) TruckSpeed = round(Speed.toFloat() * 0.621);
  if(TruckSpeed < 0) TruckSpeed = TruckSpeed * (-1);
  int speed_100 = TruckSpeed / 100;
  TruckSpeed = TruckSpeed % 100;
  int speed_010 = TruckSpeed / 10;
  int speed_001 = TruckSpeed % 10;

  max7219_write(DIGIT_6, speed_100);
  max7219_write(DIGIT_2, speed_010);
  max7219_write(DIGIT_3, speed_001);
}

String GetVal(String data, String object, String element)
{
  String val = "NO ELEMENT";
  
  //get boundaries of objectblock
  int ObjectStartIdx = data.indexOf(object);
  ObjectStartIdx = data.indexOf("{", ObjectStartIdx) + 1;
  int ObjectEndIdx = data.indexOf("}", ObjectStartIdx);
  if((ObjectStartIdx == -1) || (ObjectEndIdx == -1)) return "NO OBJECT";

  //search element in objectblock
  int ElementStartIdx = data.indexOf(element, ObjectStartIdx);
  if(ElementStartIdx < ObjectEndIdx)
  {
    ElementStartIdx = data.indexOf(":", ElementStartIdx);
    int ElementEndIdx = data.indexOf(",", ElementStartIdx);
    val = data.substring(ElementStartIdx + 1, ElementEndIdx);
    val.replace("\"", "");
    val.replace("}","");
  }
  return val;
}

void ETS2Parser(String data)
{
  uint16_t leds = 0;
  uint16_t brightness = 0;
 
  if(GetVal(data, "game", "gameName") == "ATS")
    isETS2 = false;
  else
    isETS2 = true; 
  Speed(data);
  SpeedLimit(data);
  CruiseControl(data);

  // handle LEDs
  if(GetVal(data, "truck", "lightsBeamLowOn") == "true" ||
     GetVal(data, "truck", "lightsParkingOn") == "true")
    leds |= (1<<LED_1);
  else
    leds &= ~(1<<LED_1); 

  if(GetVal(data, "truck", "fuelWarningOn") == "true")
    leds |= (1<<LED_2);
  else
    leds &= ~(1<<LED_2); 

  if(GetVal(data, "truck", "parkBrakeOn") == "true")
    leds |= (1<<LED_3);
  else
    leds &= ~(1<<LED_3); 

  if(GetVal(data, "truck", "lightsBeaconOn") == "true")
    leds |= (1<<LED_4);
  else
    leds &= ~(1<<LED_4); 

  if(GetVal(data, "truck", "blinkerLeftOn") == "true" &&
     GetVal(data, "truck", "blinkerRightOn") == "true")
    leds |= (1<<LED_5);
  else
    leds &= ~(1<<LED_5); 

  if(GetVal(data, "truck", "airPressureWarningOn") == "true")
    leds |= (1<<LED_6);
  else
    leds &= ~(1<<LED_6); 

  if(GetVal(data, "trailer", "attached") == "true")
    leds |= (1<<LED_7);
  else
    leds &= ~(1<<LED_7); 

  if(GetVal(data, "truck", "wipersOn") == "true")
    leds |= (1<<LED_8);
  else
    leds &= ~(1<<LED_8); 

  if(GetVal(data, "truck", "motorBrakeOn") == "true")
    leds |= (1<<LED_9);
  else
    leds &= ~(1<<LED_9); 

  if(GetVal(data, "truck", "electricOn") == "true")
    leds |= (1<<LED_10);
  else
    leds &= ~(1<<LED_10); 

  if(leds != led_oldstate)
  {
    led_oldstate = leds;
    led_write(leds);
  }
}

/********************************************
 * mainprogram                              *
 *******************************************/
void setup()
{
#ifdef USE_SERIAL
  USE_SERIAL.begin(115200);
  USE_SERIAL.println();
#endif
  WiFiMulti.addAP(SSID, PWD);
  delay(1000);
  pinMode(DASHBOARD_CS, OUTPUT);
  digitalWrite(DASHBOARD_CS, HIGH);
  pinMode(LED_CS, OUTPUT);
  digitalWrite(LED_CS, HIGH);
  pinMode(DIMMER, OUTPUT);
  SPI.begin ();
  SPI.setClockDivider(SPI_CLOCK_DIV16);
  max7219_write(SCAN_LIMIT,0x06);
  max7219_write(INTENSITY,START_INTENSITY);
  max7219_write(DECODE_MODE, 0xFF);
  max7219_write(SHUTDOWN,DISP_ON);
}

void loop()
{
  // wait for WiFi connection
  if((WiFiMulti.run() == WL_CONNECTED))
  {
    HTTPClient http;

    //USE_SERIAL.print("[HTTP] begin...\n");
    // configure traged server and url
    http.begin("http://192.168.2.106:25555/api/ets2/telemetry"); //HTTP

    //USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      //USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK)
      {
        String payload = http.getString();
        ETS2Parser(payload);
      }
    }
    else
    {
#ifdef USE_SERIAL
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
#endif
    }
    http.end();
  }
}

