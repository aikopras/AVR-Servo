//*****************************************************************************************************
//
// File:      myRSBus.h
// Author:    Aiko Pras
// History:   2025/05/05
//            2025/06/01 AP V1.0: first production version 
//            2025/12/02 AP V2.0: complete rewrite, to allow the use of multiple RS-Bus addresses  
// 
// Purpose:   Declaration of RS-Bus feedback functions
//
// In V2.0 the code is completely rewritten, such that multiple RSBus addresses can be used.
// This allow us to use to send the position back of more than two servos.
//
// The interface between the main program and this class is an array with feedback bits;  
// the array must have the same size as the number of servos. 
// Example code for servo i:
//     servo[i].set(accCmd.position);
//     rsbus.set(i, accCmd.position);
//
// We also need multiple RSbus objects. The number of such objects depends on the number of servos
// that share the same RSbus nibble. If SkipUnEven is set (which will usually be the case), each servo
// uses its own nibble and we need therefore one RSbus object for two servos.
// 
// 
//*****************************************************************************************************
#pragma once
#include <Arduino.h>                         // For general definitions
#include <RSbus.h>                           // Inherits and extends the RSBus class
#include "hardware.h"                        // defines MAX_NUMBER_OF_SERVOS


// How many RSbus addresses/objects do we need worst case? 
// A single object can be shared between 2 (skipUnEven = true) or 4 (skipUnEven = false) servos/switches
#define MAX_NUMBER_OF_RSBUS_ADDRESSES  ((MAX_NUMBER_OF_SERVOS + 1) / 2)


// MyRSbusConnection extends the RSbusConnection with a byte holding the feedback data.
// Having such feedback data is required since we may need to resend the two other bits in the nibble.
// In addition, the complete 8-bit feedback data is used whenever the RSbus connection needs to re-establish
class MyRSbusConnection: public RSbusConnection {
  public:
    uint8_t feedback8Bit;                    // The low and high nibble together 
    uint8_t lastSent8Bit;                    // The previous 8-bits. Needed to check for changes 
};


class Feedbacks {    
  public:
    MyRSbusConnection rsbus[MAX_NUMBER_OF_RSBUS_ADDRESSES];
    
    // init has 3 parameters:
    // - address: the RSbus address of the (first) rsbus object
    // - skip: the value of the skipUnEven CV
    // - number: the number of servos/switches for which RSbus feedback messages will be send.
    void init(uint8_t address, bool skip, uint8_t number = MAX_NUMBER_OF_SERVOS);
    void initData(uint8_t servoIndex, bool value);                // servoIndex is the servo/switch number. NOT the index of the feedback bit     
    void send(uint8_t servoIndex, bool value);                    // value: false = diverging (red, -), true = straight (green, +)
    void checkFeedbacks();                                        // Should be called from main as frequent as possible

  private:
    boolean separateNibbles;                                      // a nible is used for only one servo/switch 
    uint8_t rsbusAddressesInUse;                                  // the actual number of RSbus connections that are being used

    void sendIfChanged(uint8_t index, uint8_t nibble, Nibble_t which);

    uint8_t setLowNibble(uint8_t RSbusAddressIndex, bool position);
    uint8_t setHighNibble(uint8_t RSbusAddressIndex, bool position);

    uint8_t setFB01(uint8_t RSbusAddressIndex, bool position);
    uint8_t setFB23(uint8_t RSbusAddressIndex, bool position);
    uint8_t setFB45(uint8_t RSbusAddressIndex, bool position);
    uint8_t setFB67(uint8_t RSbusAddressIndex, bool position);
};
