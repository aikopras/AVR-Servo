//*****************************************************************************************************
//
// File:      servo_position.cpp
// Author:    Aiko Pras
// History:   2025/03/22 
//            2025/06/01 ap: first production version 
//            2025/10/05 ap: V1.1: Constructor changed into init() 
//            2025/12/07 ap: V1.2: Comments and code slightly changed to improve readability 
// 
// To get a high-level understanding of what this code is supposed to do, see servo_position.h.
//
// To make the code easier to read (and thus reduce the chance we make mistakes), we maintain
// an array of (16 bit) indexes into the EEPROM that point to the EEPROM bytes that contain the values
// for the servo positions.  
//
// incrementNumberOfBoots() is called only once, when the first servo movement takes place.
//
// 2025/10/05 ap: V1.1: Originally we used a constructor. The problem with that approach was that the
// constructor executed immediately after processor start-up. If new firmware was uploaded, the EEPROM
// was initially empty, and would be filled later. Reading from EEPROM thus returned invalid values.
// We therefore changed the cinstructor into an init(), which is called from servo::init().
// This approach has as disadvantage that ServoPosition::init is called once per servo, but that is 
// not a serious problem. 
// Another problem was spurious incorrect writing of EEPROM values. This was probably due to power 
// issues as soon as the servo started moving. After EEPROM writes an extra 10ms delay was added,
// to solve this issue.
//
//******************************************************************************************************
#include "servo_position.h"
#include <EEPROM.h>
#include "myServo.h"                        // Inherits and extends the ServoMoba6 class
#include "servo_CVs.h"                      // For the position of the CurveA CV

extern MyServo servo[MAX_NUMBER_OF_SERVOS]; // Should be instantiated in main()

// Instantiate the object for the stored positions
ServoPosition storedPositions;


void ServoPosition::init() {                            // Called for each servo at start up
  firstCall = true;                                     
  numberOfBoots = EEPROM.read(EEPROM_BOOTS_INDEX);      // 1 .. SIZE_CIRCULAR_BUFFER
  // Check the validity of the obtained value. A possible case is that the EEPROM is erased or new. 
  // In addition, we protect against values outside the valid range.
  if (numberOfBoots == 0 || numberOfBoots == 255 || numberOfBoots > SIZE_CIRCULAR_BUFFER) {     
    numberOfBoots = 1;
  }
  calculateservoPositions();                            // the index positions point to the old values
};


uint16_t ServoPosition::getIndex(uint8_t servoNumber) { 
  // Determines where the position for a specific servo is stored
  // numberOfBoots >= 1 / servoNumber = 0...
  uint16_t index = EEPROM_BOOTS_INDEX + numberOfBoots + servoNumber;    // EEPROM location for this servo
  if (index >= EEPROM_SIZE) index = index - SIZE_CIRCULAR_BUFFER;       // Wrap around
  return index;
};


void ServoPosition::incrementNumberOfBoots() {          // Will be called after first setPosition
  firstCall = false;                                    // make sure we are only called once!
  // We have to move all servo's EEPROM position one index to the right
  // For each servo, we use previousCurve to store it in its new EEPROM position
  for (uint8_t servoNr = 0; servoNr < MAX_NUMBER_OF_SERVOS; servoNr++) {
    EEPROM.update(getIndex(servoNr + 1), servo[servoNr].previousCurve);
    delay(10);                                          // Needed to make EEPROM write reliable
  }  
  if (numberOfBoots == SIZE_CIRCULAR_BUFFER)            // Overflow??
    numberOfBoots = 1;                                  // then wrap around
    else numberOfBoots++;                               // else move the buffer index one byte further
  EEPROM.update(EEPROM_BOOTS_INDEX, numberOfBoots);     // Save the new numberOfBoots 
  calculateservoPositions();                            // The index positions point now to the new values 
};


void ServoPosition::calculateservoPositions() {
  // servoNr 0 .. (MAX_NUMBER_OF_SERVOS - 1)
  uint8_t value;
  for (uint8_t servoNr = 0; servoNr < MAX_NUMBER_OF_SERVOS; servoNr++) {
    value = EEPROM.read(getIndex(servoNr));
    if (value == 0 || value == 255) {                   // The Circular buffer has has been reset  
      uint16_t firstCvForThisServo = START_INDEX_SERVO_CVS + servoNr * NUMBER_OF_SERVO_CVS;
      value = EEPROM.read(firstCvForThisServo + CurveA);   // default: start in CurveA
    }
    servoPositions[servoNr] = value;
  };
};


void ServoPosition::saveServoPosition(uint8_t number, uint8_t value) {
  if (firstCall) incrementNumberOfBoots();
  EEPROM.update(getIndex(number), value);
  // The lines below are for debugging only
  // Monitor.print("saveServoPosition. servo: "); Monitor.print(number);
  // Monitor.print(" - index: "); Monitor.print(getIndex(number));
  // Monitor.print(" - value: "); Monitor.println(value);
};


void ServoPosition::clearEEPROMCircularBufferValues() {
  uint16_t i = EEPROM_BOOTS_INDEX;
  while (i < EEPROM_SIZE) {
    EEPROM.update(i, 255);
    i++;
  };
  firstCall = true; 
  numberOfBoots = 1;                                    // the EEPROM is new, and nothing is stored yet.
  calculateservoPositions();                            // initialise these
}


//******************************************************************************************************
void ServoPosition::printEEPROM() {                     // For debugging
  // Monitor.print("");
  Monitor.print("EEPROM_SIZE: ");
  Monitor.println(EEPROM_SIZE);
  Monitor.print("EEPROM Boots index: ");
  Monitor.println(EEPROM_BOOTS_INDEX);
  Monitor.print("Number of Boots: ");
  Monitor.println(numberOfBoots);
  Monitor.print("indexPosition 0: ");
  Monitor.println(getIndex(0));
  Monitor.print("indexPosition 1: ");
  Monitor.println(getIndex(1));
  Monitor.print("Servo0 position: ");
  Monitor.println(servoPositions[0]);
  Monitor.print("Servo1 position: ");
  Monitor.println(servoPositions[1]);
};
