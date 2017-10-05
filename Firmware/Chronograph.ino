/*  Chronograph: source code for the Chronograph pedalboard clock
    Copyright (C) 2017  DS Guitar Engineering

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

Version:        1.2.0
Modified:       September 27, 2017
Verified:       October 5, 2017
Target uC:      ATtiny85

-----------------------------------------***DESCRIPTION***
The goal of this project is to build a pedalboard clock with countdown timer and stopwatch functionality.
The pedal needs to be as compact as possible therefore it will be in a small plastic enclosure and will only
use one footswitch for all needed user input.  An internal battery will be necessary to maintain a running
clock.  External power will light the display and power the microprocessor.


--------------------------------------------------------------------------------------------------------*/
//include the necessary libraries
#include <TinyWireM.h>
#include <USI_TWI_Master.h>
#include <EEPROM.h>
#include <TinyRTClib.h>

RTC_DS1307 rtc;                 // Set up real time clock

//set pin numbers
const byte FSW = 1;             //footswitch pin, active low
const byte SQW = 4;             //RTC square wave input pin

//set variables
boolean buttonDown = false;     //button's current debounced reading
boolean buttonHeld = false;     //button hold detector
unsigned long buttonTimer = 0;  //timer for debouncing button
int debounce = 40UL;            //debounce time in milliseconds

//unsigned long prevTime = 0;
byte SQWstate;

byte swMin = 0;                 //stores the minutes value of the stopwatch
byte swSec = 0;                 //stores the seconds value of the stopwatch
boolean runSW = false;          //trigger for incrementing the stopwatch in the main loop

byte ctdnMin;                   //stores the minutes value of the countdown timer (EEPROM)
byte ctdnSec;                   //stores the seconds value of the countdown timer (EEPROM)
boolean runTimer = false;       //trigger for decrementing the timer in the main loop

byte clkHour = 0;               //stores the hour of the current time
byte clkMin = 0;                //stores the minute of the current time

byte menu = 0;                  //menu register stores the current position in the menu tree
boolean setupMode = false;      //specifies whether startup should go to setup mode or normal
byte warning;                   //temporarily holds the value of the countdown timer warning

boolean drawColon = false;      //lights the colon segments on the display when true


//*****************************************SETUP**********************************************

void setup()
{
  //check EEPROM for errors and load default values if errors are found
  if(EEPROM.read(0) > 99) EEPROM.update(0, B00000001);    //initialize countdown timer minutes to 1
  if(EEPROM.read(1) > 59) EEPROM.update(1, B00000000);    //initialize countdown timer seconds to 0
  if(EEPROM.read(2) > 1)  EEPROM.update(2, B00000000);    //initialize clock hour format to "12hr"
      //0 = 12hr, 1 = 24hr
  if(EEPROM.read(3) > 1)  EEPROM.update(3, B00000000);    //initialize countdown timer warning to "on"
      //0 = on, 1 = off
  if(EEPROM.read(4) > 10) EEPROM.update(4, B00000001);    //initialize warning time to 1min

  //setup I/O pin modes
  pinMode (FSW, INPUT_PULLUP);                            //footswitch is NO between pin and ground
  pinMode (SQW, INPUT_PULLUP);                            //RTC SQW pin is open drain; requires pullup

  //initialize the RTC
  rtc.begin();

  //initialize the display
  TinyWireM.begin();                                      //setup the I2C bus
  WriteDisp(0x0C, 0x01);                                  //wake up & reset feature register
  WriteDisp(0x0A, 0x0F);                                  //set global intensity to 100%
  WriteDisp(0x0B, 0x03);                                  //set scan limit to display first 4 digits
  WriteDisp(0x09, 0x00);                                  //set all digits to "no decode"

  //if the footswitch is held down, start in setup mode
  if(digitalRead(FSW) == LOW)
  {
    writeLeftRaw(0x5B, 0x0F);     //S,t
    writeRightRaw(0x1C, 0x67);    //u,P
    delay(3000);
    drawColon = true;
    setupMode = true;
    //load the first menu item
    WriteDisp(0x09, 0x0C);                                // do not decode left, decode right
    writeLeftRaw(0x1F, 0x0E);     //b,L
    warning = EEPROM.read(4);
    writeRight(warning);
    delay(500);
    blinkRight(warning);
    //set the next menu item
    menu = B01010000;
  }

  //if the setup mode was not initiated boot in normal mode
  if(setupMode == false)
  {
    lightShow();                                          //display the startup lightshow
    writeClk();                                           //write the current time to the display
  }

}


//*****************************************MAIN LOOP**********************************************

void loop()
{
  //check for a valid button press transition
  if ((millis() - buttonTimer >= debounce) && !buttonDown && (digitalRead(FSW) == LOW))
  //if it has been longer than the debounce interval and the previous reading was "up"
  //and the current reading is "down"
  {
    buttonDown = true;                        //record a valid press
    buttonTimer = millis();                   //restart the debounce timer
  }

  //check for a valid button release transition
  if ((millis() - buttonTimer >= debounce) && buttonDown && (digitalRead(FSW) == HIGH))
  //if it has been longer than the debounce interval and the previous reading was "down"
  //and the current reading is "up"
  {
    buttonDown = false;                      //record a valid release
    buttonTimer = millis();                  //restart the debounce timer

    //when a valid release is detected:

    if (buttonHeld == true)                  //if the release occurs after a button is held
      {
        buttonHeld = false;                  //reset the buttonHeld variable
        goto bailout;                        //skip the tapHandler sequence
      }

    tapHandler();                            //go to the tap menu tree
  }

//bailout from line 226
bailout:

  //call hold functions if footswitches are held for 1 second or longer
  if ((buttonDown == true) && (millis() - buttonTimer >= 1000) && (buttonHeld == false))
    {
      buttonHeld = true;                            //record a button hold
      holdHandler();                                //go to the hold menu tree
    }

  //check the state of the RTC 1Hz square wave***********************************************************************************************************
  SQWstate = digitalRead(SQW);
  delay(50);

  //if the countdown timer is active and it has been 1 second or more
  if ((runTimer == true) && (SQWstate == 1) && (digitalRead(SQW) == 0))
  {
    ctdnSec = --ctdnSec;                            //decrement the countdown timer seconds
    //change seconds to "59" when value falls below zero
    if(ctdnSec == 255) {ctdnSec = 59; ctdnMin = --ctdnMin;}
    //refresh the display if the countdown timer is still running
    if((ctdnSec >= 0) && (ctdnMin >= 0)) {writeLeft(ctdnMin); writeRight(ctdnSec);}
    if(ctdnMin < warning) ClearDisp(490);           //blink display if the threshold has been crossed
    //if the countdown timer has expired
    if((ctdnSec == 0) && (ctdnMin == 0))
    {
      runTimer = false;                             //diable the countdown timer
      menu = B00000001;
    }

  }

  //if the stopwatch is active and it has been 1 second or more
  if ((runSW == true) && (SQWstate == 1) && (digitalRead(SQW) == 0))
  {
    swSec = ++swSec;                                //increment the stopwatch seconds
    if(swSec > 59) {swSec = 0; swMin = ++swMin;}    //change the seconds to zero if the value passes 59
    writeLeft(swMin);                               //refresh the display
    writeRight(swSec);                              //"
    if((swSec == 0) && (swMin == 90))               //if the stopwatch reaches 90 minutes and 0 seconds
    {
      runSW = false;                                //disable the stopwatch
      swSec = 0;                                    //reset the stopwatch seconds value
      swMin = 0;                                    //reset the stopwatch minutes value
      menu = B00001010;
    }
  }

  //if the clock is selected, update the current time
  if (menu == 0)
    {
      DateTime now = rtc.now();                     //get the RTC info
      if (clkMin != now.minute())                   //if the minute has changed
      {
        writeClk();                                 //update the clock
      }
    }

}                                                   //end of the main loop


//***********************************************************************************************************************
//-------------------------------------------------------SUBROUTINES-----------------------------------------------------
//***********************************************************************************************************************

//This routine handles footswitch taps
void tapHandler()
{
  switch (menu) {
    //NORMAL MENU---------------------------------------------------
    case B00000001:
      //countdown timer root
        //select countdown timer
        ctdnMin = EEPROM.read(0);                               //load the countdown timer minute from memory
        ctdnSec = EEPROM.read(1);                               //load the countdown timer seconds from memory
        drawColon = true;                                       //enable the colon
        WriteDisp(0x09, 0x0F);                                  //decode all 4 digits
        writeLeft(ctdnMin);                                     //write the minutes to the left side
        writeRight(ctdnSec);                                    //write the seconds to the right side
        delay(500);
        blinkLeft(ctdnMin);                                     //blink the minutes
        menu = B00000010;
      break;
    case B00000010:
      //select countdown timer
        //increment timer minutes
        ctdnMin = ++ctdnMin;                                    //increment the countdown timer minutes
        if(ctdnMin > 99) {ctdnMin = 0;}                         //if the minutes value passes 99, make it 0
        writeLeft(ctdnMin);                                     //write the minutes to the display
        menu = B00000010;
      break;
    case B00000100:
      //increment timer seconds
        //increment timer seconds
        ctdnSec = ++ctdnSec;                                    //increment the countdown timer seconds
        if(ctdnSec > 59) {ctdnMin = ++ctdnMin; ctdnSec = 0;}    //if the seconds pass 59, make it zero and
                                                                //  increment minutes
        if(ctdnMin > 99) {ctdnMin = 0;}                         //if the minutes value passes 99, make it 0
        writeLeft(ctdnMin);                                     //write the minutes to the left side
        writeRight(ctdnSec);                                    //write the seconds to the right side
        menu = B00000100;
      break;
    case B00000111:
      //Timer Start
        //pause timer
        runTimer = !runTimer;                                   //toggle the variable to pause/resume the timer
      break;
    case B00001010:
      //Stopwatch Root
        //select stopwatch and initialize
        swMin = 0;                                              //set the stopwatch minutes to zero
        swSec = 0;                                              //set the stopwatch seconds to zero
        drawColon = true;                                       //enable the colon
        WriteDisp(0x09, 0x0F);                                  //decode all 4 digits
        writeLeft(swMin);                                       //write the minutes to the left side
        writeRight(swSec);                                      //write the seconds to the right side
        menu = B00001011;
      break;
    case B00001011:
      //Initialize Stopwatch
        //start stopwatch
        runSW = true;                                           //enable the stopwatch
        menu = B00001100;
      break;
    case B00001100:
      //start Stopwatch
        //pause/continue stopwatch
        runSW = !runSW;                                         //toggle the variable to pause/resume the stopwatch
      break;


    //SETUP MENU--------------------------------------------------------------------
    case B00010000:
      //setup mode selected
        //change clock hour
        clkHour = ++clkHour;                      //increment the clock hour
        if(EEPROM.read(2) == 0)                   //if the clock is in 12 hour format
        {
          if(clkHour > 12) clkHour = 1;           //change the hour to 1 when 12 is passed
        }
        if(EEPROM.read(2) >= 1)                   //if the clock is in 24 hour format
        {
          if(clkHour > 23) clkHour = 0;           //change the hour to 0 when 23 is passed
        }
        writeLeft(clkHour);                       //write the hour value to the left side of the display
      break;
    case B00100000:
      //change clock hour
        //change clock minute
        clkMin = ++clkMin;                        //increment the clock minute
        if(clkMin > 59) clkMin = 0;               //if the clock minute passes 59, make it 0
        writeRight(clkMin);                       //write the minutes value to the right side of the display
        //update RTC on hold
      break;
    case B00110000:
      //change clock minute
        //change clock hour format
        if(EEPROM.read(2) == 0)                   //if the clock is in 12 hour format
        {
          EEPROM.update(2,1);                     //change the format to 24-hour
          writeRight(24);                         //write "24" to the right side of the display
          break;
        }
        if(EEPROM.read(2) >= 1)                   //if the clock is in 24 hour format
        {
          EEPROM.update(2,0);                     //change the format to 12-hour
          writeRight(12);                         //write "12" to the right side of the display
        }
      break;
    case B01010000:
      //enable or disable the timer warning
        //change the warning time
        warning = ++warning;                      //increment the warning time
        if(warning > 10) warning = 0;             //roll over to 0 when the value passes 10
        writeRight(warning);                      //write the warning time to the right side of the display
      break;
  }
}


//This routine handles footswitch holds---------------------------------------------------------------------
void holdHandler()
{
  switch (menu) {
    //NORMAL MENU-------------------------------------------------------
    case B00000000:
      //current time
        //go to countdown timer root
        drawColon = false;                        //disable the colon
        WriteDisp(0x09, 0x00);                    //set all digits to "no decode"
        writeLeftRaw(0x4E, 0x0F);     //C,t
        writeRightRaw(0x3D, 0x15);    //d,n
        WriteRTC(0x0E, B00000000);                //enable the RTC's SQW output at a frequency of 1Hz****************************************************
        menu = B00000001;
      break;
    case B00000001:
      //countdown timer root
        //go to stopwatch root
        drawColon = false;                        //disable the colon
        WriteDisp(0x09, 0x00);                    //set all digits to "no decode"
        writeLeftRaw(0x4E, 0x0F);     //C,t
        writeRightRaw(0x1C, 0x67);    //u,P
        menu = B00001010;
      break;
    case B00000010:
      //increment timer minutes
        //accept timer minutes
        blinkRight(ctdnSec);                      //blink the countdown timer seconds
        menu = B00000100;
      break;
    case B00000011:
      //accept timer minutes
        //accept timer seconds
        EEPROM.update(0, ctdnMin);                //write the countdown timer minutes to memory if different
        EEPROM.update(1, ctdnSec);                //write the countdown timer seconds to memory if different
        menu = B00000101;
      break;
    case B00000100:
      //increment timer seconds
        //accept timer seconds
        ClearDisp(500);                           //clear the display and delay for 500 milliseconds
        drawColon = true;                         //enable the colon
        writeLeft(ctdnMin);                       //write the countdown timer minutes to the left side
        writeRight(ctdnSec);                      //write the countdown timer seconds to the right side
        EEPROM.update(0, ctdnMin);                //write the countdown timer minutes to memory if different
        EEPROM.update(1, ctdnSec);                //write the countdown timer seconds to memory if different
        warning = EEPROM.read(4);                 //read the blink warning time from memory
        menu = B00000101;
      break;
    case B00000101:
      //accept timer seconds
        //start timer
        runTimer = true;                          //enable the countdown timer
        menu = B00000111;
      break;
    case B00000110:
      //Timer ready
        //start timer
        runTimer = true;                          //enable the countdown timer
        menu = B00000111;
      break;
    case B00000111:
      //Timer Start
        //stop and clear timer
        runTimer = false;                         //disable the countdown timer
        ctdnMin = EEPROM.read(0);                 //load the countdown timer minutes from memory
        ctdnSec = EEPROM.read(1);                 //load the countdown timer seconds from memory
        writeLeft(ctdnMin);                       //write the minutes to the left side of the display
        writeRight(ctdnSec);                      //write the seconds to the right side of the display
        menu = B00000001;
      break;
    case B00001000:
      //Timer Pause
        //stop and clear timer
        runTimer = false;                         //disable the countdown timer
        ctdnMin = EEPROM.read(0);                 //load the countdown timer minutes from memory
        ctdnSec = EEPROM.read(1);                 //load the countdown timer seconds from memory
        writeLeft(ctdnMin);                       //write the minutes to the left side of the display
        writeRight(ctdnSec);                      //write the seconds to the right side of the display
        menu = B00000001;
      break;
    case B00001010:
      //Timer Stop/Reset
        //go to clock
        ClearDisp(50);                            //clear the display and delay for 50 milliseconds
        drawColon = true;                         //enable the colon
        menu = B00000000;
        writeClk();                               //write the current time to the display
        WriteRTC(0x07, B00000100);                //disable the RTC's SQW output******************************************************************************
      break;
    case B00001011:
      //stopwatch Stop/Reset
        //go to clock
        ClearDisp(50);                            //clear the display and delay for 50 milliseconds
        drawColon = true;                         //enable the colon
        menu = B00000000;
        writeClk();                               //write the current time to the display
        WriteRTC(0x07, B00000100);                //disable the RTC's SQW output******************************************************************************
      break;
    case B00001100:
      //stopwatch start
        //stopwatch stop/reset
        swMin = 0;                                //reset the stopwatch minutes
        swSec = 0;                                //reset the stopwatch seconds
        writeLeft(swMin);                         //write the minutes to the left side of the display
        writeRight(swSec);                        //write the seconds to the right side of the display
        runSW = false;                            //disable the stopwatch
        menu = B00001011;
      break;

    //SETUP MENU---------------------------------------------------------------------------
    case B00010000:
      //setup mode selected
        //accept clock hour
        blinkRight(clkMin);                       //blink the clock minutes
        menu = B00100000;
      break;
    case B00100000:
      //accept clock hour
        //accept clock changes
        //RTC is always operating in 24-hour format.  uC converts to 12-hr format if necessary
        rtc.adjust(DateTime(2016, 8, 31, clkHour, clkMin, 0));  //write new time to RTC
        WriteDisp(0x09, 0x0C);                                  //do not decode left, decode right
        writeLeftRaw(0x1F, 0x0E);     //b,L
        warning = EEPROM.read(4);
        writeRight(warning);
        delay(500);
        blinkRight(warning);
        menu = B01010000;
      break;
    case B00110000:
      //accept clock minute
        //accept clock hour format
        //update RTC hour format

        //load next menu item
        writeClk();
        delay(500);
        blinkLeft(clkHour);
        menu = B00010000;
      break;
    case B01010000:
      //accept timer warning status
        //accept the warning time
        EEPROM.update(4, warning);
        //load next menu item
        writeLeftRaw(0x17, 0x05);     //h,r
        if(EEPROM.read(2) == 0) writeRight(12);
        if(EEPROM.read(2) >= 1) writeRight(24);
        menu = B00110000;
      break;
  }
}


//This routine writes the current time to the display--------------------------------------------------------------
void writeClk()
{
  DateTime now = rtc.now();                     // Get the RTC info
  clkHour = now.hour();                         // Get the hour
  if(EEPROM.read(2) == 0)
  {
    if(clkHour > 12) clkHour -= 12;             // if 12hr format is selected, convert the hours
    if(clkHour == 0) clkHour = 12;
  }
  clkMin = now.minute();                        // Get the minutes
  drawColon = true;
  WriteDisp(0x09, 0x0F);                        // Set all digits to "BCD decode".
  writeLeft(clkHour);
  writeRight(clkMin);
}


//This routine writes numbers to the left side of the display--------------------------------------------------------
void writeLeft(byte x)
{
  WriteDisp(1, x / 10);

  //if the clock is active and in 12 hour format
  if((clkHour < 10) && (menu == 0 || menu == B00010000 || menu == B00100000) && (EEPROM.read(2) == 0))
  {
    WriteDisp(1, 0x0F);           // turn off the first digit if time is less than 10:00
  }

  if(drawColon == false) {WriteDisp(2, x % 10);}
  if(drawColon == true)
  {
    x = (x % 10);
    x = (x |= B10000000);
    WriteDisp(2, x);
  }
}


//This routine writes numbers to the right side of the display------------------------------------------------------
void writeRight(byte y)
{
  WriteDisp(4, y % 10);
  if(drawColon == false) {WriteDisp(3, y / 10);}
  if(drawColon == true)
  {
    y = (y / 10);
    y = (y |= B10000000);
    WriteDisp(3, y);
  }
}


//This routine writes raw data to the left side of the display------------------------------------------------------
void writeLeftRaw(byte x, byte y)
{
  WriteDisp(1, x);            // write digit 1
  if(drawColon == false) {WriteDisp(2, y);}
  if(drawColon == true)
  {
    y = y |= B10000000;
    WriteDisp(2, y);
  }
}


//This routine writes raw data to the right side of the display--------------------------------------------------------
void writeRightRaw(byte x, byte y)
{
  if(drawColon == false) {WriteDisp(3, x);}
  if(drawColon == true)
  {
    x = x |= B10000000;
    WriteDisp(3, x);
  }
  WriteDisp(4, y);            // write digit 4
}


//This routine blinks the left side of the display--------------------------------------------------------------------
void blinkLeft(byte x)
{
  WriteDisp(1,0x0F);
  WriteDisp(2,0x0F);
  delay(500);
  writeLeft(x);
}


//This routine blinks the right side of the display--------------------------------------------------------------------
void blinkRight(byte x)
{
  WriteDisp(3,0x0F);
  WriteDisp(4,0x0F);
  delay(500);
  writeRight(x);
}


//This routine displays "dSgE ChronogrAPh" during startup--------------------------------------------------------------
void lightShow()
{
  writeLeftRaw(0x3D, 0x5B);     //d,S
  writeRightRaw(0x7B, 0x4F);    //g,E
  delay(1000);
  ClearDisp(500);
  writeLeftRaw(0x4E, 0x17);     //C,h
  writeRightRaw(0x05, 0x1D);    //r,o
  delay(750);
  writeLeftRaw(0x17, 0x05);     //h,r
  writeRightRaw(0x1D, 0x15);    //o,n
  delay(250);
  writeLeftRaw(0x05, 0x1D);     //r,o
  writeRightRaw(0x15, 0x1D);    //n,o
  delay(250);
  writeLeftRaw(0x1D, 0x15);     //o,n
  writeRightRaw(0x1D, 0x7B);    //o,g
  delay(250);
  writeLeftRaw(0x15, 0x1D);     //n,o
  writeRightRaw(0x7B, 0x05);    //g,r
  delay(250);
  writeLeftRaw(0x1D, 0x7B);     //o,g
  writeRightRaw(0x05, 0x77);    //r,A
  delay(250);
  writeLeftRaw(0x7B, 0x05);     //g,r
  writeRightRaw(0x77, 0x67);    //A,P
  delay(250);
  writeLeftRaw(0x05, 0x77);     //r,A
  writeRightRaw(0x67, 0x17);    //P,h
  delay(750);
}

//This routine addresses and sends data to the display driver (write operation)-----------------------------------------
void WriteDisp(byte cmd, byte data) {
  TinyWireM.beginTransmission(0x00);
  TinyWireM.write(cmd);
  TinyWireM.write(data);
  TinyWireM.endTransmission();
}

//This routine addresses and sends data to the RTC chip (write operation)-----------------------------------------------
void WriteRTC(byte cmd, byte data) {
  TinyWireM.beginTransmission(0x68);      //7-bit address for DS1307/3231
  TinyWireM.write(cmd);
  TinyWireM.write(data);
  TinyWireM.endTransmission();
}

//This routine turns off the display for the amount of time defined by "offTime" in milliseconds------------------------
void ClearDisp(int offTime) {
  WriteDisp(0x0C, 0x80);        //shutdown display and keep data
  delay(offTime);
  WriteDisp(0x0C, 0x81);        //resume display and restore data
}
