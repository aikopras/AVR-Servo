//*****************************************************************************************************
//
// File:      myServo.h
// Author:    Aiko Pras
// History:   2025/02/22 
//            2025/06/01 ap: first production version 
//            2026/11/17 ap: adapted to Servo-TCA 2.0.0 (ServoMoba6), moveServo() added
//            2026/11/18 ap: setRelay() for the polarisation relays (servo-2 board)
// 
// Extends the ServoMoba6 class with some extra functionality that we need for this decoder
// A maximum of 6 servo objects can be instantiated
//
// CurveA and CurveB
// =================
// For each servo there are 2 CVs for the curves: one for direction A (red) and one for direction B (green).
// As part of init(), both CVs will be read from EEPROM and stored in two variables: curve0 and curve1.
// Although the CV CurveB may have the value 0, 255 or CurveA, the servo init() / copyCurveCVs() ensures
// that the variable curve1 always indexes a valid curve.
//
// PreviousCurve
// =============
// As part of init(), also the last used curve (including the MSB = direction) is read from the
// circular buffer (that is maintained at the end of the EEPROM space), and stored in: previousCurve.
// This data in previousCurve is needed to load the last used servo curve. In init(), this curve is 
// used to determine the initial servo position (in microseconds) that must be written to the servo 
// (writeMicroseconds) before we attach the servo. 
//
// New servo command
// =================
// Whenever a new DCC command is received, the (7LSB) from previousCurve and curve0 / curve1 
// will be compared. 
// If these are equal, then the curve is symmetric. In that case only the MSB needs to be changed.
// If these are unequal, then the new curve must be loaded that belongs to the desired switch position.
// Loading this new curve can be accomplished by loadCurve().
// To subsequently move the servo, moveServoAlongCurve should be called.
//
// Polarisation relays
// ===================
// Only the servo-2 board has polarisation relays (RELAYS0_PIN and RELAYS1_PIN in hardware.h). 
// set() does not switch the relay. After set(), the main program should call setRelay(), which sets
// the relay to the current position. During configuration (handheld or PoM) setRelay() is not called,
// since the servo returns to its start position after configuration. On boards without relays,
// setRelay() does nothing.
// 
// Meaning of the bits within a curve byte
// =======================================
// The bits within the CVs and attributes that hold curves, have the following meaning:
// Bit 7 (MSB): Curve direction. 
//              0 = normal direction, 1 = opposite direction (both Y coordinates are interchanged)
// Bit 6: Curve storage location
//              0 = curve is stored in PROGMEM, 1 = curve is stored in EEPROM
// Bits 5..0: index that points to the desired curve
// 
// For further details, see: myServo.cpp
//
//******************************************************************************************************
#pragma once
#include <Arduino.h>
#include <Servo_TCA_MoBa.h>                 // Inherits and extends the ServoMoba6 class


class MyServo: public ServoMoba6 {

  public:
    void init(uint8_t servoNumber);         // In theory 0..7 
    void set(uint8_t servoPosition);        // 0 = diverging (red, -),  1 = straight (green, +)
    void invertServoDirection();            // invert the servo direction by changing curvo0 and curve1
    void loadCurve(uint8_t curve);          // load a new curve from either EEPROM or PROGMEM
    void moveServo();                       // move to the other position, without saving it (configuration)
    void setRelay();                        // set the polarisation relay (if any) to the current position

    bool getPosition();                     // 0 = diverging track, red, - / 1 = straight track, green, + 

    void configPulseSignal(                 // Configure all variables ted to the pulse signal
    uint16_t initWidth);                    // using the relaited CV values 
    void configPowerSignal();               // Set the idle power values from CVs

    uint8_t timeMultiplier;                 // 1..255 (20ms steps). Slows down servo movement

    uint8_t curve0;                         // The curve we should use for switch position 0 (red)
    uint8_t curve1;                         // The curve we should use for switch position 1 (green)
  
  private:
    void attachMyServo();                   // Attaches the servo, if the corresponding PIN is defined
    void copyCurveCVs();                    // Copies the CVs for the Curves into curvo0 and curve1
    void pulseAfterReboot(                  // Aftrer reboot, set the pulse signal to a high or low level
    uint8_t level,                          // 0 = LOW (0V), 1 = HIGH (3,3 or 5V)
    uint8_t waitTime);                      // waitTime is in 20ms ticks
    void initPolarisationRelay();           // Sets the pin for the polarisation relay (if any) as output
    void printInfoIni();                    // for debugging
    void printInfoSet();                    // for debugging

    uint8_t servoNumber;                    // In principle 0..7, in practice 0..5
    bool servoDirectionInverted;            // The servo direction was changed by invertServoDirection()
    bool invertPolarisationRelay;           // invert the relais from + is OFF to + is ON (InvertRelais CV)
};
