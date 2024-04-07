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

#define CONN_STRING     "http://192.168.2.107:25555/api/buttonbox"
#define DASHBOARD_CS    D4
#define LED_CS          D0
#define DIMMER          D3

#define HOUR_DUSK       20
#define HOUR_DAWN       5
#define PWM_DARK        900

ESP8266WiFiMulti WiFiMulti;
Ticker tick;
boolean isETS2 = true;
uint16_t led_oldstate = 0;
WiFiClient client;
HTTPClient http;
String data;

//#define USE_SERIAL            Serial

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
  max7219_write(SHUTDOWN,DISP_ON);
}

void max7219_welcome()
{
  for(uint8_t i = 0; i < 11; i++)
  {
    max7219_write(DIGIT_0, i % 10);
    max7219_write(DIGIT_1, i % 10);
    max7219_write(DIGIT_2, i % 10);
    max7219_write(DIGIT_3, i % 10);
    max7219_write(DIGIT_4, i % 10);
    max7219_write(DIGIT_5, i % 10);
    max7219_write(DIGIT_6, i % 10);
    delay(75);
  }
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
      delay(150);
      led_write(0x0000);
      delay(150);
  }
}

/********************************************
 * parse json                               *
 *******************************************/
void SpeedLimit(String SpeedLimit)
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
      max7219_write(DIGIT_0, SpeedLimit[2] - 0x30);
      max7219_write(DIGIT_4, SpeedLimit[1] - 0x30);
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
  
  if(SpeedLimit != oldLimit) 
  {
    oldLimit = SpeedLimit;
    cnt = 10;
    ledState = HIGH;
  }
}

void CruiseControl(String CruiseControlOn, String CruiseControlSpeed, String EngineRPM)
{
  if(CruiseControlOn == "1")
  {
    max7219_write(DIGIT_1, CruiseControlSpeed[1] - 0x30);
    max7219_write(DIGIT_5, CruiseControlSpeed[2] - 0x30);
  }
  else
  {
    max7219_write(DIGIT_1, EngineRPM[0] - 0x30);
    max7219_write(DIGIT_5, EngineRPM[1] - 0x30);
  }
}

void Speed(String Speed)
{
  max7219_write(DIGIT_6, Speed[0] - 0x30);
  max7219_write(DIGIT_2, Speed[1] - 0x30);
  max7219_write(DIGIT_3, Speed[2] - 0x30);
}

uint16_t GametimeToLED (String Hour, String Minute)
{
  // stringformat: 0001-01-08T21:09:00Z
  // retval: 0   = Max hell
  // retval: 900 = Max gedimmt
  uint8_t hour = Hour.toFloat();
  uint8_t minute = Minute.toFloat();
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

uint16 SetLED(uint16 LEDstate, char data, uint16 led)
{
  if(data == '1')
    return LEDstate |= (1<<led);
  else
    return LEDstate &= ~(1<<led); 
}

uint16 SetLED_OR(uint16 LEDstate, String data1, String data2, uint16 led)
{
  if(data1 == "1" || data2 == "1")
    return LEDstate |= (1<<led);
  else
    return LEDstate &= ~(1<<led); 
}

uint16 SetLED_AND(uint16 LEDstate, String data1, String data2, uint16 led)
{
  if(data1 == "1" && data2 == "1")
    return LEDstate |= (1<<led);
  else
    return LEDstate &= ~(1<<led); 
}

void ETS2Parser()
{
  int idx = 0;
  String p1, p2, p3;
  uint16_t leds = 0;
  uint16_t brightness = 0;
  
  p1 = data[idx++]; //E=ETS2, A=ATS, 0=NoConnection
  
  if(p1 == "0") return;
  
  isETS2 = (p1 == "E" ? true : false);

  p1 = data[idx++];
  p1 += data[idx++];
  p2 = data[idx++];
  p2 += data[idx++];
  brightness = GametimeToLED(p1, p2);

  p1 = data[idx++];
  p1 += data[idx++];
  p1 += data[idx++];  
  Speed(p1);

  p1 = data[idx++];
  p1 += data[idx++];
  p1 += data[idx++];  
  SpeedLimit(p1);

  p1 = data[idx++];
  p2 = data[idx++];
  p2 += data[idx++];
  p2 += data[idx++];  
  p3 = data[idx++];
  p3 += data[idx++];
  CruiseControl(p1, p2, p3);
  
  // handle LEDs
  leds = SetLED(leds, data[idx++], LED_3);     // ParkingBreak
  leds = SetLED(leds, data[idx++], LED_9);     // MotorBreak
  p1 = data[idx++];
  p2 = data[idx++];
  leds = SetLED_OR(leds, p1, p2, LED_1);       // LightsOn
  leds = SetLED(leds, data[idx++], LED_8);     // WipersOn
  p1 = data[idx++];
  p2 = data[idx++];
  leds = SetLED_AND(leds, p1, p2, LED_5);      // WarningLights
  leds = SetLED(leds, data[idx++], LED_4);     // BeaconOn
  leds = SetLED(leds, data[idx++], LED_2);     // FuelWarning
  leds = SetLED(leds, data[idx++], LED_7);     // TrailerAttached
  leds = SetLED(leds, data[idx++], LED_10);    // ElectricOn
  leds = SetLED(leds, data[idx++], LED_6);     // AirPressureWarningOn

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
  USE_SERIAL.println("ETS2 Telemetry-Client V1.1");
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
  
  // configure timerinterrupt and set callback
  tick.attach_ms(100, ETS2Parser);

  // init spi devices
  max7219_init();
  led_init();
  
  // wifi init  
  WiFiMulti.addAP(SSID, PWD);
  
  // welcome the player
  max7219_welcome();
  led_test(5);

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
#endif
    }
    http.end();
  }
}
