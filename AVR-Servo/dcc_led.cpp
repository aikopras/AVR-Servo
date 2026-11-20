// *****************************************************************************************************
//
// File:      dcc_led.cpp
// Author:    Aiko Pras
// History:   2025/12/01 ap: first production version 
//            2026/11/18 ap: no DCC LED on the servo-2 board (LED_DCC not defined)
// 
// Purpose:   Controls the DCC LED
// Only the servo-3 and servo-6 board have a DCC LED (LED_DCC in hardware.h). On the servo-2 board
// the methods do nothing.
//
// *****************************************************************************************************
#include "dcc_led.h"
#include "hardware.h"


DCC_LED dccLed;                       // Instantiate a DCC LED object that indicates if we have a signal


DCC_LED::DCC_LED() {                  // Constructor
  #ifdef LED_DCC
    pinMode(LED_DCC, OUTPUT);
  #endif
}

void DCC_LED::On() {
  #ifdef LED_DCC
    digitalWriteFast(LED_DCC, HIGH);
    DccLedTimer.setTime(1000);
  #endif
}

void DCC_LED::check() {
  #ifdef LED_DCC
    if (DccLedTimer.expired()) {digitalWriteFast(LED_DCC, LOW);}
  #endif
}
