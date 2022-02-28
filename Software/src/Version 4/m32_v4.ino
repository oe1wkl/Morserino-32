/******************************************************************************************************************************
 *  m32_v4 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018-2020  Willi Kraml, OE1WKL                                                                            ***
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

 /*****************************************************************************************************************************
 *  code by others used in this sketch, apart from the ESP32 libraries distributed by Heltec
 *  (see: https://heltec-automation-docs.readthedocs.io/en/latest/esp32+arduino/quick_start.html)
 *
 *  ClickButton library -> https://code.google.com/p/clickbutton/ by Ragnar Aronsen
 * 
 * 
 *  For volume control of NF output: I used a similar principle as  Connor Nishijima, see 
 *                                   https://hackaday.io/project/11957-10-bit-component-less-volume-control-for-arduino
 *                                   but actually using two PWM outputs, connected with an AND gate
 * 
 ****************************************************************************************************************************/


#include "morsedefs.h"
#include "wklfonts.h"         // monospaced fonts in size 12 (regular and bold) for smaller text and 15 for larger text (regular and bold), called :
                              // DialogInput_plain_12, DialogInput_bold_12 & DialogInput_plain_15, DialogInput_bold_15
                              // these fonts were created with this tool: http://oledHeltec.display -> squix.ch/#/home
#include "abbrev.h"           // common CW abbreviations
#include "english_words.h"    // common English words
#include "MorseOutput.h"      // display and sound functions
#include "MorsePreferences.h" // preferences and persistent storage, snapshots
#include "MorseMenu.h"        // main menu
#include "MorseWiFi.h"        // WiFi functions
#include "goertzel.h"         // Goertzel filter
#include "MorseDecoder.h"     // Decoder Engine


// define the buttons for the clickbutton library, & other classes that we need

/// variables, value defined at setup()
uint8_t batteryPin;
int leftPin, rightPin; /// where are the external paddles?


/// variables for battery measurement
uint16_t volt;              // store measure battery voltage here
double voltage_raw;         // raw measurement

using namespace Buttons;

ClickButton Buttons::modeButton(modeButtonPin);              // activeHigh must be set in board version 4
ClickButton Buttons::volButton(volButtonPin);               // external pullup for this one

Decoder keyDecoder(USE_KEY);
Decoder audioDecoder(USE_AUDIO);
MorseTable keyerTable;
Koch koch;

// things for reading the encoder
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;


volatile int8_t _oldState;

#define LATCHSTATE 3

volatile int8_t encoderPos = 0;
volatile uint64_t IRTime = 0;   // interrupt time
const int encoderWaitTime = 100 ;         // how long to wait for next reading from encoder in microseconds
volatile uint8_t stateRegister = 0;

// positions: [3] 1 0 2 [3] 1 0 2 [3]
// [3] is the positions where my rotary switch detends
// ==> right, count up
// <== left,  count down



///// the states the morserino can be in - selected intop level menu
morserinoMode morseState = morseKeyer;

boolean quickStart;                                     // should we execute menu item immediately?




// define modes for state machine of the various modi the encoder can be in
encoderMode encoderState = speedSettingMode;    // we start with adjusting the speed


///////////////////////////////////
//// Other Global VARIABLES ////////////
/////////////////////////////////

unsigned int interCharacterSpace, interWordSpace;   // need to be properly initialised!
unsigned int effWpm;                                // calculated effective speed in WpM
unsigned int lUntouched = 0;                        // sensor values (in untouched state) will be stored here
unsigned int rUntouched = 0;

boolean alternatePitch = false;                     // to change pitch in CW generator / file player

enum AutoStopModes 
  {
      nextword, halt, repeatword
  }; 

AutoStopModes autoStop = halt;

GEN_TYPE generatorMode = RANDOMS;          // trainer: what symbol (groups) are we going to send?            0 -  5

////////////////// Variable for file handling
File file;

/// variable for selecting a hardware configuratipon menu option
int8_t hwConf = 1;                            // 0 = cancel hw config, 1 = battery measurement calibration, 2 = lora config

boolean kochActive = false;                   // set to true when in Koch trainer mode

//  keyerControl bit definitions

#define     DIT_L      0x01     // Dit latch
#define     DAH_L      0x02     // Dah latch
#define     DIT_LAST   0x04     // Dit was last processed element

//  Global Keyer Variables
//
unsigned char keyerControl = 0; // this holds the latches for the paddles and the DIT_LAST latch, see above

//enum KEYERSTATES {IDLE_STATE, DIT, DAH, KEY_START, KEYED, INTER_ELEMENT };

boolean DIT_FIRST = false; // first latched was dit?
unsigned int ditLength ;        // dit length in milliseconds - 100ms = 60bpm = 12 wpm
unsigned int dahLength ;        // dahs are 3 dits long
KEYERSTATES keyerState;
unsigned long charCounter = 25; // we use this to count characters after changing speed - after n characters we decide to write the config into NVS
uint8_t sensor;                 // what we read from checking the touch sensors
boolean leftKey, rightKey;


unsigned long interWordTimer = 0;      // timer to detect interword spaces
unsigned long acsTimer = 0;            // timer to use for automatic character spacing (ACS)


const String CWchars = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKEäöüH";
//                      0....5....1....5....2....5....3....5....4....5....5...    
// we use substrings as char pool for trainer mode
// SANK will be replaced by <as>, <ka>, <kn> and <sk>, H = ch
// a = CWchars.substring(0,26); 9 = CWchars.substring(26,36); ? = CWchars.substring(36,45); <> = CWchars.substring(44,49);
// a9 = CWchars.substring(0,36); 9? = CWchars.substring(26,45); ?<> = CWchars.substring(36,50);
// a9? = CWchars.substring(0,45); 9?<> = CWchars.substring(26,50);
// a9?<> = CWchars;


///// variables for generating CW

String CWword = "";
String clearText = "";

int repeats = 0;

int rxDitLength = 0;                    // set new value for length of dits and dahs and other timings
int rxDahLength = 0;
int rxInterCharacterSpace = 0;
int rxInterWordSpace = 0;


boolean active = false;                           // flag for trainer mode
boolean startFirst = true;                        // to indicate that we are starting a new sequence in the trainer modi
boolean firstTime = true;                         /// for word doubler mode

uint8_t wordCounter = 0;                          // for maxSequence
uint16_t errCounter = 0;                          // counting errors in echo trainer mode
boolean stopFlag = false;                         // for maxSequence
boolean echoStop = false;                         // for maxSequence

unsigned long genTimer;                         // timer used for generating morse code in trainer mode

enum MORSE_TYPE {KEY_DOWN, KEY_UP };                    //   State Machine Defines
unsigned char generatorState;



// for each character:
// byte length// byte morse encoding as binary value, beginning with most significant bit

byte poolPair[2];           // storage in RAM for one morse code character

const byte pool[][2]  = {
// letters
               {B01000000, 2},  // a    0     
               {B10000000, 4},  // b
               {B10100000, 4},  // c
               {B10000000, 3},  // d
               {B00000000, 1},  // e
               {B00100000, 4},  // f
               {B11000000, 3},  // g
               {B00000000, 4},  // h
               {B00000000, 2},  // i
               {B01110000, 4},  // j 
               {B10100000, 3},  // k
               {B01000000, 4},  // l
               {B11000000, 2},  // m  
               {B10000000, 2},  // n
               {B11100000, 3},  // o
               {B01100000, 4},  // p  
               {B11010000, 4},  // q
               {B01000000, 3},  // r
               {B00000000, 3},  // s
               {B10000000, 1},  // t
               {B00100000, 3},  // u
               {B00010000, 4},  // v
               {B01100000, 3},  // w
               {B10010000, 4},  // x
               {B10110000, 4},  // y
               {B11000000, 4},  // z  25
// numbers
               {B11111000, 5},  // 0  26    
               {B01111000, 5},  // 1
               {B00111000, 5},  // 2
               {B00011000, 5},  // 3
               {B00001000, 5},  // 4
               {B00000000, 5},  // 5
               {B10000000, 5},  // 6
               {B11000000, 5},  // 7
               {B11100000, 5},  // 8
               {B11110000, 5},  // 9  35
// interpunct   . , : - / = ? @ +    010101 110011 111000 100001 10010 10001 001100 011010 01010
               {B01010100, 6},  // .  36    
               {B11001100, 6},  // ,  37    
               {B11100000, 6},  // :  38    
               {B10000100, 6},  // -  39    
               {B10010000, 5},  // /  40    
               {B10001000, 5},  // =  41    
               {B00110000, 6},  // ?  42    
               {B01101000, 6},  // @  43    
               {B01010000, 5},  // +  44    (at the same time <ar> !) 
// Pro signs  <>  <as> <ka> <kn> <sk>
               {B01000000, 5},  // <as> 45 S
               {B10101000, 5},  // <ka> 46 A
               {B10110000, 5},  // <kn> 47 N
               {B00010100, 6},   // <sk> 48    K
               {B00010000, 5},  // <ve> 49 E
// German characters
               {B01010000, 4},  // ä    50   
               {B11100000, 4},  // ö    51
               {B00110000, 4},  // ü    52
               {B11110000, 4}   // ch   53  H

            };

////////////////////////////////////////////////////////////////////
///// Variables for Echo Trainer Mode
/////

String echoResponse = "";
//enum echoStates { START_ECHO, SEND_WORD, REPEAT_WORD, GET_ANSWER, COMPLETE_ANSWER, EVAL_ANSWER };
echoStates echoTrainerState = START_ECHO;
String echoTrainerPrompt, echoTrainerWord;


////// variables for CW decoder

boolean lastWasKey = true;        // we want to know which decoder was last active, to get the decoded wpm from there; true if it was from the straight key
boolean speedChanged = false;

/////////////////// Variables for LoRa: Buffer management etc

char cwTxBuffer[128];

uint8_t cwRxBuffer[1024];
uint16_t byteBuFree = sizeof(cwRxBuffer);
uint8_t nextBuWrite = 0;
uint8_t nextBuRead = 0;

uint8_t cwTxSerial;                                     /// a 6 bit serial number, start with some random value, will be incremented witch each sent LoRa/WiFi packet
                                                        /// the first two bits in the byte will be the protocol id (CW_TRX_PROTO_VERSION)


///////////// Variable for WiFiTrx

IPAddress peerIP;

////////////////////////////////////////////////////////////////////
// encoder subroutines
/// interrupt service routine - needs to be positioned BEFORE all other functions, including setup() and loop()
/// interrupt service routine

void IRAM_ATTR isr ()  {                    // Interrupt service routine is executed when a HIGH to LOW transition is detected on CLK
//if (micros()  > (IRTime + 1000) ) {
portENTER_CRITICAL_ISR(&mux);

    int sig2 = digitalRead(PinDT); //MSB = most significant bit
    int sig1 = digitalRead(PinCLK); //LSB = least significant bit
    delayMicroseconds(144);                 // this seems to improve the responsiveness of the encoder and avoid any bouncing

    int8_t thisState = sig1 | (sig2 << 1);
    if (_oldState != thisState) {
      stateRegister = (stateRegister << 2) | thisState;
      if (thisState == LATCHSTATE) {
        
          if (stateRegister == 135 )
            encoderPos = 1;
          else if (stateRegister == 75)
            encoderPos = -1;
          else
            encoderPos = 0;
        }
    _oldState = thisState;
    } 
portEXIT_CRITICAL_ISR(&mux);
}



int IRAM_ATTR checkEncoder() {
  int t;
  
  portENTER_CRITICAL(&mux);

  t = encoderPos;
  if (encoderPos) {
    encoderPos = 0;
    portEXIT_CRITICAL(&mux);
    return t;
  } else {
    portEXIT_CRITICAL(&mux);
    return 0;
  }
}

//////////////////////// Function for generating DEBUG and ERROR messages on USB, ONLY IF USB is not used for outputting characters
//////////////////////// Argumenbt to DEBUG has to be a String object

void DEBUG (String s) {
  if (!MorsePreferences::serialOut || IGNORE_SERIALOUT) 
    Serial.println(s);
}

////////////////////////   S E T U P /////////////////////////////

void setup()
{
   //// the order of the following steps is important:
   //// 1. determine board version
   //// 2. configure batteryPin and modeButtonPin (clickbutton), depending on board version
   //// 3. read preferences form NVS
   //// 4. enable Vext (a must for board v3)
   //// 5. measure battery voltage (cannot do this later...)
   //// 6. initialoze display, configure interrupts for encoder
   //// 7. check for press of key/paddle at start, to initiate hw config
   //// 8. do the remaining initialisations

  Serial.begin(115200);
  delay(10); // give me time to bring up serial monitor


  MorsePreferences::determineBoardVersion();
  // now set pins according to board version
  if (MorsePreferences::boardVersion == 3) {
    batteryPin = 13;
    leftPin = 33;
    rightPin = 32;
  } else {        // must be board version 4
    batteryPin = 37;
    Buttons::modeButton.activeHigh = HIGH;      // in contrast to board v.3, in v4. the active state is HIGH not LOW
    leftPin = 32;
    rightPin = 33;
  }


// read preferences from non-volatile storage
// if version cannot be read, we have a new ESP32 and need to write the preferences first

  MorsePreferences::readPreferences("morserino");
  koch.setup(); 

 
   // enable Vext
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext,LOW);
  
  //DEBUG("Vext ON");
 

  // measure battery voltage, then set pinMode (important for board 4, as the same pin is used for battery measurement
  volt = batteryVoltage();
  pinMode(modeButtonPin, INPUT);

 //DEBUG("Volt: " + String(volt));

  // set up the encoder - we need external pull-ups as the pins used do not have built-in pull-ups!
  pinMode(PinCLK,INPUT_PULLUP);
  pinMode(PinDT,INPUT_PULLUP);  
  pinMode(keyerPin, OUTPUT);        // we can use the built-in LED to show when the transmitter is being keyed
  pinMode(leftPin, INPUT);          // external keyer left paddle
  pinMode(rightPin, INPUT);         // external keyer right paddle

  /// enable deep sleep
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, (esp_sleep_ext1_wakeup_mode_t) 0); //1 = High, 0 = Low
  analogSetAttenuation(ADC_0db);


 // init display, LoRa, Serial
  Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Enable*/, true /*Serial Enable*/, true /*LoRa use PABOOST*/, BAND /*LoRa RF working band*/);
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Init...pse wait...");   /// gives us something to watch while SPIFFS is created at very first start
  MorseOutput::soundSetup();

  //call ISR when any high/low changed seen
  //on any of the enoder pins
  attachInterrupt (digitalPinToInterrupt(PinDT), isr, CHANGE);   
  attachInterrupt (digitalPinToInterrupt(PinCLK), isr, CHANGE);
 
  encoderPos = 0;           /// this is the encoder position

/// set up for encoder button
//  pinMode(modeButtonPin, INPUT);
  pinMode(volButtonPin, INPUT_PULLUP);               // external pullup for all GPIOS > 32 with ESP32-LORA
  pinMode(modeButtonPin, INPUT);
                                                     // wake up also works without external pullup! Interesting!
  
  // Setup button timers (all in milliseconds / ms)
  // (These are default if not set, but changeable for convenience)
  
  Buttons::modeButton.debounceTime   = 11;   // Debounce timer in ms
  Buttons::modeButton.multiclickTime = 220;  // Time limit for multi clicks
  Buttons::modeButton.longClickTime  = 350; // time until "held-down clicks" register

  Buttons::volButton.debounceTime   = 11;   // Debounce timer in ms
  Buttons::volButton.multiclickTime = 220;  // Time limit for multi clicks
  Buttons::volButton.longClickTime  = 350; // time until "held-down clicks" register

  /// check if a key has been pressed on startup - if yes, we have to perform Hardware Configuration

  if (SPIFFS.begin(false))  {                     // while the SPIFFS has not been initialized (i.e. 1st programming), we are not going to check the key press
      if (key_was_pressed_at_start()) {
         MorsePreferences::displayKeyerPreferencesMenu(MorsePreferences::posHwConf);
         MorsePreferences::adjustKeyerPreference(MorsePreferences::posHwConf);
    
         switch (hwConf) {
            case 1: 
                    MorsePreferences::calibrateVoltageMeasurement();
                    break;
            case 2: MorsePreferences::loraSystemSetup();
                    break;
            default: break;
         }
      }
  }
  analogSetAttenuation(ADC_0db);
  adcAttachPin(audioInPin);

  // to calibrate sensors, we record the values in untouched state; need to do this after checking for system config
  initSensors();



  /// set up quickstart - this should only be done once at startup - after successful quickstart we disable it to allow normal menu operation
  quickStart = MorsePreferences::quickStart;


////////////  Setup for LoRa

  LoRa.setFrequency(MorsePreferences::loraQRG+0000);                       /// default = 434.150 MHz - Region 1 ISM Band, can be changed by system setup
  LoRa.setSpreadingFactor(7);                         /// default
  LoRa.setSignalBandwidth(250E3);                     /// 250 kHz
  LoRa.noCrc();                                       /// we use error correction
  LoRa.setSyncWord(MorsePreferences::loraSyncW);                      /// the default would be 0x34
  
  // register the receive callback
  LoRa.onReceive(onLoraReceive);
  /// initialise the serial number
  cwTxSerial = random(64);




  ///////////////////////// mount (or create) SPIFFS file system
    #define FORMAT_SPIFFS_IF_FAILED true

    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){     ///// if SPIFFS cannot be mounted, it does not exist. So create  (format) it, and mount it
        DEBUG("SPIFFS Mount Failed");
        return;
    }
  //////////////////////// create file player.txt if it does not exist|
  const char * defaultFile = "This is just an initial dummy file for the player. Dies ist nur die anfänglich enthaltene Standarddatei für den Player.\n"
                             "Did you not upload your own file? Hast du keine eigene Datei hochgeladen?";
                             
    if (!SPIFFS.exists("/player.txt")) {                                    // file does not exist, therefor we create it from the text above
        file = SPIFFS.open("/player.txt", FILE_WRITE);
        if(!file){
            DEBUG("- failed to open file for writing");
            return;
        }
        if(file.print(defaultFile)){
            ;
        } else {
            DEBUG("- write failed");
        }
        file.close();
    }    
    displayStartUp(volt);
  MorseMenu::menu_();
} /////////// END setup()


boolean key_was_pressed_at_start() {
     
      if (checkKey()) {
          MorseOutput::clearDisplay();   
          return true;
      }
      else return false;
}

boolean checkKey () {
      uint8_t sensor;
      uint8_t ext;
      sensor = readSensors(LEFT, RIGHT, true);
      ext = (uint8_t) !(digitalRead(leftPin) && digitalRead(rightPin));
      //DEBUG("Paddle: " + String(sensor) + " Ext.Key: " + String(ext));
      if (sensor || ext) 
        return true;
      else
        return false;
}

// display startup screen and check battery status

void displayStartUp(uint16_t volt) {

  String s = PROJECTNAME + String(" ");
  MorseOutput::clearDisplay();
  s += String(MorsePreferences::loraQRG / 10000);
  MorseOutput::printOnStatusLine( true, 0, s);
  //s = "Ver. " + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + (BETA ? " beta" : "");
  s = "V." + String(VERSION_MAJOR) + "." + String(VERSION_MINOR) ;
  if (VERSION_PATCH != 0)
    s = s + "." + String(VERSION_PATCH);
  if (BETA)
    s += " beta";
  MorseOutput::printOnScroll(0, REGULAR, 0, s );
  MorseOutput::printOnScroll(1, REGULAR, 0, "© 2018-2021");

  // uint16_t volt = batteryVoltage(); // has been measured early in setup()
  
  if (volt > 1000 && volt < 2800)
    MorseOutput::displayEmptyBattery(shutMeDown);
  else 
    MorseOutput::displayBatteryStatus(volt);
  delay(2000);
}



///////////////////////// THE MAIN LOOP - do this OFTEN! /////////////////////////////////

void loop() {
   int t;
   
   switch (keyerState) {
      case DIT:
      case DAH:
      case KEY_START: 
               break;
      default: checkPaddles();
               break;
   }
   
   switch (morseState) {
      case morseKeyer:    if (doPaddleIambic(leftKey, rightKey)) {
                              return;                                                        // we are busy keying and so need a very tight loop !
                          }
                          
                          break;
      case loraTrx:      
      case wifiTrx:       if (doPaddleIambic(leftKey, rightKey)) {
                              return;                                                        // we are busy keying and so need a very tight loop !
                          }
                          generateCW();
                          break;

      case morseTrx:      if (doPaddleIambic(leftKey, rightKey)) {
                              return;                                                        // we are busy keying and so need a very tight loop !
                          }  
                          audioDecoder.decode();
                          if (speedChanged) {
                            speedChanged = false;
                            displayCWspeed();
                          }
                          break;    
      case morseGenerator:  
                          if (MorsePreferences::autoStopMode) {
                              if ((autoStop == halt))   {                     // we check input
                                  if (leftKey) {
                                    autoStop = repeatword;
                                    delay(15);
                                  }
                                  else if (rightKey) {
                                    autoStop = nextword;
                                    delay(15);
                                  }
                                  else
                                    break;
                              // for debouncing:
                              while (checkPaddles() )
                                  ;                                                           // wait until paddles are released
                              active = true;
                              break;
                              } // end of squeeze
                          }                             /// end autostop
                          if (leftKey  || rightKey)   {
                              // for debouncing:
                              while (checkPaddles() )
                                ;
                              delay(15);
                              active = !active;
                              if (active)
                                  cleanStartSettings();
                              else {
                                  keyOut(false, true, 0, 0);
                                  MorseOutput::printOnStatusLine( true, 0, "Continue w/ Paddle");
                              }
                          } else {                  /// no paddle pressed - check stop flag
                              checkStopFlag();
                          }
                          if (active)
                            generateCW();
                          break;   
      case echoTrainer:                             ///// check stopFlag triggered by maxSequence
                          checkStopFlag();
                          if (!active && (leftKey  || rightKey))   {                       // touching a paddle starts  the generation of code
                              // for debouncing:
                              while (checkPaddles() )
                                  ;                                                           // wait until paddles are released
                              active = !active;
             
                              cleanStartSettings();
                          } /// end touch to start
                          if (active)
                          switch (echoTrainerState) {
                            case  START_ECHO:   
                            case  SEND_WORD:
                            case  REPEAT_WORD:  echoResponse = ""; generateCW();
                                                break;
                            case  EVAL_ANSWER:  echoTrainerEval();
                                                break;
                            case  COMPLETE_ANSWER:                    
                            case  GET_ANSWER:   if (doPaddleIambic(leftKey, rightKey)) 
                                                return;                             // we are busy keying and so need a very tight loop !
                                                break;
                            }                              
                            break;
      case morseDecoder: //DEBUG("case morseDecoder");
                         keyDecoder.decode();
                         audioDecoder.decode();
                         if (speedChanged) {
                            speedChanged = false;
                            displayCWspeed();
                          }
      default:            break;
            
                        
  } // end switch and code depending on state of metaMorserino

/// if we have time check for button presses

    Buttons::modeButton.Update();
    Buttons::volButton.Update();
    
    switch (Buttons::volButton.clicks) {
      case 1:   if (encoderState == scrollMode) {
                    if (morseState != morseDecoder)
                        encoderState = speedSettingMode;
                    else
                        encoderState = volumeSettingMode;
                    MorseOutput::relPos = MorseOutput::maxPos;
                    //MorseOutput::refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                    MorseOutput::displayScrollBar(false);
                } else if (encoderState == volumeSettingMode && morseState != morseDecoder) {          //  single click toggles encoder between speed and volume
                  encoderState = speedSettingMode;
                  MorsePreferences::writeVolume();
                  displayCWspeed();
                  MorseOutput::displayVolume(true, MorsePreferences::sidetoneVolume);                                     // sidetone volume
                }
                else {
                  encoderState = volumeSettingMode;
                  displayCWspeed();
                  MorseOutput::displayVolume(false, MorsePreferences::sidetoneVolume);                                     // sidetone volume
                }
                break;
      case -1:  if (encoderState == scrollMode) {
                    encoderState = (morseState == morseDecoder ? volumeSettingMode : speedSettingMode);
                    MorseOutput::relPos = MorseOutput::maxPos;
                    //MorseOutput::refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                    MorseOutput::displayScrollBar(false);
                }       
                else {
                    encoderState = scrollMode;
                    MorseOutput::displayScrollBar(true);
                }
                break;
    }
   
    switch (Buttons::modeButton.clicks) {                                // actions based on enocder button
       case -1:   MorseMenu::menu_();                                       // long click exits current mode and goes to top menu
                  return;
       case 1:    if (morseState == morseGenerator || morseState == echoTrainer) {//  start/stop in trainer modi, in others does nothing currently
                  active = !active;
                  if (!active) {
                        keyOut(false, true, 0, 0);
                        MorseOutput::printOnStatusLine( true, 0, "Continue w/ Paddle");
                  }
                  else {
                    cleanStartSettings();
                  }
                        
              }
              break;
       case 2:  MorsePreferences::setupPreferences(MorsePreferences::menuPtr);                               // double click shows the preferences menu (true would select a specific option only)
                MorseOutput::clearDisplay();                                 // restore display
                updateTopLine();
                if (morseState == morseGenerator || morseState == echoTrainer) 
                    stopFlag = true;                                  // we stop what we had been doing
                else
                    stopFlag = false;
                //startFirst = true;
                //firstTime = true;
     default: break;
    }
    
/// and we have time to check the encoder
     if ((t = checkEncoder())) {
        //DEBUG("t: " + String(t));
        MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);         /// click
        switch (encoderState) {
          case speedSettingMode:  
                                  changeSpeed(t);
                                  break;
          case volumeSettingMode: 
                                  MorsePreferences::sidetoneVolume += t+1;
                                  MorsePreferences::sidetoneVolume = constrain(MorsePreferences::sidetoneVolume, 1, 20) -1;
                                  //DEBUG(String(MorsePreferences::sidetoneVolume));
                                  MorseOutput::displayVolume(false, MorsePreferences::sidetoneVolume);      // sidetone volume;
                                  break;
          case scrollMode:
                                  if (t == 1 && MorseOutput::relPos < MorseOutput::maxPos ) {        // up = scroll towards bottom
                                    ++MorseOutput::relPos;
                                    //MorseOutput::refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                                  }
                                  else if (t == -1 && MorseOutput::relPos > 0) {
                                    --MorseOutput::relPos;
                                    //MorseOutput::refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                                  }
                                  //encoderPos = 0;
                                  //portEXIT_CRITICAL(&mux);
                                  MorseOutput::displayScrollBar(true);
                                  break;
          }
    } // encoder 
    checkShutDown(false);         // check for time out
    
}     /////////////////////// end of loop() /////////

void updateManualSpeed() {
  if (MorsePreferences::keyermode == STRAIGHTKEY && speedChanged) {
    speedChanged = false;
    displayCWspeed();
  }
}

void checkStopFlag() {
    if (stopFlag) {
      active = stopFlag = false;
      keyOut(false, true, 0, 0);
      if (morseState == echoTrainer) {
        MorseOutput::clearStatusLine();
        MorseOutput::printOnStatusLine( true, 0, String(errCounter) + " errs (" + String(wordCounter-2) + " wds)" );
        delay(5000);
      }
      wordCounter = 1; errCounter = 0;
      MorseOutput::printOnStatusLine( true, 0, "Continue w/ Paddle");
    }
}


void cleanStartSettings() {
    if (encoderState == scrollMode) {
        encoderState = speedSettingMode;
        MorseOutput::relPos = MorseOutput::maxPos;
    }
    clearText = "";
    CWword = "";
    echoTrainerState = START_ECHO;
    generatorState = KEY_UP; 
    keyerState = IDLE_STATE;
    interWordTimer = 4294967000;                 // almost the biggest possible unsigned long number :-) - do not output a space at the beginning
    genTimer = millis()-1;                       // we will be at end of KEY_DOWN when called the first time, so we can fetch a new word etc... 
    errCounter = wordCounter = 0;                // reset word and error counter for maxSequence
    startFirst = true;
    autoStop = nextword;                             // for autoStop mode
    keyOut(false, true, 0, 0);
    updateTopLine();
}

///////////////////
// we use the paddle for iambic keying
/////

boolean doPaddleIambic (boolean dit, boolean dah) {
  boolean paddleSwap;                      // temp storage if we need to swap left and right
  static long ktimer;                      // timer for current element (dit or dah)
  static long curtistimer;                 // timer for early paddle latch in Curtis mode B+
  static long latencytimer;                // timer for "muting" paddles for some time in state INTER_ELEMENT
  unsigned int pitch;

  if(MorsePreferences::keyermode == STRAIGHTKEY) {
    keyDecoder.decode();
    updateManualSpeed();
    return false;
  }
  if (!MorsePreferences::didah)   {              // swap left and right values if necessary!
      paddleSwap = dit; dit = dah; dah = paddleSwap; 
  }
      

  switch (keyerState) {                                         // this is the keyer state machine
     case IDLE_STATE:
         // display the interword space, if necessary
         if (millis() > interWordTimer) {
             if (morseState == loraTrx)    {                    // when in Trx mode
                 cwForTx(3);
                 sendWithLora();                        // finalise the string and send it to LoRA
             }
             else if (morseState == wifiTrx)    {                    // when in Trx mode
                 cwForTx(3);
                 sendWithWifi();                        // finalise the string and send it to WiFi
             }

             displayMorse(keyerTable.retrieveSymbol(), false);
             interWordTimer = 4294967000;                       // almost the biggest possible unsigned long number :-) - do not output extra spaces!
             if (echoTrainerState == COMPLETE_ANSWER)   {       // change the state of the trainer at end of word
                echoTrainerState = EVAL_ANSWER;
                return false;
             }
         }
        
       // Was there a paddle press?
        if (dit || dah ) {
            updatePaddleLatch(dit, dah);  // trigger the paddle latches
            if (morseState == echoTrainer)   {      // change the state of the trainer at end of word
                echoTrainerState = COMPLETE_ANSWER;
             }
            keyerTable.resetTable();
            if (dit) {
                setDITstate();          // set next state
                DIT_FIRST = true;          // first paddle pressed after IDLE was a DIT
            }
            else {
                setDAHstate();  
                DIT_FIRST = false;         // first paddle was a DAH
            }
        }
        else {
           if (echoTrainerState == GET_ANSWER && millis() > genTimer) {
            echoTrainerState = EVAL_ANSWER;
         } 
         return false;                // we return false if there was no paddle press in IDLE STATE - Arduino can do other tasks for a bit
        }
        break;

    case DIT:
    /// first we check that we have waited as defined by ACS settings
            if ( MorsePreferences::ACSlength > 0 && (millis() <= acsTimer))  // if we do automatic character spacing, and haven't waited for (3 or whatever) dits...
              break;
            clearPaddleLatches();                           // always clear the paddle latches at beginning of new element
            keyerControl |= DIT_LAST;                        // remember that we process a DIT

            ktimer = ditLength;                              // prime timer for dit
            switch ( MorsePreferences::keyermode ) {
              case IAMBICB:  curtistimer = 2 + (ditLength * MorsePreferences::curtisBDotTiming / 100);   
                             break;                         // enhanced Curtis mode B starts checking after some time
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = ditLength;        // no early paddle checking in Curtis mode A Ultimatic mode oder Non-squeeze
                             break;
            }
            keyerState = KEY_START;                          // set next state of state machine
            break;
            
    case DAH:
            if ( MorsePreferences::ACSlength > 0 && (millis() <= acsTimer))  // if we do automatic character spacing, and haven't waited for 3 dits...
              break;
            clearPaddleLatches();                          // clear the paddle latches
            keyerControl &= ~(DIT_LAST);                    // clear dit latch  - we are not processing a DIT
            
            ktimer = dahLength;
            switch (MorsePreferences::keyermode) {
              case IAMBICB:  curtistimer = 2 + (dahLength * MorsePreferences::curtisBTiming / 100);    // enhanced Curtis mode B starts checking after some time
                             break;
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = dahLength;        // no early paddle checking in Curtis mode A or Ultimatic mode
                             break;
            }
            keyerState = KEY_START;                          // set next state of state machine
            break;
     

      
    case KEY_START:
          // Assert key down, start timing, state shared for dit or dah
          pitch = MorseOutput::notes[MorsePreferences::sidetoneFreq];
          if ((morseState == echoTrainer || morseState == loraTrx || morseState == wifiTrx) && MorsePreferences::echoToneShift != 0) {
             pitch = (MorsePreferences::echoToneShift == 1 ? pitch * 18 / 17 : pitch * 17 / 18);        /// one half tone higher or lower, as set in parameters in echo trainer mode
          }

           keyOut(true, true, pitch, MorsePreferences::sidetoneVolume);
           ktimer += millis();                     // set ktimer to interval end time          
           curtistimer += millis();                // set curtistimer to curtis end time
           keyerState = KEYED;                     // next state
           break;
 
    case KEYED:
                                                   // Wait for timers to expire
           if (millis() >= ktimer) {                // are we at end of key down ?
               keyOut(false, true, 0, 0);
               ktimer = millis() + ditLength -1;    // inter-element time
               latencytimer = millis() + ((MorsePreferences::latency-1) * ditLength / 8);
               keyerState = INTER_ELEMENT;       // next state
            }
            else if (millis() >= curtistimer ) {     // in Curtis mode we check paddle as soon as Curtis time is off
                 if (keyerControl & DIT_LAST)       // last element was a dit
                    updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
                 else
                    updatePaddleLatch(dit, false);  
                 // updatePaddleLatch(dit, dah);       // but we remain in the same state until element time is off! 
            }
            break;
 
    case INTER_ELEMENT:
            //if ((p_keyermode != NONSQUEEZE) && (millis() < latencytimer)) {     // or should it be p_keyermode > 2 ? Latency for Ultimatic mode?
            if (millis() < latencytimer) {
              if (keyerControl & DIT_LAST)       // last element was a dit
                    updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
              else
                    updatePaddleLatch(dit, false);
            }
            else {
                updatePaddleLatch(dit, dah);          // latch paddle state while between elements       
                if (millis() >= ktimer) {               // at end of INTER-ELEMENT
                    switch(keyerControl) {
                          case 3:                                         // both paddles are latched
                          case 7: 
                                  switch (MorsePreferences::keyermode) {
                                      case STRAIGHTKEY: break;
                                      case NONSQUEEZE:  if (DIT_FIRST)                      // when first element was a DIT
                                                               setDITstate();            // next element is a DIT again
                                                        else                                // but when first element was a DAH
                                                               setDAHstate();            // the next element is a DAH again! 
                                                        break;
                                      case ULTIMATIC:   if (DIT_FIRST)                      // when first element was a DIT
                                                               setDAHstate();            // next element is a DAH
                                                        else                                // but when first element was a DAH
                                                               setDITstate();            // the next element is a DIT! 
                                                        break;
                                      default:          if (keyerControl & DIT_LAST)     // Iambic: last element was a dit - this is case 7, really
                                                            setDAHstate();               // next element will be a DAH
                                                        else                                // and this is case 3 - last element was a DAH
                                                            setDITstate();               // the next element is a DIT                         
                                   }
                                   break;
                                                                          // dit only is latched, regardless what was last element  
                          case 1:
                          case 5:  
                                   setDITstate();
                                   break;
                                                                          // dah only latched, regardless what was last element
                          case 2:
                          case 6:  
                                   setDAHstate();
                                   break;
                                                                          // none latched, regardless what was last element
                          case 0:
                          case 4:  
                                   keyerState = IDLE_STATE;               // we are at the end of the character and go back into IDLE STATE
                                   displayMorse(keyerTable.retrieveSymbol(), false);                        // display the decoded morse character(s)

                                   if (morseState == loraTrx || morseState == wifiTrx)
                                      cwForTx(0);

                                   ++charCounter;                         // count this character
                                   // if we have seen 12 chars since changing speed, we write the config to preferences (speed and left & right thresholds)
                                   if (charCounter == 12) {
                                        MorsePreferences::fireCharSeen(false);
                                   }
                                   if (MorsePreferences::ACSlength > 0)
                                        acsTimer = millis() + MorsePreferences::ACSlength * ditLength; // prime the ACS timer
                                   if (morseState == morseKeyer || morseState == loraTrx || morseState == wifiTrx || morseState == morseTrx)
                                      interWordTimer = millis() + 5*ditLength;
                                   else
                                       interWordTimer = millis() + interWordSpace;  // prime the timer to detect a space between characters
                                                                              // nominally 7 dit-lengths, but we are not quite so strict here in keyer or TrX mode,
                                                                              // use the extended time in echo trainer mode to allow longer space between characters, 
                                                                              // like in listening
                                   keyerControl = 0;                          // clear all latches completely before we go to IDLE
                          break;
                    } // switch keyerControl : evaluation of flags
                }
            } // end of INTER_ELEMENT
  } // end switch keyerState - end of state machine

  if (keyerControl & 3)                                               // any paddle latch?                            
    return true;                                                      // we return true - we processed a paddle press
  else
    return false;                                                     // when paddle latches are cleared, we return false
} /////////////////// end function doPaddleIambic()



//// this function checks the paddles (touch or external), returns true when a paddle has been activated, 
///// and sets the global variable leftKey and rightKey accordingly


boolean checkPaddles() {
  //long int timer = micros();
  static boolean oldL = false, newL, oldR = false, newR;
  int left, right;
  static long lTimer = 0, rTimer = 0;
  const int debDelay = 512;       // debounce time = 0,512  ms
  
  /* internal and external paddles are now working in parallel - the parameter p_useExtPaddle is used to indicate reverse polarity of external paddle
  */
  left = MorsePreferences::reversePolarity ? rightPin : leftPin;
  right = MorsePreferences::reversePolarity ? leftPin : rightPin;
  sensor = readSensors(LEFT, RIGHT, false);
  newL = (sensor >> 1);
  newR = (sensor & 0x01);
  
  if (MorsePreferences::keyermode != STRAIGHTKEY) {
      newL = newL | (!digitalRead(left)) ;
      newR = newR | (!digitalRead(right)) ;
  }
  
  if ((MorsePreferences::keyermode == NONSQUEEZE) && newL && newR) 
    return (leftKey || rightKey);

  if (newL != oldL)
      lTimer = micros();
  if (newR != oldR)
      rTimer = micros();
  if (micros() - lTimer > debDelay)
      if (newL != leftKey) 
          leftKey = newL;
  if (micros() - rTimer > debDelay)
      if (newR != rightKey) 
          rightKey = newR;       

  oldL = newL;
  oldR = newR;
  return (leftKey || rightKey);
}

///
/// Keyer subroutines
///

// update the paddle latches in keyerControl
void updatePaddleLatch(boolean dit, boolean dah)
{ 
    if (dit)
      keyerControl |= DIT_L;
    if (dah)
      keyerControl |= DAH_L;
}

// clear the paddle latches in keyer control
void clearPaddleLatches ()
{
    keyerControl &= ~(DIT_L + DAH_L);   // clear both paddle latch bits
}

// functions to set DIT and DAH keyer states
void setDITstate() {
  keyerState = DIT;
  keyerTable.recordDit();
  if (morseState == loraTrx || morseState == wifiTrx)
      cwForTx(1);                         // build compressed string for LoRa & Wifi
}

void setDAHstate() {
  keyerState = DAH;
  keyerTable.recordDah();
  if (morseState == loraTrx || morseState == wifiTrx)
      cwForTx(2);
}


// toggle polarity of paddles
void togglePolarity () {
      MorsePreferences::didah = !MorsePreferences::didah; 
     //displayPolarity();
}
  

/// function to read sensors:
/// read both left and right twice, repeat reading if it returns 0
/// return a binary value, depending on a (adaptable?) threshold:
/// 0 = nothing touched,  1= right touched, 2 = left touched, 3 = both touched
/// binary:   00          01                10                11

uint8_t readSensors(int left, int right, boolean init) {
  //long int timer = micros();
  //static boolean first = true;
  uint8_t v, lValue, rValue;
  
  while ( !(v=touchRead(left)) )
    ;                                       // ignore readings with value 0
  lValue = v;
   while ( !(v=touchRead(right)) )
    ;                                       // ignore readings with value 0
  rValue = v;

  if (init == false) {
    if (lValue < (MorsePreferences::tLeft+10))     {           //adaptive calibration
        MorsePreferences::tLeft = ( 7*MorsePreferences::tLeft +  ((lValue+lUntouched) / SENS_FACTOR) ) / 8;
    }
    if (rValue < (MorsePreferences::tRight+10))     {           //adaptive calibration
        MorsePreferences::tRight = ( 7*MorsePreferences::tRight +  ((rValue+rUntouched) / SENS_FACTOR) ) / 8;
    }
  } else {
    lValue -=20; rValue -=20;
  }
  return ( lValue < MorsePreferences::tLeft ? 2 : 0 ) + (rValue < MorsePreferences::tRight ? 1 : 0 );
}


void initSensors() {
  int v;
  lUntouched = rUntouched = 60;       /// new: we seek minimum
  for (int i=0; i<8; ++i) {
      while ( !(v=touchRead(LEFT)) )
        ;                                       // ignore readings with value 0
        lUntouched += v;
        //lUntouched = _min(lUntouched, v);
       while ( !(v=touchRead(RIGHT)) )
        ;                                       // ignore readings with value 0
        rUntouched += v;
        //rUntouched = _min(rUntouched, v);
  }
  lUntouched /= 8;
  rUntouched /= 8;
  MorsePreferences::tLeft = lUntouched - 9;
  MorsePreferences::tRight = rUntouched - 9;
}


String getRandomWord( int maxLength) {        //// give me a random English word, max maxLength chars long (1-5) - 0 returns any length
  if (maxLength > 6)
    maxLength = 0;
    if (kochActive)
        return koch.getRandomWord(); 
    else 
        return EnglishWords::words[random(EnglishWords::WORDS_POINTER[maxLength], EnglishWords::WORDS_NUMBER_OF_ELEMENTS)];
}

String getRandomAbbrev( int maxLength) {        //// give me a random CW abbreviation , max maxLength chars long (1-5) - 0 returns any length
  if (maxLength > 6)
    maxLength = 0;
    if (kochActive)
        return koch.getRandomAbbrev();
    else
        return Abbrev::abbreviations[random(Abbrev::ABBREV_POINTER[maxLength], Abbrev::ABBREV_NUMBER_OF_ELEMENTS)];  
}

// we use substrings as char pool for trainer mode
  // SANK will be replaced by <as>, <ka>, <kn> and <sk> (the 2nd letter is the key)
  // Options:
  //    0: a9?<> = CWchars; all of them; same as Koch 45+
  //    1: a = CWchars.substring(0,26);
  //    2: 9 = CWchars.substring(26,36);
  //    3: ? = CWchars.substring(36,45);
  //    4: <> = CWchars.substring(44,50);
  //    5: a9 = CWchars.substring(0,36);
  //    6: 9? = CWchars.substring(26,45);
  //    7: ?<> = CWchars.substring(36,50);
  //    8: a9? = CWchars.substring(0,45); 
  //    9: 9?<> = CWchars.substring(26,50);
  //  {OPT_ALL, OPT_ALPHA, OPT_NUM, OPT_PUNCT, OPT_PRO, OPT_ALNUM, OPT_NUMPUNCT, OPT_PUNCTPRO, OPT_ALNUMPUNCT, OPT_NUMPUNCTPRO}

String getRandomChars(int maxLength, int option) {             /// random char string, eg. group of 5, 9 differing character pools; maxLength = 1-6
    String result = ""; 
    result.reserve(7);
    int s = 0, e = 50;
    int i;
  
    if (maxLength > 6) {                                        // we use a random length!
      maxLength = random(2, maxLength - 3);                     // maxLength is max 10, so random upper limit is 7, means max 6 chars...
    }
    if (kochActive) {
      if (option == OPT_KOCH_ADAPTIVE)
        return koch.getAdaptiveChar(maxLength);
      else
        return koch.getRandomChar(maxLength);
    } else {
         switch (option) {
          case OPT_NUM: 
          case OPT_NUMPUNCT: 
          case OPT_NUMPUNCTPRO: 
                                s = 26; break;
          case OPT_PUNCT: 
          case OPT_PUNCTPRO: 
                                s = 36; break;
          case OPT_PRO: 
                                s = 44; break;
          default:              s = 0;  break;
        }
        switch (option) {
          case OPT_ALPHA: 
                                e = 26;  break;
          case OPT_ALNUM: 
          case OPT_NUM: 
                                e = 36; break;
          case OPT_ALNUMPUNCT: 
          case OPT_NUMPUNCT:
          case OPT_PUNCT: 
                                e = 45; break;
          default:              e = 50; break;
        }

        for (i = 0; i < maxLength; ++i) 
          result += CWchars.charAt(random(s,e));
  }
  return result;
}


String getRandomCall(int maxLength) {            // random call-sign like pattern, maxLength = 3 - 6, 0 returns any length
  const byte prefixType[] = {1,0,1,2,3,1};         // 0 = a, 1 = aa, 2 = a9, 3 = 9a
  byte prefix;
  String call = "";
  unsigned int l = 0;
  //int s, e;

  if (maxLength == 1 || maxLength == 2)
      maxLength = 3;
  if (maxLength > 6)
      maxLength = 6;

  if (maxLength == 3)
      prefix = 0;
  else  
      prefix = prefixType[random(0,6)];           // what type of prefix?
  switch (prefix) {
      case 1: call += CWchars.charAt(random(0,26));
              ++l;
      case 0: call += CWchars.charAt(random(0,26));
              ++l;
              break;
      case 2: call += CWchars.charAt(random(0,26));
              call += CWchars.charAt(random(26,36));
              l = 2;
              break;
      case 3: call += CWchars.charAt(random(26,36));
              call += CWchars.charAt(random(0,26));
              l = 2;
              break;
    } // we have a prefix by now; l is its length
      // now generate a number
    call += CWchars.charAt(random(26,36));
    ++l;
    // generate a suffix, 1 2 or 3 chars long - we re-use prefix for the suffix length
    if (maxLength == 3)
        prefix = 1;
    else if (maxLength == 0) {
        prefix = random(1,4);
        prefix = (prefix == 2 ? prefix :  random(1,4)); // increase the likelihood for suffixes of length 2
    }
    else {
        prefix = _min(maxLength - l, 3);                 // we try to always give the desired length, but never more than 3 suffix chars
    }
    while (prefix--) {
      call += CWchars.charAt(random(0,26));
      ++l;
    } // now we have the suffix as well
    // are we /p or /m? - we do this only in rare cases - 1 out of 9, and only when maxLength = 0, or maxLength-l >= 2
    if (maxLength == 0 ) //|| maxLength - l >= 2)
      if (! random(0,8)) {
      call += "/";
      call += ( random(0,2) ? "m" : "p" );
    }
    // we have a complete call sign!
    return call;
}


/////// generate CW representations from its input string 
/////// CWchars = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKEäöüH";

String generateCWword(String symbols) {
  int pointer;
  byte bitMask, NoE;
  //byte nextElement[8];      // the list of elements; 0 = dit, 1 = dah
  String result = "";
  
  int l = symbols.length();
  
  for (int i = 0; i<l; ++i) {
    char c = symbols.charAt(i);                                 // next char in string
    pointer = CWchars.indexOf(c);                               // at which position is the character in CWchars?
    NoE = pool[pointer][1];                                     // how many elements in this morse code symbol?
    bitMask = pool[pointer][0];                                 // bitMask indicates which of the elements are dots and which are dashes
    for (int j=0; j<NoE; ++j) {
        result += (bitMask & B10000000 ? "2" : "1" );         // get MSB and store it in string - 2 is dah, 1 is dit, 0 = inter-element space
        bitMask = bitMask << 1;                               // shift bitmask 1 bit to the left 
        //DEBUG("Bitmask: ");
        //DEBUG(String(bitmask, BIN));
      } /// now we are at the end of one character, therefore we add enough space for inter-character
      result += "0";
  }     /// now we are at the end of the word, therefore we remove the final 0!
  result.remove(result.length()-1);
  return result;
}

void printOnDisplay(FONT_ATTRIB style, String s) {
   MorseOutput::printToScroll(style, s, true, encoderState == scrollMode);
   if (MorsePreferences::serialOut == 7)
      Serial.print(s);
}


void generateCW () {          // this is called from loop() (frequently!)  and generates CW
  
  static int l;
  static char c;
  boolean silentEcho;
  
  switch (generatorState) {                                             // CW generator state machine - key is up or down
    case KEY_UP:
            if (millis() < genTimer)                                    // not yet at end of the pause: just wait
                return;                                                 // therefore we return to loop()
             // here we continue if the pause has been long enough
            if (startFirst == true)
                CWword = "";
            l = CWword.length();
            
            if (l==0)  {                                               // fetch a new word if we have an empty word
                if (clearText.length() > 0) {                          // this should not be reached at all.... except when display word by word
                  //DEBUG("Text left: " + clearText);

                if (MorsePreferences::trainerDisplay == DISPLAY_BY_WORD &&
                    (morseState == loraTrx || morseState == wifiTrx || morseState == morseGenerator))
                  {
                      printOnDisplay(morseState == morseGenerator ? REGULAR : BOLD,cleanUpProSigns(clearText));
                      clearText = "";
                  }
                }
                fetchNewWord();
                //DEBUG("New Word: " + CWword);
                if (CWword.length() == 0)                         // we really should have something here - unless in trx mode or in a pause; in this case return
                  return;
                if ((morseState == echoTrainer)) {
                  printOnDisplay(REGULAR, "\n");
                }
            }
            c = CWword[0];                                            // retrieve next element from CWword; if 0, we were at end of character
            CWword.remove(0,1); 
            if (c == '0' || !CWword.length())  {                      // a character just had been finished //// is there an error here?
                   if (c == '0') {
                      c = CWword[0];                                  // retrieve next element from CWword;
                      CWword.remove(0,1); 
                      if (morseState == morseGenerator && MorsePreferences::loraTrainerMode >= 1)
                          cwForTx(0);                             // send end of character to transmit buffer
                      }
            }   /// at end of character

            //// insert code here for outputting only on display, and not as morse characters - for echo trainer
            //// genTimer vy short (1ms?)
            //// no keyOut()
            if (morseState == echoTrainer && MorsePreferences::echoDisplay == DISP_ONLY)
                genTimer = millis() + 2;      // very short timing
            else if (morseState != loraTrx && morseState != wifiTrx)
                genTimer = millis() + (c == '1' ? ditLength : dahLength);           // start a dit or a dah, acording to the next element
            else 
                genTimer = millis() + (c == '1' ? rxDitLength : rxDahLength);
            if (morseState == morseGenerator && MorsePreferences::loraTrainerMode >= 1)             // send the element to transmit buffer
                c == '1' ? cwForTx(1) : cwForTx(2) ; 
            /// if Koch learn character we show dit or dah
            if (generatorMode == KOCH_LEARN)
                printOnDisplay(REGULAR, c == '1' ? "."  : "-");
            silentEcho = (morseState == echoTrainer && MorsePreferences::echoDisplay == DISP_ONLY); // echo mode with no audible prompt

            if (silentEcho || stopFlag)                                             // we finished maxSequence and so do start output (otherwise we get a short click)
              ;
            else  {
                keyOut(true, (morseState != loraTrx && morseState != wifiTrx), pitch(), MorsePreferences::sidetoneVolume);
            }
            generatorState = KEY_DOWN;                              // next state = key down = dit or dah

            break;
    case KEY_DOWN:
            if (millis() < genTimer)                                // if not at end of key down we need to wait, so we just return to loop()
                return;
            //// otherwise we continue here; stop keying,  and determine the length of the following pause: inter Element, interCharacter or InterWord?

           keyOut(false, (morseState != loraTrx && morseState != wifiTrx), 0, 0);
            if (! CWword.length())   {                                 // we just ended the the word, ...  //// intercept here in Echo Trainer mode or autoStop mode
                if (morseState == morseGenerator) 
                    autoStop = MorsePreferences::autoStopMode ? halt : nextword;
                dispGeneratedChar();
                if (morseState == echoTrainer) {
                    switch (echoTrainerState) {
                        case START_ECHO:  echoTrainerState = SEND_WORD;
                                          genTimer = millis() + interCharacterSpace + (MorsePreferences::promptPause * interWordSpace);
                                          break;
                        case REPEAT_WORD:
                                          // fall through 
                        case SEND_WORD:   if (echoStop)
                                                break;
                                          else {
                                                echoTrainerState = GET_ANSWER;
                                                if (MorsePreferences::echoDisplay != CODE_ONLY) {
                                                    printOnDisplay(REGULAR, " ");
                                                    printOnDisplay(INVERSE_REGULAR, ">");    /// add a blank after the word on the display
                                                }
                                                ++repeats;
                                                genTimer = millis() + MorsePreferences::responsePause * interWordSpace;
                                          }
                        default:          break;
                    }
                }
                else { 
                      genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxInterWordSpace : interWordSpace) ;  // we need a pause for interWordSpace
                      if (morseState == morseGenerator && MorsePreferences::loraTrainerMode >= 1) {                                   // in generator mode and we want to send with LoRa
                          cwForTx(0);
                          cwForTx(3);                           // as we have just finished a word
                          if (MorsePreferences::loraTrainerMode == 1)
                            sendWithLora();                         // finalise the string and send it to LoRA
                          else sendWithWifi();                      // or WiFi
                          delay(interCharacterSpace+ditLength);             // we need a slightly longer pause otherwise the receiving end might fall too far behind...
                      } 
                }
             }
             else if ((c = CWword[0]) == '0') {                                                                        // we are at end of character
//              // display last character 
//              // genTimer small if in echo mode and no code!
                dispGeneratedChar(); 
                if (morseState == echoTrainer && MorsePreferences::echoDisplay == DISP_ONLY)
                    genTimer = millis() +1;
                else            
                    genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxInterCharacterSpace : interCharacterSpace);          // pause = intercharacter space
             }
             else  {                                                                                                   // we are in the middle of a character
                genTimer = millis() + ((morseState == loraTrx  || morseState == wifiTrx) ? rxDitLength : ditLength);                              // pause = interelement space
             }
             generatorState = KEY_UP;                               // next state = key up = pause
             break;         
  }   /// end switch (generatorState)
}

int pitch() {                 // find out which pitch to use for the generated CW tone
    int p = MorseOutput::notes[MorsePreferences::sidetoneFreq];
    if (alternatePitch && morseState == morseGenerator)
      p = (MorsePreferences::echoToneShift == 1 ? p * 18 / 17 : p * 17 / 18);
    return p;
}

/// when generating CW, we display the character (under certain circumstances)
/// add code to display in echo mode when parameter is so set
/// p_echoDisplay 1 = CODE_ONLY 2 = DISP_ONLY 3 = CODE_AND_DISP

void dispGeneratedChar() {
  static String charString;
  charString.reserve(10);
  
  if (generatorMode == KOCH_LEARN ||
          (MorsePreferences::trainerDisplay == DISPLAY_BY_CHAR &&
          (morseState == loraTrx || morseState == wifiTrx || morseState == morseGenerator)) ||
          (morseState == echoTrainer && MorsePreferences::echoDisplay != CODE_ONLY ))
      {       /// we need to output the character on the display now  
        if (clearText.charAt(0) == 0xC3) {            //UTF-8!
          charString = String(clearText.charAt(0)) + String(clearText.charAt(1));                   /// store first 2 chars of clearText in charString
          clearText.remove(0,2);                                                    /// and remove them from clearText
        }
        else {
          charString = clearText.charAt(0);
          clearText.remove(0,1);
        }
        if (generatorMode == KOCH_LEARN) {
            printOnDisplay(REGULAR," ");                      // output a space
            MorseOutput::clearBuffer();                      // clear the buffer first
        }
        printOnDisplay((morseState == loraTrx || morseState == wifiTrx || generatorMode == KOCH_LEARN) ? BOLD : REGULAR, cleanUpProSigns(charString));
        if (generatorMode == KOCH_LEARN)
            printOnDisplay(REGULAR," ");                      // output a space
      }   //// end display_by_char
      
      ++charCounter;                         // count this character
     
     // if we have seen 12 chars since changing speed, we write the config to Preferences
     if (charCounter == 12) {
        MorsePreferences::fireCharSeen(true);
     }
}

void fetchNewWord() {
  int rssi, rxWpm, rv;
  char numBuffer[16];                // for number to string conversion with sprintf()


    if (morseState == loraTrx || morseState == wifiTrx) {       // we check the rxBuffer and see if we received something
       MorseOutput::updateSMeter(0);                                         // at end of word we set S-meter to 0 until we receive something again
       startFirst = false;
       ////// from here: retrieve next CWword from buffer!
        if (cwBuReady()) {
            printOnDisplay(BOLD, " ");
            uint8_t header = decodePacket(&rssi, &rxWpm, &CWword);
            
            if ((header >> 6) != 1)                             // invalid protocol version
              return;
            if ((rxWpm < 5) || (rxWpm >60))                    // invalid speed
              return;
            clearText = CWwordToClearText(CWword);            
            rxDitLength = 1200 /   rxWpm ;                      // set new value for length of dits and dahs and other timings
            rxDahLength = 3* rxDitLength ;                      // calculate the other timing values
            rxInterCharacterSpace = 3 * rxDitLength;
            rxInterWordSpace = 7 * rxDitLength;
            sprintf(numBuffer, "%2ir", rxWpm);
            MorseOutput::printOnStatusLine( true, 4,  numBuffer); 
            MorseOutput::printOnStatusLine( true, 9, "s");
            MorseOutput::updateSMeter(rssi);                                 // indicate signal strength of new packet
       }
       else return;                                             // we did not receive anything
               
    } // end if loraTrx or wifiTrx
    else {
    if ((morseState == morseGenerator) /*&& !MorsePreferences::autoStopMode*/ ) {
        printOnDisplay(REGULAR, " ");    /// in any case, add a blank after the word on the display
    }
    
    if (generatorMode == KOCH_LEARN) {
        startFirst = false;
        echoTrainerState = SEND_WORD;
    }
    if (startFirst == true)  {                                 /// do the intial sequence in trainer mode, too
        clearText = "vvvA";
        startFirst = false;
    } else if (morseState == morseGenerator && MorsePreferences::wordDoubler == true && firstTime == false) {
        clearText = echoTrainerWord;
        firstTime = true;
    } else if (morseState == morseGenerator && autoStop == repeatword) {
        clearText = echoTrainerWord;
        autoStop = nextword;
    } else if (morseState == echoTrainer) {
        interWordTimer = 4294967000;                   /// interword timer should not trigger something now
        //DEBUG("echoTrainerState: " + String(echoTrainerState));
        switch (echoTrainerState) {
            case  REPEAT_WORD:  if (MorsePreferences::echoRepeats == 7 || repeats <= MorsePreferences::echoRepeats) 
                                    clearText = echoTrainerWord;
                                else {
                                    clearText = echoTrainerWord;
                                    if (generatorMode != KOCH_LEARN) {
                                        printOnDisplay(INVERSE_REGULAR, cleanUpProSigns(clearText));    //// clean up first!
                                        printOnDisplay(REGULAR, " ");
                                    }
                                    goto randomGenerate;
                                }
                                break;
            //case  START_ECHO:
            case  SEND_WORD:    goto randomGenerate;
            default:            break;
        }   /// end special cases for echo Trainer
    } else {   
   
      randomGenerate:       repeats = 0;
                            clearText = "";
                            if ((MorsePreferences::maxSequence != 0) && (generatorMode != KOCH_LEARN))
                              if ( morseState == echoTrainer || ((morseState == morseGenerator) && !MorsePreferences::autoStopMode) ) {
                                // a case for maxSequence - no maxSequence in autostop mode                         
                                ++ wordCounter;                                                               // 
                                int limit = 1 + MorsePreferences::maxSequence;
                                if (wordCounter == limit) {
                                  clearText = "+";
                                  echoStop = true;
                                  if (echoTrainerState == REPEAT_WORD)
                                    echoTrainerState = SEND_WORD;
                                }
                                else if (wordCounter == (limit+1)) {
                                    stopFlag = true;
                                    echoStop = false;
                                    //wordCounter = 1;
                                }
                            }
                            if (clearText != "+") {
                                switch (generatorMode) {
                                      case  RANDOMS:  clearText = getRandomChars(MorsePreferences::randomLength, MorsePreferences::randomOption);
                                                      break;
                                      case  CALLS:    clearText = getRandomCall(MorsePreferences::callLength);
                                                      break;
                                      case  ABBREVS:  clearText = getRandomAbbrev(MorsePreferences::abbrevLength);
                                                      break;
                                      case  WORDS:    clearText = getRandomWord(MorsePreferences::wordLength);
                                                      break;
                                      case  KOCH_LEARN:clearText = koch.getNewChar();
                                                      break;
                                      case  MIXED:    rv = random(4);
                                                      switch (rv) {
                                                        case  0:  clearText = getRandomWord(MorsePreferences::wordLength);
                                                                  break;
                                                        case  1:  clearText = getRandomAbbrev(MorsePreferences::abbrevLength);
                                                                    break;
                                                        case  2:  clearText = getRandomCall(MorsePreferences::callLength);
                                                                  break;
                                                        case  3:  clearText = getRandomChars(1,OPT_PUNCTPRO);        // just a single pro-sign or interpunct
                                                                  break;
                                                      }
                                                      break;
                                      case KOCH_MIXED:rv = random(3);
                                                      switch (rv) {
                                                        case  0:  clearText = getRandomWord(MorsePreferences::wordLength);
                                                                  break;
                                                        case  1:  clearText = getRandomAbbrev(MorsePreferences::abbrevLength);
                                                                    break;
                                                        case  2:  clearText = getRandomChars(MorsePreferences::randomLength, OPT_KOCH);        // Koch option!
                                                                  break;
                                                      }
                                                      break;
                                      case KOCH_ADAPTIVE:clearText = getRandomChars(MorsePreferences::randomLength, OPT_KOCH_ADAPTIVE);
                                                      break;
                                      case PLAYER:    if (MorsePreferences::randomFile)
                                                          skipWords(random(MorsePreferences::randomFile+1));
                                                      clearText = getWord(); 
                                                      clearText = cleanUpText(clearText);
                                                      break;  
                                    }   // end switch (generatorMode)
                            }
                            firstTime = false;
      }       /// end if else - we either already had something in trainer mode, or we got a new word
      if (clearText.indexOf('P') != -1) {
        genTimer = 3 * interWordSpace + millis();
        clearText = "";
        return;
      }
      if (clearText.indexOf('T') != -1) {
        alternatePitch = !alternatePitch;
        clearText = "";
        return;
      }
      CWword = generateCWword(clearText);
      echoTrainerWord = clearText;
    } /// else (= not in loraTrx or wifiTrx mode)
} // end of fetchNewWord()



/// display decoded morse code (and store it in echoTrainer)
void displayMorse(String symbol, boolean decoded) {
  uint8_t toBePrinted = 0;

  if (MorsePreferences::serialOut == 7)
    toBePrinted = MorsePreferences::serialOut & (decoded ? 2 : 1);
  else if (MorsePreferences::serialOut && (morseState == morseKeyer || morseState == morseTrx || morseState == morseDecoder))
    toBePrinted = true;
  if (toBePrinted)
    Serial.print(symbol);
  // output  the symbol; flush if not morseGenerator, otherwise if not autostop
  MorseOutput::printToScroll( REGULAR, symbol, true, encoderState == scrollMode);
  if (morseState == echoTrainer) {                /// store the character in the response string
    symbol.replace("<as>", "S");                  // maybe we need it for echo trainer or for autstop mode
    symbol.replace("<ka>", "A");
    symbol.replace("<kn>", "N");
    symbol.replace("<sk>", "K");
    symbol.replace("<ve>", "E");
    symbol.replace("<ch>", "H");
    symbol.replace("<err>", "R");                 // error (<hh>) - to be used to repeat entry in echo trainer mode
    if (symbol != " ")
      echoResponse.concat(symbol);
  }
}   /// end of displayMorse()




//////// Display the current CW speed
/////// pos 7-8, "Wpm" on 10-12

void displayCWspeed() {
  char numBuf[16];                // for number to string conversion with sprintf()
  uint8_t wpm;
  
  wpm = lastWasKey ? keyDecoder.getWpm() : audioDecoder.getWpm();
  if ((morseState ==  echoTrainer && MorsePreferences::keyermode == STRAIGHTKEY))
    sprintf(numBuf, "(%2i)", wpm);
  else if (( morseState == morseGenerator || morseState ==  echoTrainer ))
    sprintf(numBuf, "(%2i)", effWpm);
  else if (morseState == morseTrx )
    sprintf(numBuf, "r%2is", audioDecoder.getWpm());
  else sprintf(numBuf, "    ");

  MorseOutput::printOnStatusLine(false, 3,  numBuf);                                         // effective wpm or rxwpm
  
  if (MorsePreferences::keyermode == STRAIGHTKEY && 
      (morseState == morseKeyer || morseState == loraTrx || morseState == wifiTrx ))
        sprintf(numBuf, "%2i", wpm);  
  else
    sprintf(numBuf, "%2i", (morseState == morseDecoder ? wpm : MorsePreferences::wpm));         // d_wpm (decode) or p_wpm (default)
  MorseOutput::printOnStatusLine(encoderState == speedSettingMode ? true : false, 7,  numBuf);
  MorseOutput::printOnStatusLine(false, 10,  "WpM");
  Heltec.display -> display();
}


void updateTopLine() {
  String symbol;
  MorseOutput::clearStatusLine();

  if (morseState == morseGenerator) {
    if (MorsePreferences::wordDoubler)
      symbol = "x2 ";
    else if (MorsePreferences::autoStopMode)
      symbol = "<> ";
    else
      symbol = "   ";
    MorseOutput::printOnStatusLine(true, 1,  symbol);
  }
  else
    MorseOutput::printOnStatusLine(false, 2,  getKeyerModeSymbol() + " ");

  displayCWspeed();                                     // update display of CW speed
  if ((morseState == loraTrx ) || (morseState == morseGenerator  && MorsePreferences::loraTrainerMode == 1))
    MorseOutput::dispLoraLogo();
  else if ((morseState == wifiTrx) || (morseState == morseGenerator  && MorsePreferences::loraTrainerMode == 2))
      MorseOutput::dispWifiLogo();

  MorseOutput::displayVolume(encoderState == speedSettingMode, MorsePreferences::sidetoneVolume);                                     // sidetone volume
  Heltec.display -> display();
}


String getKeyerModeSymbol() {             /// symbol to be displayed on status line
  const char symbols[] = " ABUNS";        /// start with blank as we have no keyer mode 0!
  return String(symbols[MorsePreferences::keyermode]);
}

///////// evaluate the response in Echo Trainer Mode
void echoTrainerEval() {
    int i;
    delay(interCharacterSpace / 2);
    if (echoResponse.endsWith("R")) {
      echoResponse = "";
      echoTrainerState = COMPLETE_ANSWER;
      return;
    } else 
    if ((i = echoResponse.indexOf("R")) != -1) {
      echoResponse = echoResponse.substring(i+1);
    }

    if (echoResponse == echoTrainerWord) {
      echoTrainerState = SEND_WORD;
      //printToScroll(BOLD,  "OK");
      printOnDisplay(BOLD,  "OK");
      if (MorsePreferences::echoConf) {
          MorseOutput::soundSignalOK();
      }
      if (kochActive){
        koch.decreaseWordProbability(echoTrainerWord);
      }
      delay(interWordSpace);
      if (MorsePreferences::speedAdapt)
          changeSpeed(1);
    } else {
      echoTrainerState = REPEAT_WORD;
      if (generatorMode != KOCH_LEARN || echoResponse != "") {
          ++errCounter;
          printOnDisplay(BOLD, "ERR");
          if (MorsePreferences::echoConf) {
              MorseOutput::soundSignalERR();
          }
          if (kochActive){
            koch.increaseWordProbability(echoTrainerWord, echoResponse);
          }
      }
      delay(interWordSpace);
      if (MorsePreferences::speedAdapt)
          changeSpeed(-1);
    }
    echoResponse = "";
    clearPaddleLatches();
}   // end of function


void updateTimings() {
  ditLength = 1200 / MorsePreferences::wpm;                    // set new value for length of dits and dahs and other timings
  dahLength = 3 * ditLength;
  interCharacterSpace =  MorsePreferences::interCharSpace *  ditLength;
  interWordSpace = _max(MorsePreferences::interWordSpace, MorsePreferences::interCharSpace+4) * ditLength;
  effWpm = 60000 / (31 * ditLength + 4 * interCharacterSpace + interWordSpace );  ///  effective wpm with lengthened spaces = Farnsworth speed
} 

void changeSpeed( int t) {
  MorsePreferences::wpm += t;
  MorsePreferences::wpm = constrain(MorsePreferences::wpm, 5, 60);
  updateTimings();
  displayCWspeed();                     // update display of CW speed
  charCounter = 0;                                    // reset character counter
}


void keyTransmitter(boolean noTx) {
  if (noTx )
      return;
   digitalWrite(keyerPin, HIGH);           // turn the LED on, key transmitter, or whatever
}

String cleanUpProSigns( String &input ) {
    /// clean up clearText   -   S <as>,  - A <ka> - N <kn> - K <sk> - H ch etc;
    input.replace("S", "<as>");
    input.replace("A", "<ka>");
    input.replace("N", "<kn>");
    input.replace("K", "<sk>");
    input.replace("E", "<ve>");
    input.replace("H", "ch");
    input.replace("R", "<err>");
    input.replace("U", "*");
    //DEBUG(input);
    return input;
}

//// measure battery voltage in mV

int16_t batteryVoltage() {      /// measure battery voltage and return result in milliVolts
      delay(64);
      double v= 0; int counts = 4;
      for (int i=0; i<counts   ; ++i) {
         v+= ReadVoltage(batteryPin);
         delay(8);
         //DEBUG(String(v,4));
      }
      v /= counts;
      if (MorsePreferences::boardVersion == 4)      // adjust measurement for board version 4
        v *= 1.7;
      voltage_raw = v;
      v *= (MorsePreferences::vAdjust * 12.9);      // adjust measurement and convert to millivolts
      return (int16_t) v;                                                                                       
}


double ReadVoltage(byte pin){
  adcAttachPin(batteryPin);
  analogSetClockDiv(128);           //  this value was found by experimenting - no clue what it really does :-(
  analogSetPinAttenuation(batteryPin,ADC_11db);
  double reading = analogRead(pin); // Reference voltage is 3v3 so maximum reading is 3v3 = 4095 in range 0 to 4095
  analogSetClockDiv(1); // 5ms

  //DEBUG("ReadVoltage:" + String(reading));
  if(reading < 4 || reading > 4092)     /// invalid measurement
    return 0;
  //return -0.000000000009824 * pow(reading,3) + 0.000000016557283 * pow(reading,2) + 0.000854596860691 * reading + 0.065440348345433;
  return -0.000000000000016 * pow(reading,4) + 0.000000000118171 * pow(reading,3)- 0.000000301211691 * pow(reading,2)+ 0.001109019271794 * reading + 0.034143524634089;
} // Added an improved polynomial, use either, comment out as required



void checkShutDown(boolean enforce) {       /// if enforce == true, we shut donw even if there was no time-out
  unsigned long timeOut;
  switch (MorsePreferences::timeOut) {
      case 4:   timeOut = ULONG_MAX;
                break;
      default:  timeOut = 300000UL * MorsePreferences::timeOut;
                break;
  }
  
  if ((millis() - MorseOutput::TOTcounter) > timeOut || enforce == true )  {            
      MorseOutput::clearDisplay();
      MorseOutput::printOnScroll(1, INVERSE_BOLD, 0,  "Power OFF...");
      MorseOutput::printOnScroll(2, REGULAR, 0, "RED to turn ON");
      Heltec.display -> display();
      delay (1500);
      shutMeDown();
  }  
}

void shutMeDown() {
  MorseOutput::sleep();
  //Heltec.display -> sleep();                //OLED sleep
  LoRa.sleep();                   //LORA sleep
  //WiFi.mode( WIFI_MODE_NULL );    // switch off WiFi
  WiFi.disconnect(true, false);
  delay(50);
  digitalWrite(Vext,HIGH);
  delay(50);
  esp_deep_sleep_start();         // go to deep sleep
}




/// cwForTx packs element info (dit, dah, interelement space) into a String that can be sent via LoRA
///  element can be:
///  0: inter-element space
///  1: dit
///  2: dah
///  3: end of word -: cwForTx returns a string that is ready for sending to the LoRa transceiver

void cwForTx (int element) {
  static int pairCounter = 0;
  uint8_t temp;
  uint8_t header;
  uint8_t wpm;

  if (pairCounter == 0) {    // we start a brand new word for LoRA/Wifi - clear buffer and set speed first
      for (int i=0; i<sizeof(cwTxBuffer); ++i)
          cwTxBuffer[i] = (char) 0;
      /// 1st byte: version + serial number
      header = ++cwTxSerial % 64;
      //DEBUG("cwTxSerial: " + String(cwTxSerial));
      header += CW_TRX_PROTO_VERSION * 64;        //// shift VERSION left 6 bits and add to the serial number
      cwTxBuffer[0] = header;
      //wpm = MorsePreferences::keyermode == STRAIGHTKEY  ? keyDecoder.getWpm() : MorsePreferences::wpm;
      wpm = (MorsePreferences::keyermode == STRAIGHTKEY && morseState != morseGenerator) ? keyDecoder.getWpm() : MorsePreferences::wpm;
      temp = wpm * 4;                   /// shift left 2 bits
      cwTxBuffer[1] |= temp;
      pairCounter = 7;                    /// so far we have used 7 bit pairs: 4 in the first byte (protocol version+serial); 3 in the 2nd byte (wpm)
      } 
      else if (pairCounter > sizeof(cwTxBuffer)*4-1) { // prevent buffer overflow
          return;
      }

  temp = element & B011;      /// take the two right bits

  if (temp && (temp != 3)) {                 /// no need to do the operation with 0, nor with B11
      temp = temp << (2*(3-(pairCounter % 4)));
      cwTxBuffer[pairCounter/4] |= temp;
  }

  /// now increment, unless we got end of word
  /// have we get end of word, we got end of character (0) before

  if (temp != 3)
      ++pairCounter;
  else {  
      --pairCounter; /// we are at end of word and step back to end of character
      if (pairCounter % 4 != 0)      {           // do nothing if we have a zero in the topmost two bits already, as this was end of character
          temp = temp << (2*(3-(pairCounter % 4)));
          cwTxBuffer[pairCounter/4] |= temp;
      }
      pairCounter = 0;
  }
}


void sendWithLora() {           // hand this string over as payload to the LoRA transceiver
  // send packet
  LoRa.beginPacket();
  LoRa.print(cwTxBuffer);
  LoRa.endPacket();
  if (morseState == loraTrx)
      LoRa.receive();
}

void sendWithWifi() {           // hand this string over as payload to the WiFi transceiver
  // send packet
//      const char* peerHost = MorsePreferences::wlanTRXPeer.c_str();
//      IPAddress peerIP;
//      if (MorsePreferences::wlanTRXPeer.length() == 0) {
//          peerHost = "255.255.255.255"; // send to local broadcast IP if not set
//      }
//      if (!peerIP.fromString(peerHost)) {    // try to interpret the peer as an ip address...
//          WiFi.hostByName(peerHost, peerIP); // ...and resolve peer into ip address if that fails
//      }
  //DEBUG("Send with WiFi! " + String(cwTxBuffer));
  MorseWiFi::audp.writeTo((uint8_t*)cwTxBuffer, strlen(cwTxBuffer), peerIP, MORSERINOPORT);
  //MorseWiFi::udp.beginPacket(peerIP, MORSERINOPORT);
  //MorseWiFi::udp.print(cwTxBuffer);
  //MorseWiFi::udp.endPacket();
}

void onLoraReceive(int packetSize){
  String result;
  result.reserve(sizeof(cwTxBuffer));   // we should never receive a packet lomnger than the sender is allowed to send!
  result = "";
  
  // received a packet
  // read packet
  for (int i = 0; i < packetSize; i++)
  {
    result += (char)LoRa.read();
    //DEBUG(String((char)LoRa.read()));
  }
  if (packetSize < sizeof(cwTxBuffer))
      storePacket(LoRa.packetRssi(), result);
  else
      DEBUG("LoRa Packet longer than sizeof(cwTxBuffer) bytes! Discarded...");
}

void onWifiReceive(AsyncUDPPacket packet) {
  String result;
  result.reserve(sizeof(cwTxBuffer));   // we should never receive a packet longer than the sender is allowed to send!
  result = "";
  for (int i = 0; i < packet.length(); i++) {
    result += (char)packet.data()[i];
  }
  //DEBUG("WifiReceive! " + String(result));
  if (packet.length() == 0)             // empty (= keepalive) packet
    return;
  if (packet.length() < sizeof(cwTxBuffer)+1)
      storePacket(WiFi.RSSI(), result);
  else
      DEBUG("UDP Packet longer than sizeof(cwTxBuffer) bytes! Discarded...");
}


String CWwordToClearText(String cwword) {             // decode the Morse code character in cwword to clear text
  int ptr = 0;
  String result;
  result.reserve(40);
  String symbol;
  symbol.reserve(6);

  
  result = "";
  for (int i = 0; i < cwword.length(); ++i) {
      char c = cwword[i];
      switch (c) {
          case '1': ptr = CWtree[ptr].dit;
                    break;
          case '2': ptr = CWtree[ptr].dah;
                    break;
          case '0': symbol = CWtree[ptr].symb;

                    ptr = 0;
                    result += symbol;
                    break;
      }
  }
  symbol = CWtree[ptr].symb;
  result += symbol;
  return encodeProSigns(result);
}


String encodeProSigns( String &input ) {
    /// clean up clearText   -   S <as>,  - A <ka> - N <kn> - K <sk> - H ch - E <ve> etc;
    input.replace("<as>", "S");
    input.replace("<ka>","A");
    input.replace("<kn>","N");
    input.replace("<sk>","K");
    input.replace("<ve>","E");
    input.replace("<ch>","H");
    input.replace("<err>","R");
    input.replace("*", "U");
    //DEBUG(input);
    return input;
}


//// new buffer code: unpack when needed, to save buffer space. We just use 1024 bytes of buffer, instead of 32k! 
//// in addition to the received packet, we need to store the RSSI as 8 bit positive number 
//// (it is always between -20 and -150, so an 8bit integer is fine as long as we store it without sign as an unsigned number)
//// the buffer is a 1024 byte ring buffer with two pointers:
////   nextBuRead where the next packet starts for reading it out; is incremented by l to get the next buffer read position
////      you can read a packet as long as the buffer is not empty, so we need to check bytesBuFree before we read! if it is 1024, the buffer is empty!
////      with a read, the bytesBuFree has to be increased by the number of bytes read
////   nextBuWrite where the next packet should be written; @write:
////       increment nextBuWrite by l to get new pointer; and decrement bytesBuFree by l to get new free space
//// we also need a variable that shows how many bytes are free (not in use): bytesBuFree
//// if the next packet to be stored is larger than bytesBuFree, it is discarded
//// structure of each packet:
////    l:  1 uint8_t length of packet
////    r:  1 uint8_t rssi as a positive number
////    d:  (var. length) data packet as received by LoRa
//// functions:
////    int cwBuWrite(int rssi, String packet): returns length of buffer if successful. otherwise 0
////    uint8_t cwBuRead(uint8_t* buIndex): returns length of packet, and index where to read in buffer by reference
////    boolean cwBuReady():  true if there is something in the buffer, false otherwise
////      example:
////        (somewhere else as global var: ourBuffer[256]
////        uint8_t myIndex;
////        uint8_t mylength;
////        foo() {
////          myLength = cwBuRead(&myIndex);
////          if (myLength != 0) 
////            doSomethingWith(ourBuffer[myIndex], myLength);
////        }


uint8_t cwBuWrite(int rssi, String packet) {
////   int cwBuWrite(int rssi, String packet): returns length of buffer if successful. otherwise 0
////   nextBuWrite where the next packet should be written; @write:
////       increment nextBuWrite by l to get new pointer; and decrement bytesBuFree by l to get new free space
  uint8_t l, posRssi;

  posRssi = (uint8_t) abs(rssi);
  l = 2 + packet.length();
  if (byteBuFree < l)                               // buffer full - discard packet
      return 0;
  cwRxBuffer[nextBuWrite++] = l;
  cwRxBuffer[nextBuWrite++] = posRssi;
  for (int i = 0; i < packet.length(); ++i) {       // do this for all chars in the packet
    cwRxBuffer [nextBuWrite++] = packet[i];       // at end nextBuWrite is alread where it should be
  }
  byteBuFree -= l;
  return l;
}

boolean cwBuReady() {
  if (byteBuFree == sizeof(cwRxBuffer))
    return (false);
  else
    return true;
}


uint8_t cwBuRead(uint8_t* buIndex) {
////    uint8_t cwBuRead(uint8_t* buIndex): returns length of packet, and index where to read in buffer by reference
  uint8_t l;  
  if (byteBuFree == sizeof(cwRxBuffer))
    return 0;
  else {
    l = cwRxBuffer[nextBuRead++];
    *buIndex = nextBuRead;
    byteBuFree += l;
    --l;
    nextBuRead += l;
    return l;
  }
}




void storePacket(int rssi, String packet) {             // whenever we receive something, we just store it in our buffer
  if (cwBuWrite(rssi, packet) == 0)
    DEBUG("cw Buffer full");
}


/// decodePacket analyzes packet as received and stored in buffer
/// returns the header byte (protocol version*64 + 6bit packet serial number
//// byte 0 (added by receiver): RSSI
//// byte 1: header; first two bits are the protocol version (curently 01), plus 6 bit packet serial number (starting from random)
//// byte 2: first 6 bits are wpm (must be between 5 and 60; values 00 - 04 and 61 to 63 are invalid), the remaining 2 bits are already data payload!


uint8_t decodePacket(int* rssi, int* wpm, String* cwword) {
  uint8_t l, c, header=0;
  uint8_t index = 0;

  l = cwBuRead(&index);           // where are we in  the buffer, and how long is the total packet inkl. rssi byte?

  for (int i = 0; i < l; ++i) {     // decoding loop
    c = cwRxBuffer[index+i];

    switch (i) {
      case  0:  * rssi = (int) (-1 * c);    // the rssi byte
                break;
      case  1:  header = c;
                break;
      case  2:  *wpm = (uint8_t) (c >> 2);  // the first data byte contains the wpm info in the first six bits, and actual morse in the remaining two bits
                                            // now take remaining two bits and store them in CWword as ASCII
                *cwword = (char) ((c & B011) +48); 
                break;
      default:                              // decode the rest of the packet; we do this for all but the first byte  /// we need to handle end of word!!! therefore the break
                for (int j = 0; j < 4; ++j) {
                    char cc = ((c >> 2*(3-j)) & B011) ;                // we store them as ASCII characters 0,1,2,3 !
                    if (cc != 3) {
                        *cwword  += (char) (cc + 48);
                    }
                    else break;
                }
                break;
    } // end switch
  }   // end for
  return header;
}      // end decodePacket


///// side tone and external trx key function

void keyOut(boolean on,  boolean fromHere, int f, int volume) {                                      
  //// generate a side-tone with frequency f when on==true, or turn it off
  //// differentiate external (decoder, sometimes cw_generate) and internal (keyer, sometimes Cw-generate) side tones - parameter fromHere
  //// key transmitter (and line-out audio if we are in a suitable mode) - parameter noTx true: do not key transmitter

  static boolean intTone = false;
  static boolean extTone = false;

  static int intPitch, extPitch;

  boolean noTx = ((morseState == echoTrainer || morseState == loraTrx || morseState == wifiTrx || 
      MorsePreferences::keyTrainerMode == 0 || (MorsePreferences::keyTrainerMode == 1 && morseState == morseGenerator)) ? true : !fromHere);

// DEBUG("keyOut: " + String(on) + String(fromHere));
  if (on) {
      if (fromHere) {
        intPitch = f;
        intTone = true;
        MorseOutput::pwmTone(intPitch, volume, true);
        keyTransmitter(noTx);
      } else {                    // not from here
        extTone = true;
        extPitch = f;
        if (!intTone) 
          MorseOutput::pwmTone(extPitch, volume, false);
        }
  } else {                      // key off
        if (fromHere) {
          intTone = false;
          if (extTone)
            MorseOutput::pwmTone(extPitch, volume, false);
          else
            MorseOutput::pwmNoTone();
          digitalWrite(keyerPin, LOW);      // stop keying Tx
        } else {                 // not from here
          extTone = false;
          if (!intTone)
            MorseOutput::pwmNoTone();
        }
  }   // end key off
}

///////////////// a test function for adjusting audio levels

void audioLevelAdjust() {
    uint16_t i, maxi, mini;
    uint16_t dataSize = 1216;
    uint16_t testData[dataSize];

    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, BOLD, 0, "Audio Adjust");
    MorseOutput::printOnScroll(1, REGULAR, 0, "End with RED");
    //keyTx = true;
    keyOut(true,  true, 698, 0);                                  /// we generate a side tone, f=698 Hz, also on line-out, but with vol down on speaker
    while (true) {                                                       // we also key the transmitter (can be used for tuning the Tx...)
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks)
            break;                                                /// pressing the red button gets you out of this mode!
        for (i = 0; i < dataSize ; ++i)
            testData[i] = analogRead(audioInPin);                 /// read analog input
        maxi = mini = testData[0];
        for (i = 1; i< dataSize ; ++i) {
            if (testData[i] < mini)
              mini = testData[i];
            if (testData[i] > maxi)
              maxi = testData[i];
        }
        DEBUG("From " + String(mini) + " to " + String(maxi));
        MorseOutput::showVolumeScope(mini, maxi);
    } // end while
    keyOut(false,  true, 698, 0);                                  /// stop keying
    //keyTx = true;
}



String getWord() {
  String result = "";
  byte c;
  static boolean eof = false;

  if (eof) {          // at eof return empty string
    eof = false;
    //DEBUG("return empty");
    return result;
  }
  while (file.available()) {
      c=file.read();
      if (!isSpace(c))
        result += (char) c;
      else if (result.length() > 0)    {               // end of word
        ++MorsePreferences::fileWordPointer;
        return result;
      }
      //else // l = 0
    } // here eof
    eof = true;
    //DEBUG("EOF!");
    file.close(); file = SPIFFS.open("/player.txt");
    MorsePreferences::fileWordPointer = 0;
    while (!file.available())
      ;
    return result;                                    
}

//// get a string containing all different characters found in a file; used for training with custiomized character set

String getCustomChars() {
  String usedChars = "";
  usedChars.reserve(64);
  String w;
  char c;
  
  file.close(); file = SPIFFS.open("/player.txt");
  MorsePreferences::fileWordPointer = 0;
  while (file.available()) {
      w = getWord();
      if (w == "")
        break;
      w = cleanUpText(w);
      for (unsigned int i = 0; i<w.length(); ++i) {
        if ( usedChars.indexOf(c = w.charAt(i)) == -1 )
          usedChars += c;
      }
  }
  file.close(); file = SPIFFS.open("/player.txt");
  //DEBUG("usedChars: " + usedChars);
  return usedChars;
}


String cleanUpText(String text) {                        // all to lower case, and convert umlauts
  String result = "";
  char c;
  result.reserve(64);
  text.toLowerCase();
  text = utf8umlaut(text);
  
  for (unsigned int i = 0; i<text.length(); ++i) {       // disregard all non-standard characters
    if ((koch.morserinoKochChars.indexOf(c = text.charAt(i)) != -1) || c == 'P' || c == 'T' )      // P for pause  
      result += c;
  }
  return result;
}


String utf8umlaut(String s) { /// replace umtf umlauts with digraphs, and interpret pro signs, written e.g. as [kn] or <kn> or \KN
      s.replace("ä", "ae");
      s.replace("ö", "oe");
      s.replace("ü", "ue");
      s.replace("Ä", "ae");
      s.replace("Ö", "oe");
      s.replace("Ü", "ue");
      s.replace("ß", "ss");
      s.replace("[", "<");
      s.replace("]", ">");
      s.replace("<ar>", "+");
      s.replace("<bt>", "=");
      s.replace("<as>", "S");
      s.replace("<ka>", "A");
      s.replace("<kn>", "N");
      s.replace("<sk>", "K");
      s.replace("<ve>", "E");
      s.replace("<p>", "P");    // pause
      s.replace("<t>", "T");    // change tone
      s.replace("\\ar", "+");
      s.replace("\\bt", "=");
      s.replace("\\as", "S");
      s.replace("\\ka", "A");
      s.replace("\\kn", "N");
      s.replace("\\sk", "K");
      s.replace("\\ve", "E");
      s.replace("\\p", "P");
      s.replace("\\t", "T");
      return s;
}

void skipWords(uint32_t count) {             /// just skip count words in open file fn
  while (count > 0) {
    getWord();
    --count;
  }
}
