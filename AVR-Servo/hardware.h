// *******************************************************************************************************
// File:      hardware.h
// Author:    Aiko Pras
// History:   2025/02/11 AP: Version 1.0
//            2025/10/09 AP: V1.1 first production version
//            2025/12/01 AP: V1.2 Modified for the 3-Servo Board V2.2 (2025/07)
//            2026/11/17 AP: V1.3 Board selection: servo-3 board V2.2 (2025/07) or servo-6 board V2.3 (2025/12),
//                           MAX_NUMBER_OF_SERVOS (was NUMBER_OF_SERVOS), EEPROM_MAX_SERVOS_INDEX
//            2026/11/18 AP: V1.4 servo-2 board V2.0 (2022/07) added (relays, position buttons), chip check,
//                           LED_DCC and LED_RS only on the servo-3 and servo-6 board, POWER_WHEN_IDLE
//
// Purpose:   AVR Servo - Details for the servo-2 board V2.0 (2022/07), the servo-3 board V2.2 (2025/07)
//            and the servo-6 board V2.3 (2025/12)
//
// The AVR Servo decoder software has been developed for the AVR64DA28 (servo-2 board) and the
// AVR32DA48 (servo-3 and servo-6 board) processors.
//
// Note that all DCC and RS-Bus pins are defined and initialised in the AP_DCC_Decoder_Core library
// (boards.h). The library selects these pins via the processor type:
//                servo-2 board (AVR64DA28)    servo-3 / servo-6 board (AVR32DA48)
// RSBus RX:      PA0                          PA0
// RSBus TX:      PA4                          PA4
// LED (onboard): PA7 (red)                    PB0 (red)
// DCC IN:        PD0                          PA1
// DCC ACK:       PD1                          PA2
// TASTER:        PD2                          PA3
//
// Compile settings:
// =================
// The following settings are used (under the Tools section of the Arduino IDE):
// - Board: AVR DA-series (no bootloader)  - Important to set this right
// - Chip: AVR64DA28 (servo-2 board)       - Important to set this right. Checked below
//         AVR32DA48 (servo-3 and servo-6 board)
// - Clock: 24 Mhz                         - Might work at other speeds
// - Millis: TCB2                          - Default
// - BOD: 2.7V                             - Servo's power is non-neglectable, so enable BOD
// - BOD Mode: Enabled                     - see above.
// - EEPROM: ***                           - Seems that retained doesn't work
// - Startup time: 8ms                     - Default
// - FLMAP: Use last section               - Here we store PROGMEM variables
// - Port: usbserial....                   - Depends on the specifics of your development system
// - Programmer: Serial UPDI               - Depends on the specifics of your development system
//
// Note: to activate the BOD settings, a "burn bootloader" command is needed. Such command sets the fuses.
// ******************************************************************************************************
#pragma once

// Board selection
// ===============
// Select the board for which the software will be compiled (exactly one of the defines below).
// The board determines the maximum number of servos (MAX_NUMBER_OF_SERVOS), the pins and the decoder
// type (DECODER_TYPE). These are hardware properties, which can not be changed via PoM or SM.
// The number of switches (DCC and RS-Bus addresses) can be lowered via the SwitchCount CV (normal variant).
// #define SERVO_2_BOARD                // V2.0 (2022/07): maximum 2 servos, AVR64DA28
#define SERVO_3_BOARD                   // V2.2 (2025/07): maximum 3 servos, AVR32DA48
// #define SERVO_6_BOARD                // V2.3 (2025/12): maximum 6 servos, AVR32DA48


// Number of Servos
// The number of servos follows from the board selection above.
// Although the software is written to support multiple servos, the maximum amount is determined by:
// - The board (V2.0 (2022/07) supports 2 Servos, V2.2 (2025/07) supports 3 servos, V2.3 (2025/12) supports 6 servos)
// - The number of TCA timers. Each TCA timer supporst upto 3 servos. The AVR32DA48 has two TCA timers,
//   the AVR64DA28 has one TCA timer.
// - The EEPROM size. Size = 256 => 4 servos / size = 512 => 8 servos
#if (defined(SERVO_2_BOARD) + defined(SERVO_3_BOARD) + defined(SERVO_6_BOARD)) > 1
  #error Select only one board!
#elif defined(SERVO_2_BOARD)
  #define DECODER_TYPE          ServoDecoder
  #define MAX_NUMBER_OF_SERVOS  2
  #define POWER_WHEN_IDLE       0         // Servo power off between movements (see below)
#elif defined(SERVO_3_BOARD)
  #define DECODER_TYPE          Servo3Decoder
  #define MAX_NUMBER_OF_SERVOS  3
  #define POWER_WHEN_IDLE       1         // Servo power stays on between movements (see below)
#elif defined(SERVO_6_BOARD)
  #define DECODER_TYPE          Servo6Decoder
  #define MAX_NUMBER_OF_SERVOS  6
  #define POWER_WHEN_IDLE       0         // Servo power off between movements (see below)
#else
  #error No board selected!
#endif

// Servo power between movements (POWER_WHEN_IDLE)
// ===============================================
// Whether the servo power stays on between movements depends on the board and the ServoType:
// - Generic servo (ServoType 0): the PowerWhenIdle CV. Its default value is POWER_WHEN_IDLE.
// - Uhlenbrock 81420 / Weinert (ServoType 1): POWER_WHEN_IDLE. On the servo-3 board these servos
//   do not always work reliable if the power is switched off between movements.
// - MBTronic and SG90 (ServoType 2..4): power off between movements, on all boards.

// The AP_DCC_Decoder_Core library selects the DCC and RS-Bus pins via the processor type.
// Check that the processor (Tools => Chip) matches the selected board.
#if defined(SERVO_2_BOARD) && !defined(__AVR_AVR64DA28__)
  #error The servo-2 board requires the AVR64DA28 (Tools => Chip)
#endif
#if (defined(SERVO_3_BOARD) || defined(SERVO_6_BOARD)) && !defined(__AVR_AVR32DA48__)
  #error The servo-3 and servo-6 board require the AVR32DA48 (Tools => Chip)
#endif


// In addition to the normal (DCC, RS-bus, LED, Taster) hardware, the AVR Servo decoder has
// the follwing specific hardware:
// - Two (servo-2 board), three (servo-3 board) or six (servo-6 board) servo pulse outputs
// - Two, three or six Servo Enable pins
// - A serial monitor interface
// - An extra LED, to indicate configuration mode
// The servo-2 board has in addition:
// - Two relays (to change frog polarity)
// - Two pins for buttons, to directly change the switch positions
// The servo-3 and servo-6 board have in addition:
// - Extra LEDs, to indicate valid DCC/RSbus signals

#if defined(SERVO_2_BOARD)
  #define SERVO0_PIN            PIN_PF0
  #define SERVO1_PIN            PIN_PF1
  #define SERVO0_ENABLE_PIN     PIN_PA1
  #define SERVO1_ENABLE_PIN     PIN_PA2

  #define POSITION0_PIN         PIN_PA5
  #define POSITION1_PIN         PIN_PA6
  #define DEBOUNCE_TIME         100       // 100 ms

  #define RELAYS0_PIN           PIN_PA3
  #define RELAYS1_PIN           PIN_PD3

  #define LED_CONFIG            PIN_PD4
#endif

#if defined(SERVO_3_BOARD)
  #define SERVO0_PIN            PIN_PF0
  #define SERVO1_PIN            PIN_PF1
  #define SERVO2_PIN            PIN_PF2
  #define SERVO0_ENABLE_PIN     PIN_PF3
  #define SERVO1_ENABLE_PIN     PIN_PF4
  #define SERVO2_ENABLE_PIN     PIN_PF5
#endif

#if defined(SERVO_6_BOARD)
  // Servo 1..3 (SERVO0..2) use TCA1, servo 4..6 (SERVO3..5) use TCA0
  #define SERVO0_PIN            PIN_PC4
  #define SERVO1_PIN            PIN_PC5
  #define SERVO2_PIN            PIN_PC6
  #define SERVO3_PIN            PIN_PF0
  #define SERVO4_PIN            PIN_PF1
  #define SERVO5_PIN            PIN_PF2
  #define SERVO0_ENABLE_PIN     PIN_PD2
  #define SERVO1_ENABLE_PIN     PIN_PD3
  #define SERVO2_ENABLE_PIN     PIN_PD4
  #define SERVO3_ENABLE_PIN     PIN_PF3
  #define SERVO4_ENABLE_PIN     PIN_PF4
  #define SERVO5_ENABLE_PIN     PIN_PF5
#endif

#if defined(SERVO_3_BOARD) || defined(SERVO_6_BOARD)
  #define LED_DCC               PIN_PB1
  #define LED_RS                PIN_PB2
  #define LED_CONFIG            PIN_PB3
#endif

#define SERVO_ENABLE_VALUE    1         // servo gets powered with a high signal (all three boards)


#define Monitor               Serial1


//*****************************************************************************************************
// EEPROM specific settings and usage - Do not edit below!
//*****************************************************************************************************
//
//   0        1 .. 63       64             65...                                                    511
// +---+------------------+---+-----------------------------+----------------+-----------------------+
// | I |    CVs: 1..63    | # |   Servo specific CVs: 65..  |     Curves     |    Circular Buffer    |
// +---+------------------+---+-----------------------------+----------------+-----------------------+
//
// Contents of the EEPROM:
// - The first EEPROM byte indicates if the EEPROM has been initialised (the value 0b01010101)
// - The following 63 bytes hold the default CVs, as defined in "AP_DCC_Decoder_Core"
// - Byte 64 holds the maximum number of servos that this board is able to support (see #define above)
//   Note that the number of switches (DCC and RS-Bus addresses) is set via the SwitchCount CV (CV35).
//   Byte 64 can not be written via PoM or SM.
// - The following bytes hold the servo specific CVs. Per servo, 18 bytes are used. Thus for 2 servos
//   this is 36 bytes, for 3 it is 54 and for 6 it is 108.
// - After the servo specific CVs there is space for 2 or 4 curves. Each curve requires 48 bytes
//   If the total EEPROM size is 256 bytes, we have room for 2 curves. If the EEPROM is 512, there
//   is room for 4 curves.
// - The last part of EEPROM space is used by the circular buffer. The goal of this buffer is
//   to improve EEPROM endurance. The first byte holds the number of boots, the other bytes
//   store the most recently used servo curves / positions. See servo_position.h for details.
//
// Example for a 512 byte EEPROM and 2 servos:
// -       0: EEPROM has been initialized
// -    1-63: default CVs
// -      64: maximum number of servos
// -   65-82: Servo-0 => 18 bytes
// -  83-100: Servo-1
// - 101-148: curve 0 => 48 bytes
// - 149-196: Default curve 1
// - 197-244: Default curve 2
// - 245-292: Default curve 4
// -     293: Number of Boots (311 for 3 servos)
// - 294-511: circular buffer for holding the last curve/direction => 216 bytes
//
// All EEPROM indexes will be automatically generated, once the MAX_NUMBER_OF_SERVOS and the EEPROM_SIZE
// are know. Therefore, do not change any of the #defines below. Note that it is important to embrace
// the strings from which the index values must be calculated by brackets!
//
//*****************************************************************************************************
#if (EEPROM_SIZE < 256)
  #error At least 256 bytes of EEPROM needed!
#elif (EEPROM_SIZE < 512)
  #define NUMBER_OF_CURVES 2
#else
  #define NUMBER_OF_CURVES 4
#endif

#define NUMBER_OF_SERVO_CVS            18

#define EEPROM_MAX_SERVOS_INDEX        64   // Fixed: configuration programs read the board maximum from CV64
#define START_INDEX_SERVO_CVS          65
#define START_INDEX_SERVO_CURVES       (START_INDEX_SERVO_CVS + (MAX_NUMBER_OF_SERVOS * NUMBER_OF_SERVO_CVS))
#define EEPROM_BOOTS_INDEX             (START_INDEX_SERVO_CURVES + (NUMBER_OF_CURVES * 48))

#define TEMP_SIZE_CIRCULAR_BUFFER      (EEPROM_SIZE - EEPROM_BOOTS_INDEX - 1)

// 2026/07/31 changed from 256 tot 254. Avoid 255 (undefined)
#if (TEMP_SIZE_CIRCULAR_BUFFER > 254)  // The stored circular buffer indexes are 8 bit. Avoid Overflow
  #define SIZE_CIRCULAR_BUFFER         254
#else
  #define SIZE_CIRCULAR_BUFFER         TEMP_SIZE_CIRCULAR_BUFFER
#endif
