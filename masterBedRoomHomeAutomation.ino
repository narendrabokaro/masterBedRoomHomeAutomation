/*************************************************************
  Filename - MBR and GBR switch remote - NodeMCU version
  Description - Designed for special requirement where master switch panel is not easily accessible.
  Requirement -
    > On/ Off on certain time during night time, only activate when fan switched ON using IR remote
    > POC - Start monitoring the night time room temperature on every hour and write some logic to decide fan off time. 
  version - 3.0.0
 *************************************************************/
// For RTC module (DS1307)
#include "RTClib.h"
#include <IRremote.h>
#include <EEPROM.h>
#include <TimeLib.h>      // Install library - Time by Michael Margolis

// D1 (SCL) and D2 (SDA) allotted to RTC module DS1307 (I2C support)
RTC_DS1307 rtc;
DateTime currentTime;
// IR receiver design - X (Left - GPIO, Middle - GND, Right - VCC)
// IR Reciver GPIO pin connected to D7 of NodeMCU 
IRrecv IR(D7);

#define fanRelay D3
#define tubeLightRelay D4
#define fanSwitch D5
#define tubeLightSwitch D6

// Indicate whether fan/ tubelight switched ON by pressing the wall switch
bool isFanOnByWallSwitch = false;
bool isTubeOnByWallSwitch = false;

// Indicate current fan state
bool isFanRunning = false;
// Indicate whether fan switched ON by pressing the IR remote
bool isFanSwitchedOnUsingRemote = false;
// Indicate timer state
bool isTimerSet = false;
// Decide whether memory write operation required
bool ignoreMemWrite = true;
// Store the ON/ OFF timer
long int onTimer = 0;
long int offTimer = 0;

// Memory addresses to maintain states of all devices
// ESP8266 takes 4 bytes of memory for allocation
// Indicate device state .. possible values = OFF - 0/ ON - 1
int fanMemAddr = 0;
int tubeMemAddr = 4;
// Indicate whether fan ON using IR remote [Possible value = 0/ 1]
// int fanSwitchedOnUsingRemoteAddr = 8;

// Time for various comparision
struct Time {
    int hours;
    int minutes;
};

// Active hours - Mid night 00.10 AM to 07.10 AM
struct Time activeHourStartTime = {0, 30}; // 12.30 pm
struct Time activeHourEndTime = {6, 00};    // 6.00 am

// 1 hours = 60 min = 3600 seconds
int onTimerDuration = 3600;
// 1/2 hours = 30 min = 1800 seconds
int offTimerDuration = 1800;

/*
* @function turnDevice
* @description Turn device ON/ OFF by making relay HIGH/ LOW
* @param {int} deviceRelayName Store the GIPO pin number
* @param {int} turndeviceON [0 to switch OFF the device | 1 to switch ON the device]
* @param {bool} ignoreMemWrite Indicate if we require to skip memory write operation. Default is false
*/
void turnDevice(int deviceRelayName, int turndeviceON, bool ignoreMemWrite = false) {
    // Turn ON/ OFF the devices [Turn ON the device by making relay LOW | Turn OFF the device by making relay HIGH]
    digitalWrite(deviceRelayName, turndeviceON ? LOW : HIGH);

    if (!ignoreMemWrite) {
        // Now read/ write from memory
        writeMemory(deviceRelayName == fanRelay ? fanMemAddr : tubeMemAddr, turndeviceON);
    }
}

// Indicate (boolean) if time is greater/less than given time

/*
* @function diffBtwTimePeriod
* @description Check if time is greater/less than given time
* @param {Time struct} start Store the start time in HH and MM
* @param {Time struct} end Store the end time in HH and MM
* @return {boolean} Indicate whether given time is greater
*/
bool diffBtwTimePeriod(struct Time start, struct Time end) {
    while (end.minutes > start.minutes) {
      --start.hours;
      start.minutes += 60;
    }

    return (start.hours - end.hours) >= 0;
}

// Return true if current time falls between active hours
/*
* @function isActiveHours
* @description Check if current time falls between start time and end time
* @return {boolean} true if current time falls between active hours
* TODO : Open issue - Unable to detect if start time = 22:00 (10.00 PM) and end time is 7:10 (7.10 AM)
*/
bool isActiveHours() {
    return diffBtwTimePeriod({currentTime.hour(), currentTime.minute()}, activeHourStartTime) && diffBtwTimePeriod(activeHourEndTime, {currentTime.hour(), currentTime.minute()});
}

/*
* @function writeMemory
* @description Method use to write on EEPROM memory only if the value is different. it will help to keep write cycle count minimum.
* @param {int} addr Memory Address
* @param {int} writeValue indicate the value to be written on memory location
*/
void writeMemory(int addr, int writeValue) {
    // Write operation only when there is change in value
    if (readMemory(addr) != writeValue) {
        // Serial.println(addr);
        Serial.println(" :: Written on memory with value : ");
        Serial.println(writeValue);
        // Write the memory address
        EEPROM.write(addr, writeValue);
        // Commit the changes
        EEPROM.commit();
    }
}

/*
* @function readMemory
* @description Method use to read from EEPROM memory
* @param {int} addr Memory Address
* @return {int} the value written on given memory location
*/
int readMemory(int addr) {
    // Read the memory address
    return EEPROM.read(addr);
}

/*
* @function actionBasedOnDeviceState
* @description Method use to check if any previouly unclosed device status, if found then turn On the device
*/
void actionBasedOnDeviceState() {
    // Check for the fan last state
    if (readMemory(fanMemAddr) == 1) {
        Serial.println("Found : Earlier Fan was switched ON");
        turnDevice(fanRelay, 1);
    }

    if (readMemory(tubeMemAddr) == 1) {
        Serial.println("Found : Earlier tube light was switched ON");
        turnDevice(tubeLightRelay, 1);
    }
}

// For RTC module setup
void rtcSetup() {
    Serial.println("rtcSetup :: Health status check");
    delay(1000);

    if (!rtc.begin()) {
        Serial.println("rtcSetup :: Couldn't find RTC");
        Serial.flush();
        while (1) delay(10);
    }

    if (!rtc.isrunning()) {
        Serial.println("rtcSetup :: RTC is NOT running, Please uncomment below lines to set the time!");
        // When time needs to be set on a new device, or after a power loss, the
        // following line sets the RTC to the date & time this sketch was compiled
        //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }

    Serial.println("rtcSetup :: RTC is running fine and Current time >");
    currentTime = rtc.now();
    Serial.print(currentTime.hour());
    Serial.print(":");
    Serial.print(currentTime.minute());
    Serial.print(":");
    Serial.print(currentTime.second());
}

void setup() {
    // ESP8266 have 512 bytes of internal EEPROM
    EEPROM.begin(512);
    Serial.begin(9600);

    // Setup the RTC mmodule
    rtcSetup();
    IR.enableIRIn();

    pinMode(fanRelay, OUTPUT);
    pinMode(tubeLightRelay, OUTPUT);

    pinMode(fanSwitch, INPUT_PULLUP);
    pinMode(tubeLightSwitch, INPUT_PULLUP);

    // By Default, all devices switched off 
    digitalWrite(fanRelay, HIGH);
    digitalWrite(tubeLightRelay, HIGH);

    // Turn On devices in case of power restoration 
    actionBasedOnDeviceState();
}

void onTimerBlock() {
    isTimerSet = true;
    onTimer = now() + onTimerDuration;
    // Turn on fan
    turnDevice(fanRelay, 1);
    offTimer = 0;
    Serial.println("onTimer setup done, fan will be on for 3 min");
}

void offTimerBlock() {
    isTimerSet = true;
    offTimer = now() + offTimerDuration;
    // Turn off fan
    turnDevice(fanRelay, 0);
    onTimer = 0;
    Serial.println("offTimer setup done, fan will be Off for 2 min");
}

void loop() {
    // get the fresh time stamp
    currentTime = rtc.now();

    // Handles all Infrared remote operations
    if (IR.decode()) {
        Serial.println(IR.decodedIRData.decodedRawData, HEX);

        // Switch On the FAN When IR remote button 1 is pressed
        if (IR.decodedIRData.decodedRawData == 0xF30CFF00) {
            // Turn fan ON/ OFF and update the memory address
            turnDevice(fanRelay, 1);
            // writeMemory(fanSwitchedOnUsingRemoteAddr, 1);
            // Set the flag true
            isFanSwitchedOnUsingRemote = true;
            isFanRunning = true;
            Serial.println("Button 1 pressed");
        }

        // Switch Off the FAN When IR remote button 2 is pressed
        if (IR.decodedIRData.decodedRawData == 0xE718FF00) {
            // Turn fan ON/ OFF and update the memory address
            turnDevice(fanRelay, 0);
            // writeMemory(fanSwitchedOnUsingRemoteAddr, 0);

            // Fan running status
            isFanRunning = false;
            // Set true if fan is switched On by IR remote
            isFanSwitchedOnUsingRemote = false;

            Serial.println("Button 2 pressed");
        }

        // Switch On the TubeLight When IR remote button 3 is pressed
        if (IR.decodedIRData.decodedRawData == 0xA15EFF00) {
            // Turn tubelight ON/ OFF and update the memory address
            turnDevice(tubeLightRelay, 1);
            Serial.println("Button 3 pressed");
        }

        // Switch Off the Tubelight When IR remote button 4 is pressed
        if (IR.decodedIRData.decodedRawData == 0xF708FF00) {
            // Turn tubelight ON/ OFF and update the memory address
            turnDevice(tubeLightRelay, 0);
            Serial.println("Button 4 pressed");
            // isFanSwitchedOnUsingRemote = false;
        }

        IR.resume();
  }

  // Handles all wall switch operations
  // when FAN switch ON
  if (digitalRead(fanSwitch) == LOW && !isFanOnByWallSwitch) {
      
      // Turn fan ON/ OFF and update the memory address
      turnDevice(fanRelay, 1);
      // writeMemory(fanSwitchedOnUsingRemoteAddr, 0);
      Serial.println("Fan switch pressed ON");

      // Set true if fan switched On by pressing the wall switch
      isFanOnByWallSwitch = true;
      // Fan running status
      isFanRunning = true;
      // Set true if fan is switched On by IR remote
      isFanSwitchedOnUsingRemote = false;
  }

  // when FAN switch OFF
  if (digitalRead(fanSwitch) == HIGH && isFanOnByWallSwitch) {
      // Turn fan ON/ OFF and update the memory address
      turnDevice(fanRelay, 0);
      Serial.println("Fan switch pressed OFF");

      // Set true if fan switched On by pressing the wall switch
      isFanOnByWallSwitch = false;
      // Fan running status
      isFanRunning = false;
      // standard delay of 100ms
      delay(100);
  }

  // when TubeLight switch ON
  if (digitalRead(tubeLightSwitch) == LOW && !isTubeOnByWallSwitch) {
      // Turn tubelight ON/ OFF and update the memory address
      turnDevice(tubeLightRelay, 1);
      // Set true if tubelight switched On by pressing the wall switch
      isTubeOnByWallSwitch = true;
      Serial.println("Tubelight switch pressed ON");
  }

  // when TubeLight switch OFF
  if (digitalRead(tubeLightSwitch) == HIGH && isTubeOnByWallSwitch) {
      // Turn tubelight ON/ OFF and update the memory address
      turnDevice(tubeLightRelay, 0);
      // Set true if tubelight switched On by pressing the wall switch
      isTubeOnByWallSwitch = false;
      Serial.println("Tubelight switch pressed OFF");
      // Standard delay 100ms
      delay(100);
  }

  // To start night automation, check for active hours
  if (isActiveHours() && isFanSwitchedOnUsingRemote) {
      // when timer not set
      if (!isTimerSet) {
          if (isFanRunning) {
              offTimerBlock();
          } else {
              onTimerBlock();
          }
      }

      // try to match any timer
      if (isTimerSet && offTimer > 0 && now() >= offTimer) {
          isTimerSet = false;
          Serial.println("offTimer matched, turning fan ON");
          onTimerBlock();
      }

      if (isTimerSet && onTimer > 0 && now() >= onTimer) {
          isTimerSet = false;
          Serial.println("onTimer matched, turning fan OFF");
          offTimerBlock();
      }
  }

  delay(300);
}