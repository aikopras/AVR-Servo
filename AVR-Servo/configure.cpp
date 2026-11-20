// *****************************************************************************************************
//
// File:      configure.cpp
// Author:    Aiko Pras
// History:   2025/05/31
//            2025/06/01 ap: first production version  
//            2025/12/08 ap V1.1: Servos 5..8 can also be confugured
//            2026/11/17 ap V1.2: moveServo() moved to MyServo, the Speed CV is used in doSpeed()
//            2026/11/18 ap V1.3: same end behaviour as configuration via PoM: positions are not saved,
//                                after deselection the CV values are restored and the servo returns;
//                                F7 / F8 adjust the Treshold at the straight / diverging side (was swapped)
// 
// Purpose:   Configuration of the servo speed and Tresholds
//
//
// *****************************************************************************************************
#include <Arduino.h>                        // For general definitions
#include <AP_DCC_Decoder_Core.h>            // For the BasicLed class
#include "hardware.h"
#include "configure.h"
#include "myServo.h"                        // Inherits and extends the ServoMoba6 class
#include "servo_CVs.h"

extern MyServo servo[MAX_NUMBER_OF_SERVOS]; // Should be instantiated in main()
extern BasicLed configLed;

#define F0  (bitRead(F0F4, 4))
#define F1  (bitRead(F0F4, 0))
#define F2  (bitRead(F0F4, 1))
#define F3  (bitRead(F0F4, 2))
#define F4  (bitRead(F0F4, 3))
#define F5  (bitRead(F5F8, 0))
#define F6  (bitRead(F5F8, 1))
#define F7  (bitRead(F5F8, 2))
#define F8  (bitRead(F5F8, 3))
#define F9  (bitRead(F9F12, 0))
#define F10 (bitRead(F9F12, 1))
#define F11 (bitRead(F9F12, 2))
#define F12 (bitRead(F9F12, 3))
#define F13 (bitRead(F13F20, 0))
#define F14 (bitRead(F13F20, 1))
#define F15 (bitRead(F13F20, 2))
#define F16 (bitRead(F13F20, 3))
#define F17 (bitRead(F13F20, 4))
#define F18 (bitRead(F13F20, 5))
#define F19 (bitRead(F13F20, 6))
#define F20 (bitRead(F13F20, 7))


// *****************************************************************************************************
// To enter configuration mode
// To enter configuration mode, push F9 or F19 multiple times on and off, within a certain time interval.
// The number of times is determined by the F9_ATTEMPTS_REQUIRED define. The interval in which these 
// pushes are needed, is given by the F9_ATTEMPTS_INTERVAL define. Time is in milliseconds. If this
// time is exceeded, reinitialise to start over.
// The last push should be F9 or F19 = ON, to ensure config mode is left once the handheld shows
// F9 = OFF and F19 = OFF
// *****************************************************************************************************
bool Configure::checkStart(uint8_t value1, uint8_t value2) {
  F9F12 = value1;
  F13F20 = value2;
  if ((F9 || F19 ) && (F9_AttemptsLeft == F9_ATTEMPTS_REQUIRED)) {
    // F9 or F19 is pushed ON for the first time.
    F9_TimeFirstPush = millis();
    return 0;
  }
  // is this push still within the time interval
  if ((millis() - F9_TimeFirstPush) > F9_ATTEMPTS_INTERVAL) {
    // Too long. Reinitialise to allow a next configuration attempt
    F9_TimeFirstPush = millis();
    F9_AttemptsLeft = F9_ATTEMPTS_REQUIRED;
    return 0;
  }
  if (F9_AttemptsLeft == 0) {                 // switch to config mode
    configState = Ready;
    configLed.turn_on();
    return 1;
  }
  if (!F9 && !F19  && F9_AttemptsLeft > 0) {  // F9 and F19 are both OFF?
    F9_AttemptsLeft--;
  }
  return 0;
}


// *****************************************************************************************************
// Called from the main loop, once we are in config mode
// *****************************************************************************************************
void Configure::setF0F4(uint8_t value) {F0F4 = value;}
void Configure::setF5F8(uint8_t value) {F5F8 = value;}
void Configure::setF9F12(uint8_t value) {F9F12 = value;}
void Configure::setF13F20(uint8_t value) {F13F20 = value;}


void Configure::setSpeed(uint8_t speed, bool dir) {
  locoSpeed = speed;
  locoDirection = dir;                // True = Forward / False = Reverse
}


bool Configure::checkConfig() {
  // Runs the state machine
  switch (configState) {
    case Waiting:  
    break;
    case Ready: return doReady(); // Should we stay in config mode?
    break;
    case Verify: doVerify();
    break;
    case Set: doSet();
    break;
    case Restore: doRestore();
    break;
    case Middle: doMiddle();
    break;
    case Speed: doSpeed();
    break;
    case TresholdStraight: doTresholdStraight();
    break;
    case TresholdDiverging: doTresholdDiverging();
    break;
  };
  return 1; // stay in config mode
}


// *****************************************************************************************************
// Local methods: State machine states 
// *****************************************************************************************************
bool Configure::doReady() {
  // Listens to F9 or F19, to leave configuration mode.
  if (!F9 && !F19) {
    F9_AttemptsLeft = F9_ATTEMPTS_REQUIRED;      // reinitialise to allow a next configuration attempt
    configState = Waiting;
    configLed.turn_off();
    return 0;  // we leave configuration mode
  } 
  // Listens to F1 .. F8, to determine which servo we are operating on
  selectedServo = validServoFromF();
  if (selectedServo < MAX_NUMBER_OF_SERVOS) {
    configState = Verify; 
    numberOfMoves = NUMBER_OF_SERVO_MOVES;
    startCurve = servo[selectedServo].previousCurve;   // To return to the start position (see doRestore())
    returnNeeded = false;
  }
  return 1;  // We stay in configuratio mode
}

void Configure::doVerify() {
  // We move the selected servo a couple of times, to verify the right one is selected
  // Instead of set(), we use moveServo(), to avoid the relais from switching on and off
  if (numberOfMoves > 0) {
    if (servo[selectedServo].movementCompleted == true) {
      servo[selectedServo].moveServo();
      numberOfMoves = numberOfMoves - 1;
      //if (numberOfMoves == 0) configState = Set;
    }
  }
  else  // numberOfMoves == 0
    if (servo[selectedServo].movementCompleted == true) {
      configState = Set;
    };  
}


void Configure::doSet() {
  // Positions are not saved during configuration: instead of set(), moveServo() is used.
  // Therefore the relais and RS-Bus feedback need not change.
  if (servoDeselected()) {
    // Wait until a movement (F6, F7, F8) is completed. Then restore the Tresholds and Speed from 
    // the CVs, like after a reset. Values that were not stored via F0 are thus lost.
    // Note: ::Speed is the servo CV (servo_CVs.h). Speed without :: would be the state Speed (7)
    if (servo[selectedServo].movementCompleted == true) {
      servo[selectedServo].setTreshold1(ReadServoMin(selectedServo));
      servo[selectedServo].setTreshold2(ReadServoMax(selectedServo));
      servo[selectedServo].timeMultiplier = ReadServoCV(selectedServo, ::Speed);
      servo[selectedServo].loadCurve(servo[selectedServo].previousCurve);
      // Return to the start position. If the servo is already at that side, it first moves away 
      // and back (after F5 it may stand in the middle). This is the same as with PoM.
      numberOfMoves = 0;
      if (returnNeeded) {
        if (servo[selectedServo].previousCurve != startCurve) numberOfMoves = 1;
          else numberOfMoves = 2;
      }
      configState = Restore;
    }
    return;
  }
  if (F5 || F15) {
    returnNeeded = true;
    configState = Middle;
  }
  if (F6 || F16) {
    returnNeeded = true;
    configState = Speed;
  }
  if (F7 || F17) {
    returnNeeded = true;
    if (servo[selectedServo].movementCompleted == true) {
      if (servo[selectedServo].getPosition() == 1) {
        adjustTreshold1 = atTreshold1();
        configState = TresholdStraight;
      }
      else servo[selectedServo].moveServo();                         // Move to the straight position
    }
  }
  if (F8 || F18) {
    returnNeeded = true;
    if (servo[selectedServo].movementCompleted == true) {
      if (servo[selectedServo].getPosition() == 0) {
        adjustTreshold1 = atTreshold1();
        configState = TresholdDiverging;
      }
      else servo[selectedServo].moveServo();                         // Move to the diverging position
    }
  }
}


void Configure::doRestore() {
  // After deselection, the servo returns to its start position (see doSet())
  if (servo[selectedServo].movementCompleted == false) return;
  if (numberOfMoves > 0) {
    servo[selectedServo].moveServo();
    numberOfMoves--;
    return;
  }
  servo[selectedServo].configPowerSignal();                         // Restore the original idle power values from the CVs
  configState = Ready;
}


void Configure::doMiddle() {
  servo[selectedServo].powerOn();       // writeMicroseconds() requires power
  if (F5 || F15) { 
    servo[selectedServo].writeMicroseconds(1500);
    servo[selectedServo].setTreshold1(1500);
    servo[selectedServo].setTreshold2(1500);
  }
  else configState = Set;  // Return
}


void Configure::doSpeed() {
  if (servo[selectedServo].movementCompleted == true) {
    if (F6 || F16) {
      // The attribute timeMultiplier represents the time stretch factor for a curve.
      // To activate this timeMultiplier, a new curve must be loaded. To decide which
      // curve to load, we compare previousCurve with curveA and curveB 
      uint8_t stretch = locoSpeed;
      if (stretch < 1) stretch = 6;  // the default value
      servo[selectedServo].timeMultiplier = stretch;     
      servo[selectedServo].loadCurve(servo[selectedServo].previousCurve);
      servo[selectedServo].moveServo();
      // Note: ::Speed is the servo CV (servo_CVs.h). Speed without :: would be the state Speed (7)
      if (F0) WriteServoCV(selectedServo, ::Speed, stretch);
    } 
    else {
      servo[selectedServo].timeMultiplier = ReadServoCV(selectedServo, ::Speed); // Restore EEPROM values
      servo[selectedServo].loadCurve(servo[selectedServo].previousCurve);
      configState = Set;  // Return
    }
  }
}


void Configure::doTresholdStraight() {   // F7: end position for the straight track (+, position 1)
  doTreshold(F7 || F17);
}


void Configure::doTresholdDiverging() {  // F8: end position for the diverging track (-, position 0)
  doTreshold(F8 || F18);
}


void Configure::doTreshold(bool keyPressed) {
  // Adjusts the Treshold at the side where the servo stands (see doSet() and atTreshold1()).
  // Which Treshold that is (Min or Max), depends on the curves and InvertServoDir.
  servo[selectedServo].powerOn();        // writeMicroseconds() requires power
  if (!keyPressed) {
    configState = Set;                   // Return
    return;
  }
  if ((millis() - lastConfigTime) > CONFIG_STEP_TIME) {
    lastConfigTime = millis();
    uint16_t treshold;
    if (adjustTreshold1) treshold = servo[selectedServo].getTreshold1();
      else treshold = servo[selectedServo].getTreshold2();
    if (locoSpeed < 10) {
      // As before: forward increases Treshold2 and decreases Treshold1 (if Min < Max: outwards)
      bool increase = locoDirection;
      if (adjustTreshold1) increase = !increase;
      if (increase) treshold = treshold + locoSpeed;
        else treshold = treshold - locoSpeed;
    }
    if ((treshold >= MIN_PULSE_WIDTH) && (treshold <= MAX_PULSE_WIDTH)) {
      servo[selectedServo].writeMicroseconds(treshold);
      if (adjustTreshold1) {
        servo[selectedServo].setTreshold1(treshold);
        if (F0) WriteServoMin(selectedServo, treshold);
      }
      else {
        servo[selectedServo].setTreshold2(treshold);
        if (F0) WriteServoMax(selectedServo, treshold);
      }
    }
  }
}


bool Configure::atTreshold1() {
  // Does the servo stand at the side where it ends at Treshold1 (Min)? Otherwise it stands at
  // Treshold2 (Max). After a movement, loadCurve() recalculates the position where the current curve
  // ends, using the direction of that movement. Same approach as ConfigurePom::atTreshold().
  servo[selectedServo].loadCurve(servo[selectedServo].previousCurve);
  int16_t lastPosition = servo[selectedServo].getLastCurvePosition();
  int16_t delta1 = abs(lastPosition - (int16_t)servo[selectedServo].getTreshold1());
  int16_t delta2 = abs(lastPosition - (int16_t)servo[selectedServo].getTreshold2());
  return (delta1 <= delta2);
}


// *****************************************************************************************************
// Local methods: support
// *****************************************************************************************************
uint8_t Configure::validServoFromF() {
  // Checks if one and only one of the functions F1 .. F4 / F11 .. F14 is selected
  uint8_t Fs =  ((F13F20 & 0b00000011) << 6) + ((F9F12 & 0b00001100) << 2) + (F0F4 & 0b00001111);
  uint8_t result;
  switch (Fs) {
    case   1: result = 0; break;
    case   2: result = 1; break;
    case   4: result = 2; break;
    case   8: result = 3; break;
    case  16: result = 4; break;
    case  32: result = 5; break;
    case  64: result = 6; break;
    case 128: result = 7; break;
    default: result = 255; break;
  };
  // Is the servo associated with the selected function implemented on this board?
  if (result < MAX_NUMBER_OF_SERVOS) return result;
    else return 255;
};


bool Configure::servoDeselected() {
  // Note: an earlier version of this subroutine used a switch / case construct.
  // Unfortunately, the gcc compiler generated an illegal address for the jump table,
  // causing the program to crash and reset. 
  bool result = true;
  if ((selectedServo == 0) && (F1)) result = false;
  if ((selectedServo == 1) && (F2)) result = false;
  if ((selectedServo == 2) && (F3)) result = false;
  if ((selectedServo == 3) && (F4)) result = false;
  if ((selectedServo == 4) && (F11)) result = false;
  if ((selectedServo == 5) && (F12)) result = false;
  if ((selectedServo == 6) && (F13)) result = false;
  if ((selectedServo == 7) && (F14)) result = false;
  return result;
}


