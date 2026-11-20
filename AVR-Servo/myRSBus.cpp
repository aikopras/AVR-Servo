// *****************************************************************************************************
//
// File:      myRSBus.cpp
// Author:    Aiko Pras
// History:   2025/05/05
//            2025/06/01 AP V1.0: first production version 
//            2025/10/01 AP V1.1: Only 2 RS-Bus bits used per servo
//            2025/12/02 AP V2.0: complete rewrite, to allow the use of multiple RS-Bus addresses  
//            2026/11/17 AP V2.1: number of RS-Bus addresses corrected if two servos share a nibble
//            2026/11/18 AP V2.2: RS LED only if LED_RS is defined (not on the servo-2 board)
// 
// Purpose:   Implementation of RS-Bus feedback functions. This code may also be used in other decoders.
//
// The first set of methods set the bits in the respective nibble, and send that nibble
// via the RS-Bus connection. In addition, they update the 8-bit feedback data, which is
// needed after the RS-Bus connection was temporary lost 
//
// The init() method must be called after the servos got attached, since it needs to know  
// the positions of the servos after startup
//
// The relationship between the servo/switch index, the address of the associated RSbus connection
// and the nibble (high of low) and the bits (0 & 1 / 2 & 3) is shown below.
//
// if (SkipUnEven = true)
// servoIndex | rsAddressIndex | nibble | nibbleBits |
// ---------: | -------------- | -----  | ---------- |
//     0      |        0       |  low   |    0..1    |
//     1      |        0       |  high  |    0..1    |
//     2      |        1       |  low   |    0..1    |
//     3      |        1       |  high  |    0..1    |
//     4      |        2       |  low   |    0..1    |
//     5      |        2       |  high  |    0..1    |
//     6      |        3       |  low   |    0..1    |
//     7      |        3       |  high  |    0..1    |
//
// if (SkipUnEven = false)
// servoIndex | rsAddressIndex | nibble | nibbleBits |
// ---------- | -------------- | -----  | ---------- |
//     0      |        0       |  low   |    0..1    |
//     1      |        0       |  low   |    2..3    |
//     2      |        0       |  high  |    0..1    |
//     3      |        0       |  high  |    2..3    |
//     4      |        1       |  low   |    0..1    |
//     5      |        1       |  low   |    2..3    |
//     6      |        1       |  high  |    0..1    |
//     7      |        1       |  high  |    2..3    |
//
// *****************************************************************************************************
#include <Arduino.h>                        // For general definitions
#include "myRSBus.h"
#include "servo_CVs.h"                      // for the #define DIRECTION
#include "myServo.h"                        // Class definition, to access the servo objects from main


// We need to be able to access some objects from main()
extern MyServo servo[MAX_NUMBER_OF_SERVOS];
extern RSbusHardware rsbusHardware;         // Instantiated in rs_bus.cpp. Needed for the RS LED


void Feedbacks::init(uint8_t firstAddress, bool skipUneven, uint8_t numberOfFeedbacks) {
  // Step 1: set the RSbus LED as output (servo-3 and servo-6 board only)
  #ifdef LED_RS
    pinMode(LED_RS, OUTPUT);
  #endif
  // Step 2: Set the local variable separateNibbles
  separateNibbles = skipUneven;
  // Step 3: determine the number of rsbus addresses we need
  if (separateNibbles) rsbusAddressesInUse = (numberOfFeedbacks + 1) / 2;
  else rsbusAddressesInUse = (numberOfFeedbacks + 3) / 4;
  // Step 4: initialise each RSbus object, provided the address is valid
  for (uint8_t i = 0; i < rsbusAddressesInUse; i++) {
    uint8_t addr = firstAddress + i;
    if (addr >= 1 && addr <= 127) {
      rsbus[i].address = addr;
      rsbus[i].type = Switch;
      rsbus[i].feedback8Bit = 0;
      rsbus[i].lastSent8Bit = 0xFF;
    }
  };
};


// initData() and set() are very similar, except that set()   
// immediately sends a RSbus nible.
void Feedbacks::initData(uint8_t servoIndex, bool position) {
  // position: false = diverging (red, -), true = straight (green, +)
  uint8_t index;   // this is the index into the RSbus connections array
  if (servoIndex >= MAX_NUMBER_OF_SERVOS) return;
  if (separateNibbles) {
    index = servoIndex / 2;
    switch (servoIndex - index * 2) {
      case 0: 
        setLowNibble(index, position);
      break;
      case 1: 
        setHighNibble(index, position);
      break;  
    }
  }
  else {
    index = servoIndex / 4;
    switch (servoIndex - index * 4) {
      case 0: 
        setFB01(index, position);
      break;
      case 1: 
        setFB23(index, position);
      break;
      case 2: 
        setFB45(index, position);
      break;
      case 3: 
        setFB67(index, position);
      break;    
    }
  }
};


void Feedbacks::send(uint8_t servoIndex, bool position) {
  // position: false = diverging (red, -), true = straight (green, +)
  uint8_t index;   // this is the index into the RSbus connections array
  uint8_t nibble;
  if (servoIndex >= MAX_NUMBER_OF_SERVOS) return;  // fail silently
  if (separateNibbles) {
    index = servoIndex / 2;
    switch (servoIndex - index * 2) {
      case 0: 
        nibble = setLowNibble(index, position);
        sendIfChanged(index, nibble, LowBits);
      break;
      case 1: 
        nibble = setHighNibble(index, position);
        sendIfChanged(index, nibble, HighBits);
      break;  
    }
  }
  else {
    index = servoIndex / 4;
    switch (servoIndex - index * 4) {
      case 0: 
        nibble = setFB01(index, position);
        sendIfChanged(index, nibble, LowBits);
      break;
      case 1: 
        nibble = setFB23(index, position);
        sendIfChanged(index, nibble, LowBits);
      break;
      case 2: 
        nibble = setFB45(index, position);
        sendIfChanged(index, nibble, HighBits);  
      break;
      case 3: 
        nibble = setFB67(index, position);
        sendIfChanged(index, nibble, HighBits);  
      break;    
    }
  }
}


void Feedbacks::checkFeedbacks() {
  // Should be called from main as frequent as possible
  // As frequent as possible we should check if the RS-Bus asks for the most recent feedback data.
  // This is the case after a decoder restart or after a RS-Bus error. In addition, we have to 
  // check if the buffer contains feedback data, and the ISR is ready to send that data via the UART.
  for (uint8_t i = 0; i < rsbusAddressesInUse; i++) {
    if (rsbus[i].feedbackRequested) rsbus[i].send8bits(rsbus[i].feedback8Bit);
    rsbus[i].checkConnection();
  };
  // Step 2: set / clear the RS-Signal LED (servo-3 and servo-6 board only)
  #ifdef LED_RS
    if (rsbusHardware.rsSignalIsOK) digitalWrite(LED_RS, 1); 
    else digitalWrite(LED_RS, 0);    // No valid RS-Bus signal
  #endif
}


// *****************************************************************************************************
// Private support routines
// *****************************************************************************************************
void Feedbacks::sendIfChanged(uint8_t index, uint8_t nibble, Nibble_t which) {
  uint8_t oldNibble;
  if (which == LowBits)
    oldNibble = rsbus[index].lastSent8Bit & 0x0F;          // low nibble
  else
    oldNibble = (rsbus[index].lastSent8Bit >> 4) & 0x0F;   // high nibble
  if (nibble != oldNibble) {
    rsbus[index].send4bits(which, nibble);
    rsbus[index].lastSent8Bit = rsbus[index].feedback8Bit;
  }
}


uint8_t Feedbacks::setLowNibble(uint8_t index, bool position) {
  uint8_t nibble;
  if (position) nibble = 0b00000010;
  else nibble = 0b00000001;
  rsbus[index].feedback8Bit &= ~0b00001111;         // clear nibble 0
  rsbus[index].feedback8Bit |= nibble;              // Add nibble 0
  return nibble;
}

uint8_t Feedbacks::setHighNibble(uint8_t index, bool position) {
  uint8_t nibble;
  if (position) nibble = 0b00000010;
  else nibble = 0b00000001;
  rsbus[index].feedback8Bit &= ~0b11110000;         // clear nibble 1
  rsbus[index].feedback8Bit |= (nibble << 4);       // Add nibble 1
  return nibble;
}


uint8_t Feedbacks::setFB01(uint8_t index, bool position) {  // xxxxxx10 or xx01xx01
  uint8_t nibble;
  nibble = rsbus[index].feedback8Bit & 0b00001100;  // keep bits 2 and 3
  if (position) nibble |= 0b00000010;               // set bit 1
    else nibble |= 0b00000001;                      // set bit 0
  rsbus[index].feedback8Bit &= ~0b00001111;         // clear nibble 0
  rsbus[index].feedback8Bit |= nibble;              // Add nibble 0
  return nibble;
}

uint8_t Feedbacks::setFB23(uint8_t index, bool position) {  // xxxx10xx or xxxx01xx
  uint8_t nibble;
  nibble = rsbus[index].feedback8Bit & 0b00000011;  // keep bits 0 and 1
  if (position) nibble |= 0b00001000;               // set bit 3
    else nibble |= 0b00000100;                      // set bit 2
  rsbus[index].feedback8Bit &= ~0b00001111;         // clear nibble 0
  rsbus[index].feedback8Bit |= nibble;              // Add nibble 0
  return nibble;
}

uint8_t Feedbacks::setFB45(uint8_t index, bool position) {  // xx10xxxx or xx01xxxx
  uint8_t nibble;
  nibble = rsbus[index].feedback8Bit & 0b11000000;  // keep bits 6 and 7
  nibble = nibble >> 4;                             // move high nibble to low 
  if (position) nibble |= 0b00000010;               // set bit 1
    else nibble |= 0b00000001;                      // set bit 0
  rsbus[index].feedback8Bit &= ~0b11110000;         // clear nibble 1
  rsbus[index].feedback8Bit |= (nibble << 4);       // Add nibble 1
  return nibble;
}

uint8_t Feedbacks::setFB67(uint8_t index, bool position) {  // 10xxxxxx or 01xxxxxx
  uint8_t nibble;
  nibble = rsbus[index].feedback8Bit & 0b00110000;  // keep bits 4 and 5
  nibble = nibble >> 4;                             // move high nibble to low 
  if (position) nibble |= 0b00001000;               // set bit 3
    else nibble |= 0b00000100;                      // set bit 2
  rsbus[index].feedback8Bit &= ~0b11110000;         // clear nibble 1
  rsbus[index].feedback8Bit |= (nibble << 4);       // Add nibble 1
  return nibble;
}
