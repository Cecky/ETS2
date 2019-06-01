/* ETS2 Buttonbox
 * 
 * Hardware: Arduino Pro Micro
 * 
 * In Arduino-IDE set board to "Teensy2.0"
 * To programm press the resetbutton on the mainbaord to start the bootloader
 * and use the deliverd batchfile to upload the code. 
 * Maybe you need to change the comport!
 * 
 * 2019 Cecky
 */
#include <Bounce.h>
#include "crumb.h"

#define PRESS       1
#define RELEASE     0

Bounce BTN_1 = Bounce(CRUMB_PD3, 10);
Bounce BTN_2 = Bounce(CRUMB_PD2, 10);
Bounce BTN_3 = Bounce(CRUMB_PD1, 10);
Bounce BTN_4 = Bounce(CRUMB_PD0, 10);
Bounce BTN_5 = Bounce(CRUMB_PD4, 10);
Bounce BTN_6 = Bounce(CRUMB_PC6, 10);
Bounce BTN_7 = Bounce(CRUMB_PD7, 10);
Bounce BTN_8 = Bounce(CRUMB_PE6, 10);
Bounce BTN_9 = Bounce(CRUMB_PB4, 10);
Bounce BTN_10 = Bounce(CRUMB_PB5, 10);
Bounce BTN_11 = Bounce(CRUMB_PB1, 10);
Bounce BTN_12 = Bounce(CRUMB_PB3, 10);
Bounce BTN_13 = Bounce(CRUMB_PB2, 10);
Bounce BTN_14 = Bounce(CRUMB_PB6, 10);

boolean Startup_OK = false;

void setup()
{
  pinMode(CRUMB_PB1, INPUT_PULLUP);
  pinMode(CRUMB_PB2, INPUT_PULLUP);
  pinMode(CRUMB_PB3, INPUT_PULLUP);
  pinMode(CRUMB_PB4, INPUT_PULLUP);
  pinMode(CRUMB_PB5, INPUT_PULLUP);
  pinMode(CRUMB_PB6, INPUT_PULLUP);
  
  pinMode(CRUMB_PC6, INPUT_PULLUP);

  pinMode(CRUMB_PD0, INPUT_PULLUP);
  pinMode(CRUMB_PD1, INPUT_PULLUP);
  pinMode(CRUMB_PD2, INPUT_PULLUP);
  pinMode(CRUMB_PD3, INPUT_PULLUP);
  pinMode(CRUMB_PD4, INPUT_PULLUP);
  pinMode(CRUMB_PD7, INPUT_PULLUP);

  pinMode(CRUMB_PE6, INPUT_PULLUP);
}

void loop()
{
  if(millis() > 1000) Startup_OK = true;
  
  BTN_1.update();
  BTN_2.update();
  BTN_3.update();
  BTN_4.update();
  BTN_5.update();
  BTN_6.update();
  BTN_7.update();
  BTN_8.update();
  BTN_9.update();
  BTN_10.update();
  BTN_11.update();
  BTN_12.update();
  BTN_13.update();
  BTN_14.update();

//############ falling edge #############
  if (BTN_1.fallingEdge() && Startup_OK) Joystick.button(1, PRESS);
  if (BTN_2.fallingEdge() && Startup_OK) Joystick.button(2, RELEASE);
  if (BTN_3.fallingEdge() && Startup_OK) Joystick.button(3, RELEASE);
  if (BTN_4.fallingEdge() && Startup_OK) Joystick.button(4, PRESS);
  if (BTN_5.fallingEdge() && Startup_OK) Joystick.button(5, PRESS);
  if (BTN_6.fallingEdge() && Startup_OK) Joystick.button(6, PRESS);
  if (BTN_7.fallingEdge() && Startup_OK) Joystick.button(7, PRESS);
  if (BTN_8.fallingEdge() && Startup_OK) Joystick.button(8, PRESS);
  if (BTN_9.fallingEdge() && Startup_OK) Joystick.button(9, PRESS);
  if (BTN_10.fallingEdge() && Startup_OK) Joystick.button(10, PRESS);
  if (BTN_11.fallingEdge() && Startup_OK) Joystick.button(11, PRESS);
  if (BTN_12.fallingEdge() && Startup_OK) Joystick.button(12, PRESS);
  if (BTN_13.fallingEdge() && Startup_OK) Joystick.button(13, PRESS);
  if (BTN_14.fallingEdge() && Startup_OK) Joystick.button(14, PRESS);

//########### rising edge ###############
  if (BTN_1.risingEdge() && Startup_OK) Joystick.button(1, RELEASE);
  if (BTN_2.risingEdge() && Startup_OK) Joystick.button(2, PRESS);
  if (BTN_3.risingEdge() && Startup_OK) Joystick.button(3, PRESS);
  if (BTN_4.risingEdge() && Startup_OK) Joystick.button(4, RELEASE);
  if (BTN_5.risingEdge() && Startup_OK) Joystick.button(5, RELEASE);
  if (BTN_6.risingEdge() && Startup_OK) Joystick.button(6, RELEASE);
  if (BTN_7.risingEdge() && Startup_OK) Joystick.button(7, RELEASE);
  if (BTN_8.risingEdge() && Startup_OK) Joystick.button(8, RELEASE);
  if (BTN_9.risingEdge() && Startup_OK) Joystick.button(9, RELEASE);
  if (BTN_10.risingEdge() && Startup_OK) Joystick.button(10, RELEASE);
  if (BTN_11.risingEdge() && Startup_OK) Joystick.button(11, RELEASE);
  if (BTN_12.risingEdge() && Startup_OK) Joystick.button(12, RELEASE);
  if (BTN_13.risingEdge() && Startup_OK) Joystick.button(13, RELEASE);
  if (BTN_14.risingEdge() && Startup_OK) Joystick.button(14, RELEASE);

  Joystick.send_now();
}


