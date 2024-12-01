/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2021  Willi Kraml, OE1WKL                                                                            ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

#ifndef MORSEDEFS_H
#define MORSEDEFS_H

#include "Arduino.h"
#include <ESP32Encoder.h>   // https://github.com/madhephaestus/ESP32Encoder.git 
#include <ArduinoJson.h>
#include "ClickButton.h"   // button control library
#include "WiFi.h"
#include <WiFiUdp.h>       // UDP lib for WiFi TRX
#include <AsyncUDP.h>      // UDP lib for WiFi TRX
#include <Preferences.h>   // ESP 32 library for storing things in non-volatile storage
#include <WebServer.h>     // simple web sever
#include <ESPmDNS.h>       // DNS functionality
#include <Update.h>        // update "over the air" (OTA) functionality
#include "FS.h"
#include "SPIFFS.h"

/////// Program Name & Version

const String PROJECTNAME = "Morserino-32";

#define VERSION_MAJOR 6
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define BETA false                                                                                                                                                                                                                                                                                                                                                                                                                                                                


#define IGNORE_SERIALOUT false

// if IGNORE_SERIALOUT is true, alle DEBUG messages are on serial out, even when Serial Out is active outputting characters from Keyer, Decoder etc


// using the M32 serial protocol
//define M32Protocol version updateTimings()
#define M32P_VERSION "1.1"

/////// protocol version for IP (and LoRa) - for the time being this is B01
/////// the first version of the CW over LoRA protocol; future versions will be B02, B03, B00 (reserved for future use)
#define CW_TRX_PROTO_VERSION B01



namespace Buttons
{

    // define the buttons for the clickbutton library
    extern ClickButton modeButton;
    extern ClickButton volButton;

    //void setup();
    //void click();
    //void audioLevelAdjust();

}

///// its is crucial to have the right board version - Boards 2 and 2a (prototypes only) set it to 2, Boards 3 set it to 3
///// the Board Version 2 is for HEltec Modules V1 only, Board Version 3 for Heltec V2 only
///// Board version 1 not supported anymore!


///#define BOARDVERSION  4


///////////////////////////////      H A R D W A R E      ///////////////////////////////////////////
//// Here are the definitions for the various hardware-related I/O pins of the ESP32
/////////////////////////////////////////////////////////////////////////////////////////////////////
///// Board versions:
///// 2(a): for Heltec ESP32 LORA Version 1 (Morserino-32 prototypes)
///// 3: for heltec ESP32 LORA Version 2
///// Warning: Board version 1 not supported anymore!!!! (was an early prototype)
/////
///// the following Pins are dependent on the board version
///// 21:  V2: an encoder port, using interrupts      V3: Vext - used internally to switch Vext on and off
///// 39:  V2: ADC input to measure battery voltage   V3: encoder port (interrupt driven) instead of 21
///// 13:  V2: leftPin (external paddle, ext. pullup) V3: used internally to read battery voltage
///// 38:  V2: not in use                             V3: encoder port (interrupt driven) instead of 35
///// 32:  V2: used for LoRa internally               V3: leftPin
///// 33:  V2: used for LoRa internally               V3: rightPin
///// 34: rightPin                                    V3: cannot be used any longer, used for LoRa internally
///// 35: PinDT                                       V3: cannot be used any longer, used for LoRa internally


/////// here are the board dependent pins definitions

//// BOARD 3 & 4 differences
//// batteryPin 13 in 3, 37 in 4
//// these are NOT compile options in this version but will be determined at startup (in setup()), to achieve backwards compatibility


/// where is the encoder?
#ifdef PIN_ROT_CLK
const int PinCLK=PIN_ROT_CLK;          // Used for generating interrupts using CLK signal - needs external pullup resisitor!
#else
const int PinCLK=38;
#endif

#ifdef PIN_ROT_DT
const int PinDT=PIN_ROT_DT;            // Used for reading DT signal  - needs external pullup resisitor! 
#else
const int PinDT=39;
#endif

/// encoder switch (BLACK knob)
#ifdef PIN_ROT_CLK
const int modeButtonPin = PIN_ROT_BTN;
#else
const int modeButtonPin = 37;
#endif

/// 2nd switch button - toggles between Speed control and Volume control (RED button)
#ifdef PIN_VOL_BTN
const int volButtonPin = PIN_VOL_BTN;
#else
const int volButtonPin = 0;
#endif


//// with the following we define which pins are used as output for the two pwm channels
//// HF output (with varying duticycle and fixed frequency) and LF output (with varying frequency and fixed dutycycle of 50%)
/// are being added with a 2-transistor AND gate to create a tone frequency with variable frequency and volume

#ifdef PIN_LF
const int LF_Pin = PIN_LF;    // for the lower (= NF) frequency generation
#else
const int LF_Pin = 23;
#endif

#ifdef PIN_HF
const int HF_Pin = PIN_HF;    // for the HF PWM generation
#else
const int HF_Pin = 22;
#endif


/// where are the touch paddles?
#ifdef PIN_TOUCH_LEFT
const int LEFT = PIN_TOUCH_LEFT;
#else
const int LEFT = T2;        // = Pin 2
#endif
#ifdef PIN_TOUCH_RIGHT
const int RIGHT = PIN_TOUCH_RIGHT;
#else
const int RIGHT = T5;       // = Pin 12
#endif

// Tx keyer 
#ifdef PIN_KEYER
const int keyerPin = PIN_KEYER; // this keys the transmitter / through a MOSFET Optocoupler - at the same time lights up the LED
#else
const int keyerPin = 25;
#endif


// audio in
#ifdef PIN_AUDIO_IN
const int audioInPin = PIN_AUDIO_IN;      // audio in for Morse decoder // 
#else
const int audioInPin = 36;
#endif


// NF Line-out (for iCW etc.)
#ifdef PIN_AUDIO_OUT
const int lineOutPin = PIN_AUDIO_OUT; // for NF line out
#else
const int lineOutPin = 17; // for NF line out
#endif

#ifndef SKIP_LEGACY_PINDEFS // workaround to allow arduino compilation with default values for legacy heltec v2
#ifndef OLED_SDA
#define OLED_SDA 4
#endif

#ifndef OLED_SCL
#define OLED_SCL 15
#endif

#ifndef OLED_RST
#define OLED_RST 16
#endif

#ifndef PIN_VEXT
#define PIN_VEXT 21
#endif
#endif

// SENS_FACTOR is used for auto-calibrating sensitivity of touch paddles (somewhere between 2.0 and 2.5)
#define SENS_FACTOR 2.22

#ifndef BAND
#define BAND    433E6  //you can set band here directly,e.g. 868E6,915E6
#endif

#ifndef LORA_DISABLED // this is used to default to lora for Arduino IDE legacy builds
#define LORA_RADIOLIB
#endif

#ifdef RADIO_SX1262
#define RADIO SX1262
#else
#define RADIO SX1278
#define RADIO_SX1278 1
#endif

// heltec lora v2 pins as defaults

#ifndef LoRa_nss
#define LoRa_nss 18
#endif

#ifndef LoRa_dio0
#define LoRa_dio0 26
#endif

#ifndef LoRa_dio1
#define LoRa_dio1 35
#endif

#ifdef LoRa_dio2
#define LoRa_dio2 34
#endif

#ifndef LoRa_MISO
#define LoRa_MISO 19
#endif

#ifndef LoRa_MOSI
#define LoRa_MOSI 27
#endif

#ifndef LoRa_nrst
#define LoRa_nrst 14
#endif

#ifndef LoRa_SCK
#define LoRa_SCK SCK
#endif

#ifndef LoRa_busy
#define LoRa_busy 13
#endif

/*
  heltec v2:
  lora cs: 18
  lora sck: 5
*/

///////////////////////////////////////// END OF HARDWARE DEFS ////////////////////////////////////////////////////////////////////


// defines for keyer modi
//

#define    IAMBICA      1          // Curtis Mode A
#define    IAMBICB      2          // Curtis Mode B (with enhanced Curtis timing, set as parameter
#define    ULTIMATIC    3          // Ultimatic mode
#define    NONSQUEEZE   4          // Non-squeeze mode of dual-lever paddles - simulate a single-lever paddle
#define    STRAIGHTKEY  5          // use of a straight key (for echo training etc) - not really a "keyer" mode


enum DISPLAY_TYPE               // how we display in trainer mode
  {
      NO_DISPLAY, DISPLAY_BY_CHAR, DISPLAY_BY_WORD
  };

enum random_OPTIONS             // what we generate
  {
      OPT_ALL, OPT_ALPHA, OPT_NUM, OPT_PUNCT, OPT_PRO, OPT_ALNUM, OPT_NUMPUNCT, OPT_PUNCTPRO, OPT_ALNUMPUNCT, OPT_NUMPUNCTPRO, OPT_KOCH, OPT_KOCH_ADAPTIVE
  };
enum PROMPT_TYPE                // how we prompt in echo trainer mode
  {
      NO_PROMPT, CODE_ONLY, DISP_ONLY, CODE_AND_DISP
  };

enum GEN_TYPE                   // the things we can generate in generator mode
  { 
      RANDOMS, ABBREVS, WORDS, CALLS, MIXED, PLAYER, KOCH_MIXED, KOCH_LEARN, KOCH_ADAPTIVE
  };              


enum DECODER_STATES             // state machine for decoding CW
  {
      LOW_, HIGH_, INTERELEMENT_, INTERCHAR_
  };

enum encoderMode                // define modes for state machine of the various modi the encoder can be in
  {
      speedSettingMode, volumeSettingMode, scrollMode, memSelMode
  }; 

enum morserinoMode              // the states the morserino can be in - selected in top level menu
  {
      morseKeyer, loraTrx, wifiTrx, morseTrx, morseGenerator, echoTrainer, morseDecoder, shutDown, measureNF, invalid
  };

const uint8_t menuN = 43;     // no of menu items +1

enum menuNo 
  {   _dummy, _keyer, _gen, _genRand, _genAbb, _genWords, _genCalls, _genMixed, _genPlayer,
        _echo, _echoRand, _echoAbb, _echoWords, _echoCalls, _echoMixed, _echoPlayer,
        _koch, _kochSel, _kochLearn, _kochGen, _kochGenRand, _kochGenAbb, _kochGenWords,
        _kochGenMixed, _kochEcho, _kochEchoRand, _kochEchoAbb, _kochEchoWords, _kochEchoMixed, _kochEchoAdaptive,
        _trx, _trxLora, _trxWifi, _trxIcw, _decode, _wifi, _wifi_mac, _wifi_config, _wifi_check, _wifi_upload, _wifi_update, _wifi_select, _goToSleep 
  };

enum loops
  { active_loop, menu_loop, preferences_loop
  };

  
enum echoStates 
  {   
      START_ECHO, SEND_WORD, REPEAT_WORD, GET_ANSWER, COMPLETE_ANSWER, EVAL_ANSWER 
  };

enum KEYERSTATES 
  {
      IDLE_STATE, DIT, DAH, KEY_START, KEYED, INTER_ELEMENT 
  };

  
enum prefPos : uint8_t {
                posClicks, posPitch, posExtPddlPolarity, posPolarity,                                         // 0
                posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,                              // 4
                posEchoToneShift, posInterWordSpace, posInterCharSpace, posRandomOption,                      // 8
                posRandomLength, posCallLength, posAbbrevLength, posWordLength,                               // 12
                posGeneratorDisplay, posWordDoubler, posEchoDisplay, posEchoRepeats,  posEchoConf,            // 16           
                posKeyExternalTx, posLoraCwTransmit, posGoertzelBandwidth, posSpeedAdapt,                     // 21
                posKochSeq, posCarouselStart, posLatency, posRandomFile, posExtAudioOnDecode, posTimeOut,     // 25
                posQuickStart, posAutoStop,posMaxSequence, posLoraChannel,   posSerialOut,                    // 31
                // to be treated differently:
                posKochFilter,                                                                                // 36
                posLoraBand, posLoraQRG, posLoraPower, posSnapRecall, posSnapStore,  posVAdjust, posHwConf    // 37
};
  
enum actMessage : int {
  ACT_EXIT, ACT_ON, ACT_SET, ACT_CANCELLED, ACT_RECALLED, ACT_CLEARED
};

extern void DEBUG (String s);


// we need this for some strange reason: the min definition breaks with WiFi
#define _min(a,b) ((a)<(b)?(a):(b))
#define _max(a,b) ((a)>(b)?(a):(b))

#endif
