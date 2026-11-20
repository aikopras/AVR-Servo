// *****************************************************************************************************
//
// File:      configure.h
// Author:    Aiko Pras
// History:   2025/05/23
//            2025/06/01 ap: first production version 
//            2025/12/08 ap V1.1: Servos 5..8 can also be confugured
//            2026/11/17 ap V1.2: moveServo() moved to MyServo, loco address corrected
//            2026/11/18 ap V1.3: same end behaviour as configuration via PoM (see below), F7 / F8 corrected
// 
// Purpose:   Via a normal handheld, configuration of the servo speed and Tresholds
//
// Configuration of servo speed and Tresholds can be performed via a normal DCC handheld.
// For that purpose, the decoder listens to the loco address 7000 + decoder address.
// Example: if the switch address on a Lenz LH100 is 397..400, the decoder address will be 99
// (400 / 4 - 1) and the loco address 7099. This loco address is also used for configuration via PoM.
//
// To enter configuration mode, push F9 multiple times on and off, within a certain time interval.
// The number of times is determined by the ATTEMPTS_REQUIRED define. The interval in which these 
// pushes are needed, is given by the ATTEMPTS_INTERVAL define. Time is in milliseconds.
//
// - F9/F19: enter / leave configuration mode
// - F1 .. F4: select servo 1 ..4
// - F11 .. F14: select servo 5 ..8
// - F5/F15: Set servo in middle position, as well as both tresholds (1500 us). 
// - F6/F16: Set sevo speed (default = 6)
// - F7/F17: Set the end position for the straight track (+, position 1 according to RCN-213)
// - F8/F18: Set the end position for the diverging track (-, position 0 according to RCN-213)
//   Whether this end position is Treshold1 (Min) or Treshold2 (Max), depends on the curves and
//   InvertServoDir. The software determines this from the side where the servo stands.
// - F0: store current setting in EEPROM (while in F6, F7 or F8)
//
// During configuration the servo positions are not saved (circular buffer), and the relais and RS-Bus
// feedback do not change. Once the servo is deselected (F1..F4 / F11..F14 off), the Tresholds and Speed
// are restored from the CVs (as after a reset; values that were not stored via F0 are lost) and the
// servo returns, via a normal movement, to the position it had when it was selected. If it is already
// at that side, it first moves away and back. This is the same as with configuration via PoM.
//
// *****************************************************************************************************
#pragma once
#include <Arduino.h>                        // For general definitions

#define F9_ATTEMPTS_REQUIRED    2           // # F9 ON/OFF pushes before configuration mode is entered
#define F9_ATTEMPTS_INTERVAL 3000           // Number of milliseconds for all F9 pushes

#define NUMBER_OF_SERVO_MOVES   2           // Number of times a servo should move for identification
#define CONFIG_STEP_TIME      200           // Time between set treshold movements (im ms)


class Configure {
  
  public:
    // checkStart: is F9 or F19 pushed ON/OFF the number of ATTEMPTS_REQUIRED?
    bool checkStart(uint8_t F9F12, uint8_t F13F20);
    bool checkConfig();                     // Should be called from the main loop as often as possible

    void setF0F4(uint8_t F0F4);             // Saves the value of F0 .. F4 
    void setF5F8(uint8_t F5F8);             // Saves the value of F5 .. F8 
    void setF9F12(uint8_t F9F12);           // Saves the value of F9 .. F12 
    void setF13F20(uint8_t F13F20);         // Saves the value of F13 .. F20 
    void setSpeed(uint8_t sp, bool dir);    // Handles the loco speed command


  private:
    typedef enum {                          // Which configuration types are possible?
      Waiting,                              // Configuration mode has not started yet
      Ready,                                // Configuration mode started
      Verify,                               // To verify if we elected the right servo
      Set,                                  // We can set now
      Restore,                              // Servo deselected: return to the start position
      Middle,                               // Servo middle Treshold (1500 us)
      TresholdStraight,                     // For the straight position (green / + / 1)
      TresholdDiverging,                    // For the diverging position (red / - /0)
      Speed                                 // Peeds of the servo movement
    } State_t;
    State_t configState = Waiting;

    uint8_t F0F4 = 0;                       // Local copy from the locoCmd object
    uint8_t F5F8 = 0;                       // Local copy
    uint8_t F9F12 = 0;                      // Local copy
    uint8_t F13F20 = 0;                     // Local copy
    uint8_t locoSpeed = 0;                  // Local copy
    bool locoDirection = 0;                 // Local copy - True = Forward / False = Reverse

    uint8_t selectedServo = 255;            // which servo we are operating on??    

    // routines for each state
    bool doReady();
    void doVerify();
    void doSet();
    void doRestore();
    void doMiddle();
    void doTresholdStraight();
    void doTresholdDiverging();
    void doTreshold(bool keyPressed);       // Common part of doTresholdStraight() and doTresholdDiverging()
    bool atTreshold1();                     // Does the servo stand at Treshold1 (Min)?
    void doSpeed();

    uint8_t validServoFromF();              // One and only 1 servo function is selected
    bool servoDeselected();                 // the previous selected servo is no longer selected

    // For verify if we selected the right servo (see also MyServo::moveServo())
    uint8_t numberOfMoves = NUMBER_OF_SERVO_MOVES;

    // To return to the start position after deselection (see doRestore())
    uint8_t startCurve;                     // previousCurve of the servo at the moment it was selected
    bool returnNeeded = false;              // F5..F8 were used: servo should return to its start position

    // For entering configuration mode
    uint8_t F9_AttemptsLeft = F9_ATTEMPTS_REQUIRED;
    long F9_TimeFirstPush = 0;
    long lastConfigTime;
    
    bool adjustTreshold1;                   // F7 / F8: the servo stands at Treshold1 (Min), otherwise at Treshold2
};
