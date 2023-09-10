/*************************************************************
  Filename - MBR and GBR switch remote - NodeMCU version
  Description - Designed for special requirement where master switch panel is not easily accessible.
  Requirement -
    > Maintain the device state inside ESP 8266 EEPROM memory.
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
// X (Left - GPIO, Middle - GND, Right - VCC)
// Reciver GPIO pin 
IRrecv IR(D7);

#define fanRelay D3
#define tubeLightRelay D4
#define fanSwitch D5
#define tubeLightSwitch D6

// Tell whether FAN/ Tubeligh are On/ Off
bool isFanOn = false;
bool isFanRunning = false;
bool isTubelightOn = false;
bool isFanSwitchedOnUsingRemote = false;
bool isTimerSet = false;

bool ignoreMemWrite = true;


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
struct Time activeHourStartTime = {0, 1}; // 0.1 am
struct Time activeHourEndTime = {7, 50};    // 7.50 am

// Int turndeviceON [0 to switch OFF the device | 1 to switch ON the device]
void turnDevice(int deviceRelayName, int turndeviceON, bool ignoreMemWrite = false) {
    // Turn ON/ OFF the devices
    digitalWrite(deviceRelayName, turndeviceON ? LOW : HIGH);

    if (!ignoreMemWrite) {
        // Now read/ write from memory
        writeMemory(deviceRelayName == D3 ? fanMemAddr : tubeMemAddr, turndeviceON);
    }
}

// Indicate (boolean) if time is greater/less than given time
bool diffBtwTimePeriod(struct Time start, struct Time end) {
    while (end.minutes > start.minutes) {
      --start.hours;
      start.minutes += 60;
    }

    return (start.hours - end.hours) >= 0;
}

// Return true if current time falls between active hours
bool isActiveHours() {
    // Serial.println(diffBtwTimePeriod({currentTime.hour(), currentTime.minute()}, activeHourStartTime));
    // Serial.println(diffBtwTimePeriod(activeHourEndTime, {currentTime.hour(), currentTime.minute()}));
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

// 5 min for Fan On
int onTimerLimit = 180;
// 2 min for Fan Off
int offTimerLimit = 120;

void onTimerBlock() {
    isTimerSet = true;
    onTimer = now() + onTimerLimit;   // 1 hours = 60 min = 3600 seconds
    // Turn on fan
    turnDevice(fanRelay, 1);
    offTimer = 0;
    Serial.println("onTimer setup done, fan will be on for 3 min");
}

void offTimerBlock() {
    isTimerSet = true;
    offTimer = now() + offTimerLimit;   // 1/2 hours = 30 min = 1800 seconds
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

        // TODO - Read and write the approriate code for buttons
        // Switch On the FAN When IR remote button 1 is pressed
        if (IR.decodedIRData.decodedRawData == 0xF30CFF00) {
            turnDevice(fanRelay, 1);

            // writeMemory(fanSwitchedOnUsingRemoteAddr, 1);
            Serial.println("Button 1 pressed");
            // Set the flag true
            isFanSwitchedOnUsingRemote = true;
            isFanRunning = true;
        }

        // Switch Off the FAN When IR remote button 2 is pressed
        if (IR.decodedIRData.decodedRawData == 0xE718FF00) {
            turnDevice(fanRelay, 0);
            // writeMemory(fanSwitchedOnUsingRemoteAddr, 0);
            Serial.println("Button 2 pressed");
            // Set the flag false
            isFanRunning = false;
            isFanSwitchedOnUsingRemote = false;
        }

        // Switch On the TubeLight When IR remote button 3 is pressed
        if (IR.decodedIRData.decodedRawData == 0xA15EFF00) {
            turnDevice(tubeLightRelay, 1);
            Serial.println("Button 3 pressed");
        }

        // Switch Off the Tubelight When IR remote button 4 is pressed
        if (IR.decodedIRData.decodedRawData == 0xF708FF00) {
            turnDevice(tubeLightRelay, 0);
            Serial.println("Button 4 pressed");
            // isFanSwitchedOnUsingRemote = false;
        }

        IR.resume();
  }

  // Handles all wall switch operations
  // when FAN switch ON
  if (digitalRead(fanSwitch) == LOW && !isFanOn) {
      // Turn ON the device by making relay LOW
      // Turn OFF the device by making relay HIGH
      turnDevice(fanRelay, 1);

      // Indicate that fan is switched on by pressing wall Switch 
      // writeMemory(fanSwitchedOnUsingRemoteAddr, 0);
      Serial.println("Fan switch pressed ON");

      // Set the flag true
      isFanOn = true;
      isFanRunning = isFanOn;
      isFanSwitchedOnUsingRemote = false;
  }

  // when FAN switch OFF
  if (digitalRead(fanSwitch) == HIGH && isFanOn) {
      turnDevice(fanRelay, 0);
      Serial.println("Fan switch pressed OFF");

      // Set the flag false
      isFanOn = false;
      isFanRunning = isFanOn;
      delay(100);
  }

  // when TubeLight switch ON
  if (digitalRead(tubeLightSwitch) == LOW && !isTubelightOn) {
      turnDevice(tubeLightRelay, 1);
      Serial.println("Tubelight switch pressed ON");
      // Set the flag true
      isTubelightOn = true;
  }

  // when TubeLight switch OFF
  if (digitalRead(tubeLightSwitch) == HIGH && isTubelightOn) {
      turnDevice(tubeLightRelay, 0);
      Serial.println("Tubelight switch pressed OFF");
      // Set the flag false
      isTubelightOn = false;
      delay(100);
  }

  // To start night automation, check for active hours
  if (isActiveHours() && isFanSwitchedOnUsingRemote) {
      // when both timer not set
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

  // isFanRunning = isFanOn;
  delay(500);
}