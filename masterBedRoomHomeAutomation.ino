/*************************************************************
  Filename - MBR and GBR switch remote - NodeMCU version
  Description - Designed for special requirement where master switch panel is not easily accessible.
  Requirement -
    > Maintain the device state inside ESP 8266 EEPROM memory.
  version - 2.0.0
 *************************************************************/

#include <IRremote.h>
#include <EEPROM.h>

// Reciver GPIO pin
IRrecv IR(D7);

#define fanRelay D3
#define tubeLightRelay D4
#define fanSwitch D5
#define tubeLightSwitch D6

// Tell whether FAN/ Tubeligh are On/ Off
boolean isFanOn = false;
boolean isTubelightOn = false;

// Memory addresses to maintain states of all devices
// ESP8266 takes 4 bytes of memory for allocation
int fanMemAddr = 0;
int tubeMemAddr = 4;

void setup() {
    // ESP8266 have 512 bytes of internal EEPROM
    EEPROM.begin(512);
    Serial.begin(9600);

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

/*
* @function writeMemory
* @description Method used to write on EEPROM memory only if the value is different. it will help to keep write cycle count minimum.
* @param {int} addr Memory Address
* @param {int} writeValue indicate the value to be written on memory location
*/
void writeMemory(int addr, int writeValue) {
    // Write operation only when there is change in value
    if (readMemory(addr) != writeValue) {
        Serial.println("Written on memory with value : ");
        Serial.println(writeValue);
        // Write the memory address
        EEPROM.write(addr, writeValue);
        EEPROM.commit();
    }
}

/*
* @function readMemory
* @description Method used to read from EEPROM memory
* @param {int} addr Memory Address
* @return {int} the value written on given memory location
*/
int readMemory(int addr) {
    // Read the memory address
    return EEPROM.read(addr);
}

/*
* @function actionBasedOnDeviceState
* @description Method used to check if any previouly unclosed device status, if found then turn On the device
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

// Int turndeviceON [0 to switch OFF the device | 1 to switch ON the device]
void turnDevice(int deviceRelayName, int turndeviceON) {
    // Turn ON/ OFF the devices
    digitalWrite(deviceRelayName, turndeviceON ? LOW : HIGH);

    // Now read/ write from memory
    writeMemory(deviceRelayName == D3 ? fanMemAddr : tubeMemAddr, turndeviceON);
}

void loop() {
    // Handles all Infrared remote operations
    if (IR.decode()) {
        Serial.println(IR.decodedIRData.decodedRawData, HEX);

        // TODO - Read and write the approriate code for buttons
        // Switch On the FAN When IR remote button 1 is pressed
        if (IR.decodedIRData.decodedRawData == 0xF30CFF00) {
            turnDevice(fanRelay, 1);
            Serial.println("Button 1 pressed");
        }

        // Switch Off the FAN When IR remote button 2 is pressed
        if (IR.decodedIRData.decodedRawData == 0xE718FF00) {
            turnDevice(fanRelay, 0);
            Serial.println("Button 2 pressed");
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
        }

        IR.resume();
  }

  // Handles all wall switch operations
  if (digitalRead(fanSwitch) == LOW && !isFanOn) {
      // Turn ON the device by making relay LOW
      // Turn OFF the device by making relay HIGH
      turnDevice(fanRelay, 1);
      Serial.println("Fan switch pressed ON");

      // Set the flag true
      isFanOn = true;
  }

  // when FAN switch OFF
  if (digitalRead(fanSwitch) == HIGH && isFanOn) {
      turnDevice(fanRelay, 0);
      Serial.println("Fan switch pressed OFF");

      // Set the flag false
      isFanOn = false;
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

  delay(500);
}
