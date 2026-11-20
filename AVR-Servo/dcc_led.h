// *****************************************************************************************************
//
// File:      dcc_led.h
// Author:    Aiko Pras
// History:   2025/12/01 ap: first production version 
// 
// Purpose:   Controls the DCC LED
//
// *****************************************************************************************************
#pragma once
#include <Arduino.h>                         // For general definitions
#include <AP_DCC_Decoder_Core.h>             // To include the DCC timer object

class DCC_LED {
  private:
    DccTimer DccLedTimer;

  public:
    DCC_LED();
    void On();
    void check();
};


extern DCC_LED dccLed;                       // LED object that indicates if we have a DCC signal
