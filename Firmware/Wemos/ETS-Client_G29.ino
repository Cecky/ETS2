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
#include <Ticker.h>
#include "max7219.h"
#include "led.h"
#include "credentials.h"

#define CONN_STRING     "http://192.168.178.20:25555/api/buttonbox"
#define DASHBOARD_CS    D4
#define LED_CS          D0
#define DIMMER          D3

#define HOUR_DUSK       20
#define HOUR_DAWN       5
#define PWM_DARK        900
#define BLINKTIME       200

//#define USE_SERIAL            Serial

ESP8266WiFiMulti WiFiMulti;
Ticker tick;
boolean isETS2 = true;
uint16_t led_oldstate = 0;
WiFiClient client;
HTTPClient http;
String data = "0";

struct ParserDataStruct
{
  String GameID;
  String GametimeHour;
  String GametimeMinute;
  String Speed;
  String SpeedLimit;
  String CruiseControlSpeed;
  String EngineRPM;
  bool CruiseControlOn;
  bool ParkingBreak;
  bool MotorBreak;
  bool LightsBeamLowOn;
  bool LightsParkingOn;
  bool WipersOn;
  bool BlinkerLeftOn;
  bool BlinkerRightOn;
  bool BeaconOn;
  bool FuelWarning;
  bool TrailerAttached;
  bool EngineOn;
  bool AirPressureWarningOn;
  bool ElectricOn;
};

/********************************************
 * SPI                                      *
 *******************************************/
void max7219_init()
{
  max7219_write(DISPLAY_TEST, NORMAL_OPERATION);
  max7219_write(NO_OP, NO_OP);
  max7219_write(SCAN_LIMIT,0x06);
  max7219_write(INTENSITY,START_INTENSITY);
  max7219_write(DECODE_MODE, CODE_B_0TO7);
  for(uint8_t i=0; i < 7; i++)
  {
        max7219_write(i + 1, 0x0F);
  }
  max7219_write(SHUTDOWN,DISP_ON);
}

void max7219_welcome()
{
  for(uint8_t i = 0; i < 3;i++)
  {
    max7219_write(DISPLAY_TEST,DISP_ON);
    delay(BLINKTIME);
    max7219_write(DISPLAY_TEST,DISP_OFF);
    delay(BLINKTIME);
  }
    max7219_write(DIGIT_0, 0x0F);
    max7219_write(DIGIT_1, 0x0F);
    max7219_write(DIGIT_2, 0x0A);
    max7219_write(DIGIT_3, 0x0F);
    max7219_write(DIGIT_4, 0x0F);
    max7219_write(DIGIT_5, 0x0F);
    max7219_write(DIGIT_6, 0x0F);
}

void max7219_write(char command, char data)
{
  digitalWrite(DASHBOARD_CS, LOW);
  SPI.transfer(command);
  SPI.transfer(data);
  digitalWrite(DASHBOARD_CS, HIGH);
}

void led_init()
{
  digitalWrite(LED_CS, LOW);
  SPI.transfer(0);
  SPI.transfer(0);
  digitalWrite(LED_CS, HIGH);
}

void led_write(uint16_t data)
{
  digitalWrite(LED_CS, LOW);
  SPI.transfer(data>>8);
  SPI.transfer(data & 0xFF);
  digitalWrite(LED_CS, HIGH);
}

void led_test(uint8_t loops)
{
  for(uint8_t i = 0; i < loops; i++)
  {
      led_write(0xffff);
      delay(BLINKTIME);
      led_write(0x0000);
      delay(BLINKTIME);
  }
}

/********************************************
 * parse json                               *
 *******************************************/
void SpeedLimit(ParserDataStruct ParserData)
{
  static String oldLimit = "";
  unsigned long currentMillis = millis();
  static unsigned long previousMillis = 0;
  static uint8_t ledState = 0;
  static uint8_t cnt = 0;

  if ((currentMillis - previousMillis >= 100) && cnt)
  {
    previousMillis = currentMillis;
    if (ledState == LOW) 
    {
      max7219_write(DIGIT_0, ParserData.SpeedLimit[2] - 0x30);
      max7219_write(DIGIT_4, ParserData.SpeedLimit[1] - 0x30);
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
  
  if(ParserData.SpeedLimit != oldLimit) 
  {
    oldLimit = ParserData.SpeedLimit;
    cnt = 10;
    ledState = HIGH;
  }
}

void CruiseControl(ParserDataStruct ParserData)
{
  if(ParserData.CruiseControlOn)
  {
    max7219_write(DIGIT_1, ParserData.CruiseControlSpeed[1] - 0x30);
    max7219_write(DIGIT_5, ParserData.CruiseControlSpeed[2] - 0x30);
  }
  else
  {
    max7219_write(DIGIT_1, ParserData.EngineRPM[0] - 0x30);
    max7219_write(DIGIT_5, ParserData.EngineRPM[1] - 0x30);
  }
}

void Speed(ParserDataStruct ParserData)
{
  max7219_write(DIGIT_6, ParserData.Speed[0] - 0x30);
  max7219_write(DIGIT_2, ParserData.Speed[1] - 0x30);
  max7219_write(DIGIT_3, ParserData.Speed[2] - 0x30);
}

uint16_t GametimeToLED (ParserDataStruct ParserData)
{
  // stringformat: 0001-01-08T21:09:00Z
  // retval: 0   = Max hell
  // retval: 900 = Max gedimmt
  uint8_t hour = ParserData.GametimeHour.toFloat();
  uint8_t minute = ParserData.GametimeMinute.toFloat();
  uint16_t retval = 0;

  if(hour < HOUR_DAWN) retval = PWM_DARK;

  if(hour >= HOUR_DAWN && hour < HOUR_DUSK)
  {
    retval = (hour - HOUR_DAWN) * 600;
    retval += minute * 10;
    if(retval > PWM_DARK) retval = PWM_DARK;
    retval = PWM_DARK - retval;
  }

  if(hour >= HOUR_DUSK && hour <= 23)
  {
    retval = (hour - HOUR_DUSK) * 600;
    retval += minute * 10;
    if(retval > PWM_DARK) retval = PWM_DARK;
  }

  return retval;
}

uint16 SetLED(uint16 LEDstate, bool data, uint16 led)
{
  if(data)
    return LEDstate |= (1 << led);
  else
    return LEDstate &= ~(1 << led); 
}

uint16 SetLED_OR(uint16 LEDstate, bool data1, bool data2, uint16 led)
{
  if(data1 || data2)
    return LEDstate |= (1 << led);
  else
    return LEDstate &= ~(1 << led); 
}

uint16 SetLED_AND(uint16 LEDstate, bool data1, bool data2, uint16 led)
{
  if(data1 && data2)
    return LEDstate |= (1 << led);
  else
    return LEDstate &= ~(1 << led); 
}

void ETS2Parser()
{
  ParserDataStruct ParserData;
  int idx = 0;
  uint16_t leds = 0;
  uint16_t brightness = 0;

  // if game/server isn't running then leave
  ParserData.GameID = data[idx++]; //E=ETS2, A=ATS, 0=NoConnection
  if(ParserData.GameID == "0") 
    return;
  
  // read telemetrydata
  ParserData.GametimeHour = data[idx++];
  ParserData.GametimeHour += data[idx++];
  ParserData.GametimeMinute = data[idx++];
  ParserData.GametimeMinute += data[idx++];

  ParserData.Speed = data[idx++];
  ParserData.Speed += data[idx++];
  ParserData.Speed += data[idx++];
  
  ParserData.SpeedLimit = data[idx++];
  ParserData.SpeedLimit += data[idx++];
  ParserData.SpeedLimit += data[idx++];

  ParserData.CruiseControlSpeed = data[idx++];
  ParserData.CruiseControlSpeed += data[idx++];
  ParserData.CruiseControlSpeed += data[idx++];
  ParserData.EngineRPM = data[idx++];
  ParserData.EngineRPM += data[idx++];
  
  ParserData.CruiseControlOn = (data[idx++] == '1' ? true : false);
  ParserData.ParkingBreak = (data[idx++] == '1' ? true : false);
  ParserData.MotorBreak = (data[idx++] == '1' ? true : false);
  ParserData.LightsBeamLowOn = (data[idx++] == '1' ? true : false);
  ParserData.LightsParkingOn = (data[idx++] == '1' ? true : false);
  ParserData.WipersOn = (data[idx++] == '1' ? true : false);
  ParserData.BlinkerLeftOn = (data[idx++] == '1' ? true : false);
  ParserData.BlinkerRightOn = (data[idx++] == '1' ? true : false);
  ParserData.BeaconOn = (data[idx++] == '1' ? true : false);
  ParserData.FuelWarning = (data[idx++] == '1' ? true : false);
  ParserData.TrailerAttached = (data[idx++] == '1' ? true : false);
  ParserData.EngineOn = (data[idx++] == '1' ? true : false);
  ParserData.AirPressureWarningOn = (data[idx++] == '1' ? true : false);
  ParserData.ElectricOn = (data[idx++] == '1' ? true : false);

  // lets do stuff with the telemetrydata
  isETS2 = (ParserData.GameID == "E" ? true : false);
  brightness = GametimeToLED(ParserData);

  if(ParserData.EngineOn && ParserData.ElectricOn)
  {
    // Engine is On
    Speed(ParserData);
    SpeedLimit(ParserData);
    CruiseControl(ParserData);
  }
  else if(!ParserData.EngineOn && ParserData.ElectricOn)
  {
    // Engine is off but ignition is on
    max7219_write(DIGIT_0, 0x0F);
    max7219_write(DIGIT_1, 0x0F);
    max7219_write(DIGIT_2, 0x0A);
    max7219_write(DIGIT_3, 0x0F);
    max7219_write(DIGIT_4, 0x0F);
    max7219_write(DIGIT_5, 0x0F);
    max7219_write(DIGIT_6, 0x0F);
  }
  else
  {
    // Engine and ignition are off
    max7219_write(DIGIT_0, 0x0F);
    max7219_write(DIGIT_1, 0x0F);
    max7219_write(DIGIT_2, 0x0F);
    max7219_write(DIGIT_3, 0x0F);
    max7219_write(DIGIT_4, 0x0F);
    max7219_write(DIGIT_5, 0x0F);
    max7219_write(DIGIT_6, 0x0F);
  }
  
  // handle LEDs
  leds = SetLED(leds, ParserData.ParkingBreak, LED_3);
  leds = SetLED(leds, ParserData.MotorBreak, LED_9);
  leds = SetLED_OR(leds, ParserData.LightsBeamLowOn, ParserData.LightsParkingOn, LED_1);
  leds = SetLED(leds, ParserData.WipersOn, LED_8);
  leds = SetLED_AND(leds, ParserData.BlinkerLeftOn, ParserData.BlinkerRightOn, LED_5);
  leds = SetLED(leds, ParserData.BeaconOn, LED_4);
  leds = SetLED(leds, ParserData.FuelWarning, LED_2);
  leds = SetLED(leds, ParserData.TrailerAttached, LED_7);
  leds = SetLED(leds, ParserData.EngineOn, LED_10);
  leds = SetLED(leds, ParserData.AirPressureWarningOn, LED_6);

  if(leds != led_oldstate)
  {
    led_oldstate = leds;
    led_write(leds);
  }

  analogWrite(DIMMER, brightness);
  max7219_write(INTENSITY, 0x0F - (brightness/75));
}

/********************************************
 * mainprogram                              *
 *******************************************/
void setup()
{
#ifdef USE_SERIAL
  USE_SERIAL.begin(115200);
  USE_SERIAL.println("ETS2 Telemetry-Client V2.0");
  USE_SERIAL.println();
#endif
  // setup gpio
  pinMode(DASHBOARD_CS, OUTPUT);
  digitalWrite(DASHBOARD_CS, HIGH);
  pinMode(LED_CS, OUTPUT);
  digitalWrite(LED_CS, HIGH);
  pinMode(DIMMER, OUTPUT);

  // configure spi
  SPI.begin ();
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  // configure analog input
  analogWriteRange(1023);   //stupid new framework defaults to 256!
  
  // init spi devices
  max7219_init();
  led_init();

  // welcome the player
  max7219_welcome();
  led_test(5);
  
  // wifi init  
  WiFiMulti.addAP(SSID, PWD);

  // configure timerinterrupt and set callback
  tick.attach_ms(100, ETS2Parser);

#ifdef USE_SERIAL
  USE_SERIAL.println("[SETUP] OK");
#endif
}

void loop()
{
  // wait for WiFi connection
  if((WiFiMulti.run() == WL_CONNECTED))
  {
    http.begin(client, CONN_STRING); //HTTP

#ifdef USE_SERIAL
    USE_SERIAL.print("[HTTP] GET...\n");
#endif
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
#ifdef USE_SERIAL
      USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
#endif
      // file found at server
      if(httpCode == HTTP_CODE_OK)
      {
        data = http.getString();
      }
    }
    else
    {
#ifdef USE_SERIAL
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      data = "0";
#endif
    }
    http.end();
  }
}
