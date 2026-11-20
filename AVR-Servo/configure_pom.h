// *****************************************************************************************************
//
// File:      configure_pom.h
// Author:    Aiko Pras
// History:   2026/11/17 ap: first version
//            2026/11/20 ap: extra LED (LED_CONFIG) on during configuration
//
// Purpose:   Configuration of the servo end positions (Tresholds) and speed via PoM
//
// Similar to the configuration via a handheld (see configure.h), the servo end positions and speed
// can be configured by a PC program, such as Programmer-Decoder-POM. For that purpose two command
// CVs are used. These CVs only exist in RAM: they are not stored in EEPROM, but can be read via PoM.
// After a restart both are 0.
// - CV63: which servo should be configured: 1..MAX_NUMBER_OF_SERVOS. 0 = ready
// - CV62: what should be configured: 0 = nothing, 1 = Min (Treshold 1), 2 = Max (Treshold 2), 3 = Speed
//
// - CV63 = n: servo n moves once away and back, to verify the right servo is selected.
// - CV62 = 1 (or 2): the servo moves to the side where it ends at Min (or Max), and stays there.
//   If the PC program subsequently writes MinLow and MinHigh (or MaxLow and MaxHigh) for this servo,
//   the servo jumps to the new position once the high byte has been written.
// - CV62 = 3: the servo moves continuously back and forth. A new value for the Speed CV will be
//   used for the next movement.
// - CV62 = 0 or CV63 = 0: the servo returns, via a normal movement, to the side where it was when it
//   was selected. If it is already at that side, it first moves away and back. Thereafter the idle
//   power settings are restored. The stored servo position (circular buffer) is not changed.
// - Selecting another servo first ends the configuration of the current servo.
//
// The new values for Min, Max and Speed are stored in EEPROM by the normal PoM write (there is no
// separate SAVE). While a servo is being configured, accessory (switch) commands are ignored, and the
// extra LED (LED_CONFIG, the same LED as for configuration via the handheld) is on.
// Configuration via SM is not supported: in SM, writes to CV62 and CV63 are ignored.
// For PoM as well as SM, writes to CV64 (EEPROM_MAX_SERVOS_INDEX) are ignored.
//
// *****************************************************************************************************
#pragma once
#include <Arduino.h>                        // For general definitions
#include <AP_DCC_Decoder_Core.h>            // For Dcc::CmdType_t

#define CV_POM_CONFIG_MODE     62           // What should be configured (see above)
#define CV_POM_CONFIG_SERVO    63           // Which servo should be configured (see above)


class ConfigurePom {
  public:
    void processMessage(Dcc::CmdType_t cmdType); // To be called instead of cvProgramming.processMessage()
    void checkConfig();                     // Should be called from the main loop as often as possible
    bool active();                          // True while a servo is being configured

  private:
    typedef enum {                          // What should be configured? (values of CV62)
      NoMode,                               // 0: nothing
      MinMode,                              // 1: Min (Treshold 1)
      MaxMode,                              // 2: Max (Treshold 2)
      SpeedMode                             // 3: Speed
    } Mode_t;
    uint8_t requestedMode = NoMode;         // CV62 (RAM only)
    uint8_t requestedServo = 0;             // CV63 (RAM only): 0 = ready, 1..MAX_NUMBER_OF_SERVOS
    uint8_t selectedServo = 255;            // 0..MAX_NUMBER_OF_SERVOS-1. 255 = no servo selected
    uint8_t currentMode = NoMode;           // The mode the selected servo is in
    uint8_t startCurve;                     // previousCurve of the servo at the moment it was selected
    uint8_t numberOfMoves = 0;              // Moves still to be made (verification / return)
    bool holding = false;                   // Servo stands at the Min or Max side, and stays there
    bool returnNeeded = false;              // Servo should return to its start side via a normal movement

    bool writeCommand();                    // Is the PoM or SM message a write command?
    void newServoValue();                   // Called after a PoM write, to use new values immediately
    bool atTreshold(uint8_t mode);          // Is the servo at the Min (MinMode) or Max (MaxMode) side?
    void move();                            // Move the selected servo to the other side
};
