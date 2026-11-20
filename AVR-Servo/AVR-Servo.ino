// *****************************************************************************************************
//
// File:      AVR-Servo.ino
// Author:    Aiko Pras
// History:   2022/07/12 AP V2.0
//            2025/02/16 AP V2.1
//            2025/06/01 AP V2.2 - First production version
//            2025/10/01 AP V2.3 - RSbus type = Switch.
//            2025/10/10 AP V3.0 - copied from AVR-Servo-2.1 and modified for 3 servo's
//            2025/12/03 AP V3.1 - RSBus code rewritten, to cope with multiple RS-Bus addresses
//            2025/12/03 AP V3.2 - Configuration code rewritten, to support servos 4..8
//            2025/12/12 AP V3.3 - Tested. Release version
//            2026/11/17 AP V3.4 - Renamed into AVR-Servo-3-6. Servo-TCA 2.0.0, servo-6 board,
//                                 CV64 and DecType are board properties, numberOfSwitches (DCC and RS-Bus
//                                 addresses) from SwitchCount or set by DKW / Hosentrager,
//                                 Hosentrager ignores index > 3 and is not possible on the servo-3 board,
//                                 initial RS-Bus feedback uses getPosition(),
//                                 configuration via PoM (CV62 / CV63, see configure_pom.h)
//            2026/11/18 AP V3.5 - Renamed into AVR-Servo. servo-2 board added (polarisation relays via
//                                 setRelay(), position buttons, ignored during configuration),
//                                 DKW requires 3 servos, handheld configuration ends like PoM,
//                                 debug output off, on-board LED flashes for PoM commands
//
// Purpose:   DCC Servo decoder for the AVR Servo boards with two, three or six servo connections
//
// Processor:   AVR64DA28 (servo-2 board) or AVR32DA48 (servo-3 and servo-6 board)
// Programmer:  Serial UPDI  (see hardware.h for compile settings)
// Decoder PCB: servo-2 board V2.0 (2022/07), servo-3 board V2.2 (2025/07) or servo-6 board V2.3 (2025/12).
//              Select the board in hardware.h
//
// According to RCN-213, the switch positions are:
//  accCmd.position = 0: diverging track / red / -
//  accCmd.position = 1: straight track / green / +
//
//
// ==========================================
// Operation modes for the decoder (variant):
// - normal:    independent servos, one per switch. The number of switches is set via the SwitchCount CV
//              (maximum: MAX_NUMBER_OF_SERVOS)
// - dkw:       two switches: two nomal servos and a third servo for the Spurkranzlenkung
//              (not possible on the servo-2 board)
// - Crossover: four switches with four coupled servos, that avoid "illegal" settings
//              (only possible on the servo-6 board)
// A switch has its own DCC (turnout) address and its own RS-Bus feedback.
//
// ******************************************************************************************************
#include <Arduino.h>              // For general definitions
#include <AP_DCC_Decoder_Core.h>  // To include all objects, such as dcc, accCmd, etc.
#include "hardware.h"             // The pin and EEPROM specific details
#include "servo_CVs.h"            // Servo specific CVs
#include "servo_position.h"       // Storage for the servo positions in EEPROM
#include "myServo.h"              // Inherits and extends the ServoMoba6 class
#include "myRSBus.h"              // Perfroms all RS-Bus feedback functions
#include "configure.h"            // Allows configuration via the hand held
#include "configure_pom.h"        // Allows configuration via PoM (CV62 and CV63)
#include "dcc_led.h"              // Controls the DCC Led

#define SOFTWARE_VERSION 24        // DECODER_TYPE is defined in hardware.h

MyServo servo[MAX_NUMBER_OF_SERVOS];  // MyServo is a new class, which inherits ServoMoba6 (and Servo6)
Feedbacks rsbus;                  // Feedbacks inherits RSbusConnection, and is used for feedback bits
BasicLed configLed;               // Instantiate the extra LED to show configuration mode

#ifdef POSITION0_PIN               // Only the servo-2 board has buttons to change the servo position
ToggleButton buttonPos0;
ToggleButton buttonPos1;
#endif

Configure handheldConfig;         // object that takes care of configuration via the hand held
bool configMode = false;          // Flag that tells if we are (not) in hand held configuration mode
ConfigurePom pomConfig;           // object that takes care of configuration via PoM

unsigned int firstDecoderAddress; // Derived from CV1 + CV9 (1..511)
unsigned int lastDecoderAddress;  // Depends on the number of switches
uint8_t numberOfSwitches;         // Number of DCC (turnout) addresses and RS-Bus feedbacks. See setup() step 6
bool onlyEvenAddresses;           // If true, decoder uses even adresses only

// The decoder can operate in several modes (variants). See explanation above
uint8_t variant;
#define DKW 1                     // To improve code readability. Value for variant
#define HOSENTRAGER 2             // To improve code readability. Value for variant


//******************************************************************************************************
void setup() {
  // Step 1: Serial monitor. Debug output is off by default.
  delay(100);
  //Monitor.begin(115200);
  //delay(200);
  // Step 2: If the EEPROM has not yet been initialised, initialise the SERVO SPECIFIC values.
  // This involves all EEPROM values from index 65 and higher, and includes three parts:
  // 1) servo specific CVs (65..128/192),
  // 2) the (2 or 4) curves that are stored in EEPROM
  // 3) the circular buffer, which holds the last positions
  if (cvValues.notInitialised()) CreateDefaultServoValuesInEEPROM();
  // CreateDefaultServoValuesInEEPROM();     // May be used to temporarily reinitialise the EEPROM
  //
  // Step 3: Set the default CV values (1..64; see AP_CV_values.h. for details)
  // Decoder type (DecType) and software version (version) are set using cvValues.init().
  // After cvValues.init() is called, CV default values may be modified using cvValues.defaults[...]
  // Updated values take effect after setDefaults() is called, either by a long push of the
  // onboard programming button, or by a CV-POM or CV-SM message to CV8 (VID).
  cvValues.init(DECODER_TYPE, SOFTWARE_VERSION);
  // DecType is a board property (hardware.h). Restore it, in case it was changed via PoM or SM.
  // Since write() uses EEPROM.update(), the EEPROM is only written if the value differs.
  cvValues.write(DecType, DECODER_TYPE);
  //
  // Step 4: Read the SkipUnEven and the SwitchType CVs
  // Skipping uneven (or even) addresses avoids potential problems with RS-Bus switch feedback 
  // See: https://github.com/aikopras/RSbus/blob/master/extras/switch-feedback-problems.md
  // SwitchType tells if the decoder is a "normal" decoder, or special for Weinert's Spurkranzlenkung
  // or a double cross (Hosentrager)
  // DKW requires 3 servos and Hosentrager requires 4 servos. On a board with less servos (servo-2 and
  // servo-3 board), the decoder operates as a normal decoder.
  onlyEvenAddresses = cvValues.read(SkipUnEven);
  variant = cvValues.read(SwitchType);
  if ((variant == DKW) && (MAX_NUMBER_OF_SERVOS < 3)) variant = 0;          // 0 = normal
  if ((variant == HOSENTRAGER) && (MAX_NUMBER_OF_SERVOS < 4)) variant = 0;  // 0 = normal
  //
  // Step 5: initialise the dcc and rsbus objects.
  // This involves attaching the following pins: DCC input, the RS-Bus in- and output,
  // the onboard LED and the onboard button. In case the EEPROM wasn't initialised yet, 
  // we initialise the CV values (1..64). Finally we set the first Accessory Address from the CVs.
  decoderHardware.init();
  //
  // Step 6: depending on the number of switches, we may need to listen to 
  // more than one decoder address. Therefore we set a range of decoder addresses
  // this accessory decoder will listen to.  
  // A switch has its own DCC (turnout) address and its own RS-Bus feedback. The number of switches
  // may differ from the number of servos: the DKW variant has 2 switches and 3 servos.
  // For DKW and Hosentrager, the variant determines the number of switches. For the normal variant,
  // the SwitchCount CV holds the number of switches. It can be set via PoM or SM, to claim fewer
  // decoder and RS-bus addresses than the board maximum (MAX_NUMBER_OF_SERVOS).
  // According to the AP_DCC_Decoder_Core library, the values 0 and 255 mean: use the board maximum.
  // 0 is also the default value. A value above the board maximum is invalid, and is treated the same.
  firstDecoderAddress = cvValues.storedAddress();
  switch (variant) {
    case DKW:         numberOfSwitches = 2; break;
    case HOSENTRAGER: numberOfSwitches = 4; break;
    default:          numberOfSwitches = cvValues.read(SwitchCount); break;
  }
  if (numberOfSwitches == 0 || numberOfSwitches == 255 || numberOfSwitches > MAX_NUMBER_OF_SERVOS)
    numberOfSwitches = MAX_NUMBER_OF_SERVOS;
  // Determine the last address this decoder listens to
  if (onlyEvenAddresses) lastDecoderAddress = firstDecoderAddress + ((numberOfSwitches - 1) / 2);
    else lastDecoderAddress = firstDecoderAddress + ((numberOfSwitches - 1) / 4);
  accCmd.setMyAddress(firstDecoderAddress, lastDecoderAddress);
  //
  // Step 7: Attach the extra LED, which we will use to signal we are in configuration mode
  configLed.attach(LED_CONFIG);
  //
  // Step 8: Initialse the servos
  // The main part of this sketch is responsible for the servo initialisation and movement
  // See myServo for details
  // During initialisation the servo power is switched on, one servo after the other
  // (see PowerStartUpDelay in servo_CVs.h). If several decoders share the same 5V power supply
  // and start at the same moment, their servos should not be switched on at the same moment.
  // Therefore we first wait 0..80 ms, depending on the decoder address (see OpenDCC, method 6).
  // With consecutive decoder addresses, up to 5 decoders get a different start moment.
  delay((firstDecoderAddress % 5) * 20);
  for (uint8_t i = 0; i < MAX_NUMBER_OF_SERVOS; i++) servo[i].init(i);
  //
  // Step 9: Initialise the object for RS-Bus feedback messages. 
  // The first parameter is the (first) RSbus address, which is taken from the myRSAddr CV.
  // The second parameter is the boolean onlyEvenAddresses (=SkipUnEven), which tells if each switch
  // will use an own feedback nibble, or if two switches share the same RS-Bus nibble. 
  // The third parameter is optional, and tells for how many switches we send RS-Bus feedbacks.
  // This parameter is useful to limit the required number of RSbus addresses, in case not all  
  // servos are connected, or in cases where a servo is used for some support tasks
  // that does not require a RSbus feedback, such as the third servo of the DKW variant
  // (Weinert's DKW Spurkranzlenkung). See step 6 for numberOfSwitches.
  rsbus.init(cvValues.read(myRSAddr), onlyEvenAddresses, numberOfSwitches);
  //
  // Step 10: Set the startup values for the RSbus feedback data.
  // Use, for each switch, the current position of its servo (0 = diverging / 1 = straight).
  // Note that bit 7 of previousCurve (DIRECTION) can not be used, since it differs from the position
  // if the servo direction is inverted or if the curves are not symmetric.
  for (uint8_t i = 0; i < numberOfSwitches; i++) {
    rsbus.initData(i, servo[i].getPosition());
  }
  //
  // Step 11: Connect the two buttons that can be used to change the servo's position (servo-2 board)
  #ifdef POSITION0_PIN
    buttonPos0.attach(POSITION0_PIN, DEBOUNCE_TIME);
    buttonPos1.attach(POSITION1_PIN, DEBOUNCE_TIME);
  #endif
  //printinfo();
  //printTwoCurves();
  //printCircularBuffer();
}


//******************************************************************************************************
// 
//******************************************************************************************************
void loop() {
  unsigned int index;                                    // index which servo should be used
  // Step1: Check if we are in normal mode or configuration mode
  if (!configMode) {    // We are in normal mode for servo operation
    if (dcc.input()) {  // Any DCC command received?
      
      switch (dcc.cmdType) {
        case Dcc::MyAccessoryCmd:
          onBoardLed.activity();                         // Flash the LED once 
          //printAccessoryDetails();                     // for debugging
          // The deactivate should be ignored. During configuration via PoM all accessory commands are ignored.
          // (During configuration via the handheld, accessory commands do not reach this point; see configMode)
          if (accCmd.activate && !pomConfig.active()) {
            // To what servo is the Accessory Command directed
            index = (accCmd.decoderAddress - firstDecoderAddress) * 4 + accCmd.turnout - 1;            
            if (onlyEvenAddresses) index = index / 2;    // 2 accCmd.turnout addresses share the same servo
            switch (variant) {
              case DKW: handleDkw(index); 
              break;
              case HOSENTRAGER: handleHosentrager(index);
              break;
              default:                                   // This is the normal case. Set the requested servo
                if (index < numberOfSwitches) {
                  servo[index].set(accCmd.position);
                  servo[index].setRelay();               // Polarisation relay (servo-2 board only)
                  rsbus.send(index, accCmd.position);
                }
              break;
            }
          }
        break;

        case Dcc::MyPomCmd:
          // Flash the LED once, before processMessage(): a write of CV23 (Search) may then start
          // the search flashing. Note: each next PoM command (or accessory command) stops that flashing.
          onBoardLed.activity();
          pomConfig.processMessage(Dcc::MyPomCmd);       // Calls cvProgramming.processMessage()
        break;

        case Dcc::SmCmd:
          pomConfig.processMessage(Dcc::SmCmd);          // Calls cvProgramming.processMessage()
        break;

        case Dcc::MyLocoF9F12Cmd:
        case Dcc::MyLocoF13F20Cmd:
          // Check if F9 or F19 is pushed and we need to switch to configuration mode
          // Not during configuration via PoM, to avoid both configurations controlling a servo
          if (!pomConfig.active()) configMode = handheldConfig.checkStart(locoCmd.F9F12, locoCmd.F13F20);
        break;

        default:
          // nothing
        break;
      };                           // end switch
    dccLed.On();                   // We have a DCC signal
    };                             // end of DCC input
  }                                // end of normal mode
  else {                           // This is the mode to configure servo speed and position
    if (dcc.input()) {             // Any DCC command received?
      switch (dcc.cmdType) {
        case Dcc::MyLocoSpeedCmd:  handheldConfig.setSpeed(locoCmd.speed, locoCmd.forward); break;
        case Dcc::MyLocoF0F4Cmd:   handheldConfig.setF0F4(locoCmd.F0F4);    break;
        case Dcc::MyLocoF5F8Cmd:   handheldConfig.setF5F8(locoCmd.F5F8);    break;
        case Dcc::MyLocoF9F12Cmd:  handheldConfig.setF9F12(locoCmd.F9F12);  break;
        case Dcc::MyLocoF13F20Cmd: handheldConfig.setF13F20(locoCmd.F13F20); break;
        default: break;  // Nothing
      };
    dccLed.On();                   // We have a DCC signal
    };    // end of DCC input
    configMode = handheldConfig.checkConfig();  // Should be called as frequent as possible
  };      // end of config mode
  dccLed.check();                // Check if the DCC signal disappeared (based on a timer)
  //
  // Step 2: as frequent as possible update the RS-Bus hardware, check if the programming
  // button is pushed, and if the status of the onboard LED should be changed.
  decoderHardware.update();
  //
  // Step 3: as frequent as possible check if RSBus feedback messages should be send.
  rsbus.checkFeedbacks();
  //
  // Step 4: as frequent as possible check if one or more servos require updates
  for (uint8_t i = 0; i < MAX_NUMBER_OF_SERVOS; i++) servo[i].checkServo();
  //
  // Step 5: as frequent as possible check if a servo is being configured via PoM
  pomConfig.checkConfig();
  //
  // Step 6: Check the buttons if switch positions should be changed (servo-2 board only)
  // The buttons are read continuously, but ignored during configuration (handheld or PoM)
  #ifdef POSITION0_PIN
    buttonPos0.read();
    buttonPos1.read();
    if (!configMode && !pomConfig.active()) {
      if (buttonPos0.changed()) setByButton(0);
      if (buttonPos1.changed()) setByButton(1);
    }
  #endif
};



//******************************************************************************************************
// Support routine for the position buttons (servo-2 board only)
//******************************************************************************************************
void setByButton(uint8_t index) {
  // A button changes the position of its servo, like an accessory command with the other position
  if (index >= numberOfSwitches) return;      // Servo not in use (SwitchCount)
  bool newPos = !servo[index].getPosition();
  servo[index].set(newPos);
  servo[index].setRelay();
  rsbus.send(index, newPos);
};


//******************************************************************************************************
// Support routines for special variants
//******************************************************************************************************
void handleDkw(uint8_t index) {
  // In case of Weinert's DKW (74942), a third servo is needed for the "Spurkranzlenkung".
  // This third servo should be coupled with one of the two other servos. Which of these
  // is not important, although the position of the third servo may need to be changed
  // for correct operation of the Spurkranzlenkung. 
  // In this variant the decoder will listen to two DCC servo addresses only (= one Decoder address), 
  // and use a single RSbus feedback address only.
  if (MAX_NUMBER_OF_SERVOS < 3) return;       // Requires a board with 3 or more servos (see setup() step 4)
  switch (index) {
    case 0: 
      servo[0].set(accCmd.position);      // Set servo 0 to the required position
      rsbus.send(0, accCmd.position);     // Send a RSbus feedback message for servo 0
      servo[2].set(accCmd.position);      // And set servo 2, which is used for the Spurkranzlenking
    break;
    case 1: 
      servo[1].set(accCmd.position);
      rsbus.send(1, accCmd.position); 
    break;
    default:                              // This decoder variant listens to two servo addresses only
    break;
  }
};


void handleHosentrager(uint8_t index) {
  //
  //    A1   A2           index = 0: A1
  //  ----------                  1: B1
  //      \ /                     2: A2
  //      / \                     3: B2
  //  ----------                 >3: ignore index
  //    B1   B2
  //
  // A double-cross (Hosentrager) allows only the following switch positions / routes:
  // R0: all switches are in the straight position
  // R1: The opposing switches A1 and B2 are diverging, and the switches B1 and A2 are straight
  // R2: The opposing switches B1 and A2 are diverging, and the switches A1 and B2 are straight
  // All other routes are invalid. 
  // A state in which all 4 switches are diverging, is invalid, since such state prohibits 
  // correct frog polarisation (since polarisation is coupled to the switches A1 and A2).
  //
  //    A1   A2
  //  ----------        R0 ----------        R1 ---    ---           ---    ---     
  //      \ /                                       \                     /
  //      / \                                        \                   /
  //  ----------           ----------           ---    ---        R2 ---    ---
  //    B1   B2
  //
  // The strategy taken by this code is to switch ALL servo's as soon as a route change is requested.
  // In principle such strategy bears the risk that too many servos get activated and too many RSbus
  // feedback messages get transmitted. However, the code that sets a servo / sends a RSbus feedback
  // message returns immediately whenever the requested new state matches the existing state.
  // Therefore the strategy will work in practice.
  //
  // To improve code readability, we made the following defines:
  #define DIVERGING 0                        //  accCmd.position = 0: diverging track (curve0)
  #define STRAIGHT 1                         //  accCmd.position = 1: straight track (curve1)
  #define POS_A1 servo[0].getPosition()      //  0 = diverging / 1 = straight
  #define POS_B1 servo[1].getPosition()
  #define POS_A2 servo[2].getPosition()
  #define POS_B2 servo[3].getPosition()

  if (MAX_NUMBER_OF_SERVOS < 4) return;       // Requires a board with 4 or more servos (see setup() step 4)
  if (index > 3) return;                      // Only the indexes for A1, B1, A2 and B2 are valid
  
  if (accCmd.position == STRAIGHT) {
    bool inRoute1 = (POS_A1 == DIVERGING && POS_B2 == DIVERGING && POS_B1 == STRAIGHT && POS_A2 == STRAIGHT);
    bool inRoute2 = (POS_A2 == DIVERGING && POS_B1 == DIVERGING && POS_A1 == STRAIGHT && POS_B2 == STRAIGHT);
    
    // Route R1 was already set, but now A2/B1 are set to straight
    if (inRoute1 && ((index == 1) || (index == 2))) return;

    // Route R2 was already set, but now A1/B2 are set to straight
    if (inRoute2 && ((index == 0) || (index == 3))) return;
    
    // This is route R0: all straight
    servo[0].set(STRAIGHT);
    servo[1].set(STRAIGHT);
    servo[2].set(STRAIGHT);
    servo[3].set(STRAIGHT);
    rsbus.send(0, STRAIGHT);
    rsbus.send(1, STRAIGHT);
    rsbus.send(2, STRAIGHT);
    rsbus.send(3, STRAIGHT);
  }

  else if ((accCmd.position == DIVERGING) && ((index == 0) || (index == 3))) {
    // This is route R1
    servo[0].set(DIVERGING);
    servo[1].set(STRAIGHT);
    servo[2].set(STRAIGHT);
    servo[3].set(DIVERGING);
    rsbus.send(0, DIVERGING);
    rsbus.send(1, STRAIGHT);
    rsbus.send(2, STRAIGHT);
    rsbus.send(3, DIVERGING);
  }

  else if ((accCmd.position == DIVERGING) && ((index == 1) || (index == 2))) {
    // This is route R2
    servo[0].set(STRAIGHT);
    servo[1].set(DIVERGING);
    servo[2].set(DIVERGING);
    servo[3].set(STRAIGHT);
    rsbus.send(0, STRAIGHT);
    rsbus.send(1, DIVERGING);
    rsbus.send(2, DIVERGING);
    rsbus.send(3, STRAIGHT);
  };
};


//******************************************************************************************************
// Some temporary print routines for debugging
//******************************************************************************************************
void printinfo() {
  Monitor.println("");
  Monitor.println();
  Monitor.println("AVR-Servo Decoder");
  Monitor.print("Maximum number of servos: "); Monitor.println(MAX_NUMBER_OF_SERVOS);
  Monitor.print("Software version: "); Monitor.println(SOFTWARE_VERSION);
  if (cvValues.addressNotSet()) {
    Monitor.println("Address not set");
    return;
  }
  Monitor.print("First Decoder Address: "); Monitor.println(firstDecoderAddress);
  Monitor.print("First RS-Bus Feedback Address: "); Monitor.println(cvValues.read(myRSAddr));
  Monitor.print("Loco adres: "); Monitor.println(cvValues.storedAddress() + 7000);
  Monitor.println("");
}

void printCVs() {
  for (uint8_t i = 0; i <= 64; i++) {
    Monitor.print("CV");
    Monitor.print(i);
    Monitor.print(": ");
    Monitor.println(cvValues.read(i));
  }
  if (cvValues.addressNotSet()) Monitor.println("Address not set");
  Monitor.print("First Decoder Address: ");
  Monitor.println(firstDecoderAddress);
  Monitor.print("First RS-Bus Feedback Address: ");
  Monitor.println(cvValues.read(myRSAddr));
}

void printAccessoryDetails() {
  Monitor.println("");
  Monitor.print("Switch address: ");
  Monitor.print(accCmd.outputAddress);
  Monitor.print(", Position: ");
  if (accCmd.position == 1) Monitor.println("+ (1)");
  else Monitor.println("- (0)");
};

void printCircularBuffer() {
  // Prints the first 20 entries only 
  for (uint16_t i = EEPROM_BOOTS_INDEX; i <= (EEPROM_BOOTS_INDEX + 20); i++) {
    Monitor.print("CV");
    Monitor.print(i);
    Monitor.print(": ");
    Monitor.println(cvValues.read(i));
  }
}

void printTwoCurves() {
  Monitor.println();
  Monitor.print("previous curve 1: ");
  Monitor.println(servo[0].previousCurve);
  Monitor.print("previous curve 2: ");
  Monitor.println(servo[1].previousCurve);
  Monitor.println();
}
