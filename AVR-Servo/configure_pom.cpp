// *****************************************************************************************************
//
// File:      configure_pom.cpp
// Author:    Aiko Pras
// History:   2026/11/17 ap: first version
//            2026/11/20 ap: the extra LED (LED_CONFIG) is on while a servo is configured, as with the handheld
//
// Purpose:   Configuration of the servo end positions (Tresholds) and speed via PoM
//            See configure_pom.h for details
//
// *****************************************************************************************************
#include <Arduino.h>                        // For general definitions
#include <AP_DCC_Decoder_Core.h>            // For cvCmd, cvProgramming and RSbusConnection
#include "hardware.h"
#include "configure_pom.h"
#include "myServo.h"                        // Inherits and extends the ServoMoba6 class
#include "servo_CVs.h"

extern MyServo servo[MAX_NUMBER_OF_SERVOS]; // Should be instantiated in main()
extern RSbusConnection rsbusPom;            // Defined in AP_DCC_Decoder_Core (CommonFunctions.cpp)
extern BasicLed configLed;                  // The extra LED to show configuration mode (main sketch)

#define NO_SERVO  255


// *****************************************************************************************************
// processMessage() should be called by the main loop for PoM and SM messages, instead of
// cvProgramming.processMessage(). Only CV62, CV63 and CV64 are handled here; all other CVs are
// handled by the AP_DCC_Decoder_Core library.
// *****************************************************************************************************
void ConfigurePom::processMessage(Dcc::CmdType_t cmdType) {
  bool PoM = (cmdType == Dcc::MyPomCmd);
  // CV64 holds MAX_NUMBER_OF_SERVOS, which is a board property (hardware.h).
  // Configuration programs read CV64 to determine the maximum number of servos of this board.
  // PoM and SM may therefore read CV64, but writing is ignored.
  if ((cvCmd.number == EEPROM_MAX_SERVOS_INDEX) && writeCommand()) return;
  // CV62 and CV63 are command CVs that only exist in RAM. They are only used with PoM.
  if ((cvCmd.number == CV_POM_CONFIG_MODE) || (cvCmd.number == CV_POM_CONFIG_SERVO)) {
    if (!PoM) return;
    if (cvCmd.operation == CvAccess::verifyByte) {      // PoM read: reply via RS-Bus address 128
      if (cvCmd.number == CV_POM_CONFIG_SERVO) rsbusPom.send8bits(requestedServo);
      else rsbusPom.send8bits(requestedMode);
    }
    if (cvCmd.operation == CvAccess::writeByte) {       // Invalid values are ignored
      if (cvCmd.number == CV_POM_CONFIG_SERVO) {
        if (cvCmd.value <= MAX_NUMBER_OF_SERVOS) requestedServo = cvCmd.value;
      }
      else if (cvCmd.value <= SpeedMode) requestedMode = cvCmd.value;
    }
    return;
  }
  // All other CVs are handled by the AP_DCC_Decoder_Core library
  cvProgramming.processMessage(cmdType);
  // New Min, Max or Speed values of the selected servo should be used immediately
  if (PoM && (selectedServo != NO_SERVO) && (cvCmd.operation == CvAccess::writeByte)) newServoValue();
}


bool ConfigurePom::writeCommand() {
  if (cvCmd.operation == CvAccess::writeByte) return true;
  if ((cvCmd.operation == CvAccess::bitManipulation) && cvCmd.writecmd) return true;
  return false;
}


// *****************************************************************************************************
// newServoValue() is called after a PoM write, which has already been stored in EEPROM.
// If it was the MinHigh, MaxHigh or Speed CV of the selected servo, the new value is used immediately.
// The PC program should write MinLow before MinHigh, and MaxLow before MaxHigh.
// *****************************************************************************************************
void ConfigurePom::newServoValue() {
  if (cvCmd.number < START_INDEX_SERVO_CVS) return;
  uint16_t index = cvCmd.number - START_INDEX_SERVO_CVS;
  if ((index / NUMBER_OF_SERVO_CVS) != selectedServo) return;   // Not a CV of the selected servo
  switch (index % NUMBER_OF_SERVO_CVS) {
    case MinHigh:
      servo[selectedServo].setTreshold1(ReadServoMin(selectedServo));
      if (holding && (currentMode == MinMode)) {
        servo[selectedServo].powerOn();                         // writeMicroseconds() requires power
        servo[selectedServo].writeMicroseconds(servo[selectedServo].getTreshold1());
      }
    break;
    case MaxHigh:
      servo[selectedServo].setTreshold2(ReadServoMax(selectedServo));
      if (holding && (currentMode == MaxMode)) {
        servo[selectedServo].powerOn();                         // writeMicroseconds() requires power
        servo[selectedServo].writeMicroseconds(servo[selectedServo].getTreshold2());
      }
    break;
    case Speed:                                                 // Will be used for the next movement
      servo[selectedServo].timeMultiplier = ReadServoCV(selectedServo, Speed);
    break;
    default:                                                    // Other CVs: after a restart
    break;
  }
}


// *****************************************************************************************************
// checkConfig() should be called from the main loop as often as possible.
// It waits until the selected servo has completed its movement, before the next step is taken.
// *****************************************************************************************************
void ConfigurePom::checkConfig() {
  // Step 1: if no servo is selected yet, check if a servo should be selected
  if (selectedServo == NO_SERVO) {
    if (requestedServo == 0) return;
    selectedServo = requestedServo - 1;
    startCurve = servo[selectedServo].previousCurve;
    currentMode = NoMode;
    holding = false;
    returnNeeded = false;
    numberOfMoves = 2;                      // Once away and back, to verify the right servo is selected
    configLed.turn_on();
  }
  // Step 2: wait until the servo has completed its movement, and make the moves still to be made
  if (!servo[selectedServo].movementCompleted) return;
  if (numberOfMoves > 0) {
    move();
    numberOfMoves--;
    return;
  }
  // Step 3: should the configuration of this servo end (CV63 = 0 or another servo)?
  // In that case the servo should first return to its start side (as with CV62 = 0)
  bool endConfig = (requestedServo != selectedServo + 1);
  uint8_t mode = requestedMode;
  if (endConfig) mode = NoMode;
  // Step 4: act according to the mode
  switch (mode) {
    case MinMode:
    case MaxMode:
      returnNeeded = true;
      if (holding && (currentMode == mode)) return;           // Servo stays at this side
      if (currentMode != mode) {                               // We just entered this mode
        currentMode = mode;
        holding = false;
        if (!atTreshold(mode)) {                               // Servo is at the other side
          move();                                              // One movement to the right side
          return;
        }
      }
      holding = true;                                          // Servo stays at this side
    break;
    case SpeedMode:
      returnNeeded = true;
      holding = false;
      currentMode = mode;
      move();                                                  // Continuously back and forth
    break;
    default:                                                   // NoMode: return to the start side
      holding = false;
      currentMode = NoMode;
      if (returnNeeded) {
        returnNeeded = false;
        // Via a normal movement, the pulse signal and power end the same way as after a normal move
        if (servo[selectedServo].previousCurve != startCurve) numberOfMoves = 1;
        else numberOfMoves = 2;
        return;
      }
      if (endConfig) {
        servo[selectedServo].configPowerSignal();              // Restore the idle power values from the CVs
        selectedServo = NO_SERVO;
        configLed.turn_off();
      }
    break;
  }
}


bool ConfigurePom::active() {
  return ((selectedServo != NO_SERVO) || (requestedServo != 0));
}


// *****************************************************************************************************
// atTreshold() checks if the servo stands at the side where it ends at Min (Treshold 1) or Max
// (Treshold 2). Which side that is, depends on the curves and InvertServoDir. After a movement,
// loadCurve() recalculates the position where the current curve ends, using the direction of that
// movement. If both Tresholds are equal, the servo is at both sides.
// *****************************************************************************************************
bool ConfigurePom::atTreshold(uint8_t mode) {
  servo[selectedServo].loadCurve(servo[selectedServo].previousCurve);
  int16_t lastPosition = servo[selectedServo].getLastCurvePosition();
  int16_t delta1 = abs(lastPosition - (int16_t)servo[selectedServo].getTreshold1());
  int16_t delta2 = abs(lastPosition - (int16_t)servo[selectedServo].getTreshold2());
  if (mode == MinMode) return (delta1 <= delta2);
  else return (delta2 <= delta1);
}


void ConfigurePom::move() {
  // loadCurve() ensures the current speed (timeMultiplier) is used, also for symmetric curves
  servo[selectedServo].loadCurve(servo[selectedServo].previousCurve);
  servo[selectedServo].moveServo();
}
