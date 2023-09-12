/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018-2020  Willi Kraml, OE1WKL                                                                                 ***
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

#include <Preferences.h>   // ESP 32 library for storing things in non-volatile storage
#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "abbrev.h"
#include "english_words.h"
#include "ClickButton.h"   // button control library
#include "goertzel.h"

using namespace MorsePreferences;

Preferences pref;               // use the Preferences library for storing and retrieving objects

///// the preferences variable and their defaults

//// First, all those that can be changed in the parameters (preferences) menu

#define SizeOfArray(x)       (sizeof(x) / sizeof(x[0]))

  /* ////////////////// order of preferences //////////// make sure the next twom items are in sync with the following, up to posSerislOut:
  enum prefPos : uint8_t 
      {posClicks, posPitch, posExtPddlPolarity, posPolarity,                                        // 0
      posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,                              // 4
      posEchoToneShift, posInterWordSpace, posInterCharSpace, posRandomOption,                      // 8
      posRandomLength, posCallLength, posAbbrevLength, posWordLength,                               // 12
      posGeneratorDisplay, posWordDoubler, posEchoDisplay, posEchoRepeats,  posEchoConf,            // 16           
      posKeyExternalTx, posLoraCwTransmit, posGoertzelBandwidth, posSpeedAdapt,                     // 21
      posKochSeq, posCarouselStart, posLatency, posRandomFile, posExtAudioOnDecode, posTimeOut,     // 25
      posQuickStart, posAutoStop, posMaxSequence, posLoraChannel,   posSerialOut,                   // 31
      // to be treated differently:
      posKochFilter,                                                                                // 36
      posLoraBand, posLoraQRG, posSnapRecall, posSnapStore,  posVAdjust, posHwConf                  // 37
      };
  */

const char * prefName[] = {"encoderClicks", "sidetoneFreq", "useExtPaddle", "didah", 
                      "keyermode", "curtisBTiming", "curtisBDotT", "ACSlength",
                      "echoToneShift", "interWordSpace", "farnsworthMode", "randomOption",
                      "randomLength", "callLength", "abbrevLength", "wordLength",
                      "GeneratorDispl", "wordDoubler", "echoDisplay", "echoRepeats", "echoConf",
                      "KeyExternalTx", "LoraCwTransmit", "goertzelBW", "speedAdapt",
                      "KochSeq", "carouselStart", "latency", "randomFile", "extAudioOnDecod", "timeOut",
                      "quickStart", "autoStop", "maxSequence", "LoraChannel", "serialOut"};

parameter MorsePreferences::pliste[] = {
  {
    1, 0, 1, 1,                                                 // should rotating the encoder generate a click? yes If 1
    "Encoder Click",
    "Click when encoder is turned",
    true,
    {"Off", "On"}
  },
  {
    10, 0, 14, 1,                                                // side tone frequency    0-14  was  1 - 15
    "Tone Pitch",
    "Pitch of CW tone",
    true,
    {"233 Hz bflat", "262 Hz c1", "294 Hz d1", "311 Hz e1", "349 Hz f1", "392 Hz g1", "440 Hz a1", "466 Hz b1flat", "523 Hz c2", "587 Hz d2", "622 Hz e2", "698 Hz f2", "784 Hz g2", "880 Hz a2", "932 Hz b2flat"}
  },
  {
    0, 0, 1, 1,                                                   // true (1) when we need to reverse the polarity of the ext paddle
    "External Pol.",
    "Polarity of external paddle",
    true,
    {"Normal", "Reversed"}
  },
  {
    1, 0, 1, 1,                                                 // paddle polarity
    "Paddle Polar.",
    "Where are dits and dahs",
    true,
    {"-. dah dit", ".- dit dah"}
  },
  {
    2, 1, 5, 1,                                                 // keyer modes (Curtis modes etc.)
    "Keyer Mode",
    "Iambic Modes, Non-squeeze mode, Straight Key mode",
    true,
    {"", "Iambic A", "Iambic B", "Ultimatic", "Non-Squeeze", "Straight Key" }
  },
  {
    45, 0, 100, 5,
    "CurtisB DahT%",                                          // keyer: timing for enhanced Curtis mode: dah                    0 - 100
    "Timing for CurtisB Dahs in %",
    false,
    {}
  },
  {
    75, 0, 100, 5,                                              // keyer: timing for enhanced Curtis mode: dit                    0 - 100
    "CurtisB DitT%",
    "Timing for CurtisB Dits in %",
    false,
    {}
  },
  {
    0, 0, 3, 1,                                                   // AutoChar Spacing: extend the pause between chars; 0=off, 1-3 ==> 2-4
    "AutoChar Spc",                                           
    "ACS: Minimum spacing between characters",
    true,
    {"Off", "2 dits", "3 dits", "4 dits"}
  },
  {
    1, 0, 2, 1,                                                 // 0 = no shift, 1 = up, 2 = down (a half tone)                   0 - 2
    "Tone Shift",
    "Shift tones up or down (at receive, or echo input)",
    true,
    {"No Tone Shift", "Up 1 Half", "Down 1 Half"}
  },
  {
    7, 6, 105, 1,                                                // Generator: normal interword spacing in lengths of dit,          6 - 45 ; default = norm = 7
    "InterWord Spc",
    "The time (in dits) that is inserted between generated words",
    false,
    {}
  },
  {
    3, 3, 45, 1,                                                  // for generators: intercharacter space, in dit dit lengths; default = 3
    "Interchar Spc",
    "Space between generated characters, in dits",
    false,
    {}
  },
  {
    0, 0, 9,  1,                                                // Generators: from which pool are we generating random characters?
    "Random Groups",
    "Which character subsets should be used for generating",
    true,
    {"All Chars","Alpha", "Numerals", "Interpunct.", "Pro Signs", "Alpha + Num", "Num+Interp.", "Interp+ProSn","Alph+Num+Int","Num+Int+ProS"}
  },
  {
    3, 1, 10, 1,                                                // generators: length of randomchar groups 2-6; 7-10 for rnd length 2 to 3-6
    "Length Rnd Gr",
    "How many characters in each group of random characters?",
    true,
    {"", "1", "2", "3", "4", "5", "6", "2 to 3", "2 to 4", "2 to 5", "2 to 6"}
  },
  {
    0, 0, 4, 1,                                                 // Generators: max length of call signs generated (0 = unlimited)    0, 3 - 6
    "Length Calls",                                           
    "Maximum length of generated call signs",
    true,
    {"Unlimited", "3", "4", "5", "6"}
  },
  {
    0, 0, 5,  1,                                                // Generators: max length of abbreviations generated (0 = unlimited) 0, 1-5 = 2-6
    "Length Abbrev",                                          
    "Maximum length of generated common CW abbreviations",
    true,
    {"Unlimited", "2", "3", "4", "5", "6"}
  },
  {
    0, 0, 5,  1,                                                // Generators: max length of english words generated (0 = unlimited) 0, 1-5 = 2-6
    "Length Words",                                           
    "Maximum length of generated common English words",
    true,
    {"Unlimited", "2", "3", "4", "5", "6"}
  },
  {
    1, 0, 2,  1,                                                // Generator: how we display what the trainer generates: nothing, by char, or by word  0-2
    "CW Gen Displ",                                           
    "No, char by char or word by word display",
    true,
    {"Display off", "Char by char", "Word by word"}
  },
  {
    0, 0, 3,  1,                                                // in CW trainer mode: repeat each word?
    "Each Word 2x",
    "Repeat each generated word",
    true,
    {"OFF", "ON", "ON (less ICS)", "ON (true WpM)"}
  },
  {
    1, 1, 3,  1,                                                //  1 = CODE_ONLY 2 = DISP_ONLY 3 = CODE_AND_DISP
    "Echo Prompt",
    "Echo Trainer prompt by sound, display, or both?",
    true,
    {"", "Sound only", "Display only", "Sound & Disp"}
  },
  {
    3, 0, 7, 1,                                                 // how often will echo trainer repeat after an error?             0 - 7, 7=forever, default = 3
    "Echo Repeats",
    "Maximum repetition of words in Echo Trainer after error",
    true,
    {"0", "1", "2", "3", "4", "5", "6", "Forever"}
  },
  {
    1, 0, 1,  1,                                                // true if echo trainer confirms audibly too, not just visually
    "Confrm. Tone",
    "Audible confirmation in Echo Trainer",
    true,
    {"OFF", "ON"}
  },
  {
    1, 0, 3, 1,                                                 // key TX in generator/player/receiver mode?
    "Key ext TX",
    "When to key an external transmitter",
    true,
    {"Never", "CW Keyer only", "Keyer & Gen.", "Keyer&Gen.&RX"}
  },
  {
    0, 0, 2,  1,                                                // transmit generated/played things via LoRa or WiFi?
    "Generator Tx",
    "Generated CW to be sent by LoRa or Wifi?",
    true,
    {"Tx OFF", "LoRa Tx ON", "WiFi Tx ON"}
  },
  {
    0, 0, 1, 1,                                                 //  0: "Wide" 1: "Narrow"
    "Bandwidth",
    "Audio bandwidth of the CW decoder",
    true,
    {"Wide", "Narrow"}
  },
  {
    0, 0, 1, 1,                                                 //  true: in echo modes, increase speed when OK, reduce when not ok
    "Adaptv. Speed",
    "Adaptive Speed of Echo Trainer?",
    true,
    {"OFF", "ON"}
  },
  {
    0, 0, 4, 1,                                                 // select Koch sequence: 0 = native/JLMC, 1 = LCWO, 2 = CW Academy, 3 = LICW, 4 = Custom
    "Koch Sequence",                                          
    "Sequence of characters for the Koch method",
    true,
    {"M32", "LCWO", "CW Academy", "LICW Carousel", "Custom Chars"}
  },
  {
    0, 0, 13,  1,                                               // Offset for LICW Carousel
    "LICW Carousel",
    "Entry Point into the LICW Carousel curriculum",
    true,
    {"BC1: r e a", "BC1: t i n", "BC1: p g s", "BC1: l c d", "BC1: h o f", "BC1: u w b",
      "BC2: k m y", "BC2: 5 9 ,", "BC2: q x v", "BC2: ar sk =", "BC2: 1 6 .",
      "BC2: z j /", "BC2: 2 8 bk", "BC2: 4 0"}
  },
  {
    4, 0, 7, 1,                                                 //  time span after currently sent element during which paddles are not checked; in 1/8th dits
    "Latency",
    "How long (in dit) after generating dit or dah the resp. paddle will be insensitive",
    true,
    {"0%", "12.5%", "25%", "37.5%", "50%", "62.5%", "75%", "87.5%", "100%"}
  },
  {
    0, 0, 1,  1,                                                // if 0, play file word by word; if 1, skip random number of words (0 - 255) between reads
    "Randomize File",                                         
    "Should file player skip words randomly",
    true,
    {"OFF", "ON"}
  },
  {
    0, 0, 1, 1,                                                 // send decoded audio also to external audio  I/O port
    "Decoded on IO",
    "Decoded audio to be sent to the I/O port)",
    true,
    {"OFF", "ON"}
  },
  {
    1, 0, 3,  1,                                                // time-out value: 0 = no timeout, 1 = 5 min, 2 = 10 min, 3 = 15 min
    "Time-out",
    "Time of inactivity before the device will go to sleep mode",
    true,
    {"No time-out", "5 min", "10 min", "15 min"}
  },
  {
    0, 0, 1, 1,                                                 // should we start the last executed command immediately?
    "Quick Start",
    "Bypass initial menu selection and start with mode used last",
    true,
    {"OFF", "ON"}
  },
  {
    0, 0, 1, 1,                                                 // Stop after each word in CW generator modes?
    "Stop<Next>Rep",
    "Stop after each word; choose repeat or next word with paddle",
    true,
    {"OFF", "ON"}
  },
  {
    0, 0, 250, 5,                                               // max # of words generated before the Morserino pauses, 0 = no limit; allow step = 5 only
    "Max # of Words",
    "Stop after selected number of words",
    false,
    {}
  },
  {
    0, 0, 1,  1,                                                // allows to set different LoRa sync words, and so creating virtual "channels"; 0 = 0x27, 1 = 0x66
    "LoRa Channel",
    "Which virtual channel is used by LoRa",
    true,
    {"Standard", "Secondary"}
  },
  {
    5, 0, 5, 1,                                                 // output characters on USB serial? 0 = none (but DEBUG/ERR) 1= keyed, 2 = decode, 3=both, 4=generated, 5=all
    "Serial Output",
    "Select what is sent to the serial (USB) port",
    true,
    {"Nothing", "Keyed", "Decoded", "Keyed+Decoded", "Generated", "All"}
  } 
};

String extraItems[] = {"Koch Lesson", "LoRa Band",  "LoRa Frequ", "LoRa Power", "RECALLSnapshot", "STORE Snapshot", "Calibrate Batt", "Hardware Conf" };

///// used like a parameter, but not in parameter menuPtr


uint8_t MorsePreferences::loraBand = 0;                     // 0 = 433, 1 = 868, 2 = 920
uint32_t MorsePreferences::loraQRG = QRG433;                // for 70 cm band
uint8_t MorsePreferences::loraPower = 14;                   // default 14 dBm = 25 mW



  ///// stored in preferences, but not adjustable through preferences menu:

  uint8_t MorsePreferences::version_major = VERSION_MAJOR;
  uint8_t MorsePreferences::version_minor = VERSION_MINOR;

  uint8_t MorsePreferences::sidetoneVolume = 16;              // side tone volume, as a value between 0 and 19   0 -19
  extern const uint8_t MorsePreferences::volumeMin = 0;
  extern const uint8_t MorsePreferences::volumeMax = 19;

  uint8_t MorsePreferences::wpm = 15;                         // keyer speed in words per minute                  5 - 60
  extern const uint8_t MorsePreferences::wpmMin = 5;
  extern const uint8_t MorsePreferences::wpmMax = 60;

  uint8_t MorsePreferences::kochFilter = 5;                   // constrain output to characters learned according to Koch's method 2
  uint8_t MorsePreferences::kochCharsLength = 52;             // # of chars for Koch training
  uint8_t MorsePreferences::kochMinimum = 1;
  uint8_t MorsePreferences::kochMaximum = 52;
  String  MorsePreferences::customCharSet = "";               // a place to store the custom character set
  boolean MorsePreferences::useCustomChars = false;           // flag if we should use custom characters
  uint8_t MorsePreferences::responsePause = 5;                // in echoTrainer mode, how long do we wait for response? in interWordSpaces; 2-12, default 5

  uint8_t MorsePreferences::menuPtr = 1;                      // current position of menu
  uint8_t MorsePreferences::newMenuPtr = 1;                   // current position of menu when changed
  String  MorsePreferences::wlanSSID = "";                    // SSID for connecting to the Internet
  String  MorsePreferences::wlanPassword = "";                // password for connecting to WiFi router
  String  MorsePreferences::wlanTRXPeer = "";                 // peer Morserino for WiFI TRX
  String  MorsePreferences::wlanSSID1 = "";                    // SSID for connecting to the Internet
  String  MorsePreferences::wlanPassword1 = "";                // password for connecting to WiFi router
  String  MorsePreferences::wlanTRXPeer1 = "";                 // peer Morserino for WiFI TRX
  String  MorsePreferences::wlanSSID2 = "";                    // SSID for connecting to the Internet
  String  MorsePreferences::wlanPassword2 = "";                // password for connecting to WiFi router
  String  MorsePreferences::wlanTRXPeer2 = "";                 // peer Morserino for WiFI TRX
  String  MorsePreferences::wlanSSID3 = "";                    // SSID for connecting to the Internet
  String  MorsePreferences::wlanPassword3 = "";                // password for connecting to WiFi router
  String  MorsePreferences::wlanTRXPeer3 = "";                 // peer Morserino for WiFI TRX

  uint32_t MorsePreferences::fileWordPointer = 0;             // remember how far we have read the file in player mode / reset when loading new file
  uint8_t MorsePreferences::promptPause = 2;                  // in echoTrainer mode, length of pause before we send next word; multiplied by interWordSpace
  uint8_t MorsePreferences::tLeft = 20;                       // threshold for left paddle
  uint8_t MorsePreferences::tRight = 20;                      // threshold for right paddle

  uint8_t MorsePreferences::vAdjust = 180;                    // correction value: 155 - 250


  uint8_t MorsePreferences::snapShots = 0;                    // keep track which snapshots are being used ( 0 .. 7, called 1 to 8)

  uint8_t MorsePreferences::boardVersion = 0;                 // which Morserino board version? v3 uses heltec Wifi Lora V2, V4 uses V2.1

  uint8_t MorsePreferences::oledBrightness = 255;


//////// end of variables stored in preferences

//// temporary buffer for conversions, local to this file, and some other items neeeded for preference menus
char numBuffer[16];

String topLine; //topLine.reserve(20);
String itemLine; //itemLine.reserve(20);
const int topMax = 17;  const int elseMax = 13;
const String emptyLine = "                    ";

/// variables for managing snapshots
uint8_t MorsePreferences::memories[8];
uint8_t MorsePreferences::memCounter;
uint8_t MorsePreferences::memPtr = 0;


  prefPos MorsePreferences::keyerOptions[] =    { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency 
                                                 };
  prefPos MorsePreferences::generatorOptions[] = { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, posInterCharSpace, posInterWordSpace, 
                                                   posRandomOption, posRandomLength, posCallLength, posAbbrevLength,  posWordLength, 
                                                   posMaxSequence, posAutoStop, posGeneratorDisplay, posWordDoubler, 
                                                   posKeyExternalTx, posLoraCwTransmit, posLoraChannel
                                                 };
 prefPos MorsePreferences::playerOptions[] =     { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posInterCharSpace, posInterWordSpace, 
                                                   posMaxSequence, posAutoStop, posGeneratorDisplay, posRandomFile, posWordDoubler, 
                                                   posKeyExternalTx, posLoraCwTransmit, posLoraChannel
                                                 };
                                                 
 prefPos MorsePreferences::echoPlayerOptions[] = { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency, posInterCharSpace, posInterWordSpace, 
                                                   posMaxSequence, posRandomFile, posEchoRepeats, posEchoDisplay, posEchoConf, posEchoToneShift, posSpeedAdapt, 
                                                 };
                                                 
 prefPos MorsePreferences::echoTrainerOptions[]= { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency, posInterCharSpace, posInterWordSpace, 
                                                   posRandomOption, posRandomLength, posCallLength, posAbbrevLength,  posWordLength, 
                                                   posMaxSequence, posEchoRepeats, posEchoDisplay, posEchoConf, posEchoToneShift, posSpeedAdapt, 
                                                 };
                                                 
 prefPos MorsePreferences::kochGenOptions[] =    { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posKochSeq, posCarouselStart, posInterCharSpace,  posInterWordSpace, posRandomLength, posAbbrevLength,  posWordLength, 
                                                   posMaxSequence, posAutoStop, posGeneratorDisplay, posWordDoubler, 
                                                   posKeyExternalTx, posLoraCwTransmit, posLoraChannel
                                                 };
                                                 
 prefPos MorsePreferences::kochEchoOptions[] =   { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency, posKochSeq, posCarouselStart, 
                                                   posInterCharSpace, posInterWordSpace, posRandomLength, posAbbrevLength,  posWordLength, 
                                                   posMaxSequence, posEchoRepeats, posEchoDisplay, posEchoConf, posEchoToneShift, posSpeedAdapt, 
                                                 };
                                                 
 prefPos MorsePreferences::loraTrxOptions[] =    { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency, posGeneratorDisplay, 
                                                   posEchoToneShift, posKeyExternalTx, posLoraChannel, posExtAudioOnDecode 
                                                 };
                                                 
 prefPos MorsePreferences::wifiTrxOptions[] =    { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency, posGeneratorDisplay, posEchoToneShift,
                                                   posKeyExternalTx,  posExtAudioOnDecode 
                                                 };
                                                 
 prefPos MorsePreferences::extTrxOptions[] =     { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency, posEchoToneShift, 
                                                   posGoertzelBandwidth, posExtAudioOnDecode 
                                                 };
                                                 
 prefPos MorsePreferences::decoderOptions[] =    { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, 
                                                   posCurtisMode, posGoertzelBandwidth, posExtAudioOnDecode 
                                                 };

 prefPos MorsePreferences::allOptions[] =        { posClicks, posPitch, posTimeOut, posQuickStart, posSerialOut, posPolarity, posExtPddlPolarity, 
                                                   posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,  posLatency, posKochSeq, posCarouselStart, 
                                                   posInterCharSpace, posInterWordSpace, posRandomOption, posRandomLength, posCallLength, posAbbrevLength,  posWordLength, 
                                                   posMaxSequence, posAutoStop, posGeneratorDisplay, posRandomFile, posWordDoubler, 
                                                   posEchoRepeats, posEchoDisplay, posEchoConf, posEchoToneShift, posSpeedAdapt, 
                                                   posKeyExternalTx, posLoraCwTransmit, posLoraChannel, posGoertzelBandwidth, posExtAudioOnDecode 
                                                 };

prefPos *MorsePreferences::currentOptions = MorsePreferences::allOptions;

int MorsePreferences::keyerOptionsSize = SizeOfArray(MorsePreferences::keyerOptions);
int MorsePreferences::generatorOptionsSize = SizeOfArray(MorsePreferences::generatorOptions);
int MorsePreferences::playerOptionsSize = SizeOfArray(MorsePreferences::playerOptions);
int MorsePreferences::echoPlayerOptionsSize = SizeOfArray(MorsePreferences::echoPlayerOptions);
int MorsePreferences::echoTrainerOptionsSize = SizeOfArray(MorsePreferences::echoTrainerOptions);
int MorsePreferences::kochGenOptionsSize = SizeOfArray(MorsePreferences::kochGenOptions);
int MorsePreferences::kochEchoOptionsSize = SizeOfArray(MorsePreferences::kochEchoOptions);
int MorsePreferences::loraTrxOptionsSize = SizeOfArray(MorsePreferences::loraTrxOptions);
int MorsePreferences::wifiTrxOptionsSize = SizeOfArray(MorsePreferences::wifiTrxOptions);
int MorsePreferences::extTrxOptionsSize = SizeOfArray(MorsePreferences::extTrxOptions);
int MorsePreferences::decoderOptionsSize = SizeOfArray(MorsePreferences::decoderOptions);
int MorsePreferences::allOptionsSize = SizeOfArray(MorsePreferences::allOptions);


int currentOptionSize;

extern double voltage_raw;
extern int16_t volt;

////// setup preferences ///////


boolean MorsePreferences::setupPreferences(uint8_t atMenu) {
  // enum morserinoMode {morseKeyer, loraTrx, morseGenerator, echoTrainer, shutDown, morseDecoder, invalid };
  static int oldPos = 1;
  int t;

  int ptrIndex, ptrMax;
  prefPos posPtr;

  m32state = preferences_loop;
  ptrMax = currentOptionSize;

  ///// we should check here if the old ptr (oldIndex) is contained in the current preferences collection (currentOptions)
  ptrIndex = 1;
  for (int i = 0; i < ptrMax; ++i) {
      if (currentOptions[i] == oldPos) {
          ptrIndex = i;
          break;
      }
  }
  posPtr = currentOptions[ptrIndex];
  keyOut(false, true, 0, 0);                // turn the LED off, unkey transmitter, or whatever; just in case....
  keyOut(false,false, 0, 0);
  displayKeyerPreferencesMenu(posPtr);
  MorseOutput::printOnScroll(2, REGULAR, 0,  " ");

  while (true) {                            // we wait for single click = selection or long click = exit - or single or long click or RED button, or for a serial event
        serialEvent();
        if (goToMenu) {
            jsonActivate(ACT_EXIT); 
            goToMenu = false;
            goto exitFromHere;
        }        
        Buttons::modeButton.Update();
        switch (Buttons::modeButton.clicks) {            // button was clicked
          case 1:     if (adjustKeyerPreference(posPtr)) 
                         goto exitFromHere;
                      break;
          case -1:    //////// long press indicates we are done with setting preferences - check if we need to store some of the preferences

          exitFromHere: if (MorsePreferences::useCustomChars)
                            koch.setCustomChars(getCustomChars()); //// get custom characters
                        if (m32protocol && posPtr < posKochFilter)
                            jsonActivate(ACT_EXIT);
                        writePreferences("morserino");
                        //delay(200);
                        return false;
                        break;
          }

          Buttons:: volButton.Update();                 // RED button
          switch (Buttons:: volButton.clicks) {         // was clicked
            case 1:     // recall snapshot
                        if (MorsePreferences::recallSnapshot()) {
                          writePreferences("morserino");
                          if (m32protocol)
                              jsonActivate(ACT_RECALLED);
                        }
                        else if(m32protocol)
                          jsonActivate(ACT_CANCELLED);
                        return true;
                        break;
            case 2:     MorseOutput::decreaseBrightness();
                        displayKeyerPreferencesMenu(posPtr);
                        break;
            case -1:    //store snapshot
                        if (MorsePreferences::storeSnapshot(atMenu)) {
                          // writePreferences("morserino"); now in writePreferences()
                          if (m32protocol)
                              jsonActivate(ACT_SET);
                        }
                        else if (m32protocol)
                          jsonActivate(ACT_CANCELLED);
                        while(Buttons:: volButton.clicks)
                          Buttons:: volButton.Update();
                        return false;
                        break;
          }


          //// display the value of the preference in question

         if ((t=checkEncoder())) {
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);         /// click
            ptrIndex = (ptrIndex +ptrMax + t) % ptrMax;
            posPtr = currentOptions[ptrIndex];
                                                             // remember menu position
            oldPos = posPtr;

            displayKeyerPreferencesMenu(posPtr);
            MorseOutput::printOnScroll(2, REGULAR, 0, " ");

            Heltec.display -> display();                                                        // update the display
         }    // end if (encoderPos)
         checkShutDown(false);         // check for time out
  } // end while - we leave as soon as the button has been pressed long
}   // end function setupKeyerPreferences()


//////// Display the preferences menu - we display the following preferences

//// new way of displaying it

void MorsePreferences::displayKeyerPreferencesMenu(prefPos pos) {
  const int maxLength = 14;
  
  MorseOutput::clearDisplay();
  
  if (pos < posKochFilter)
    topLine = "Set Preferences:";
  else if (pos < posLoraBand)
    topLine = "Set Lesson #:";
  else if (pos < posSnapRecall)
    topLine = "Config LoRa:";
  else if (pos < posVAdjust)
    topLine = "Manage Snapshots:";
  else if (pos < posHwConf)
    topLine = "Calibrate Voltage";
  else
    topLine = "Hardware Config.";

  topLine += emptyLine.substring(0,topMax - topLine.length());
  MorseOutput::printOnStatusLine( true, 0, topLine);

  itemLine = (pos <= posSerialOut ? MorsePreferences::pliste[pos].parName : extraItems[pos-posKochFilter]);
  itemLine += emptyLine.substring(0,maxLength - itemLine.length());
  MorseOutput::printOnScroll(1, BOLD, 0, itemLine);  
  displayValueLine(pos, itemLine, false);                              
}

/// posKochFilter, posLoraBand, posLoraQRG, posSnapRecall, posSnapStore,  posVAdjust, posHwConf



void MorsePreferences::displayValueLine(prefPos pos, String itemText, boolean jsonOnly) {
    String valueLine; valueLine.reserve(20);
    const String emptyLine = "                    ";
    const int maxLength = 14;
    String jsonValueLine; jsonValueLine.reserve(48);
    String item; item.reserve(18);
    int value;


    value = pos <= posSerialOut ? (int) pliste[pos].value : getValue(pos);
    valueLine = (pos <= posSerialOut ? (pliste[pos].isMapped ? pliste[pos].mapping[pliste[pos].value] : String(pliste[pos].value)) : getValueLine(pos));
    if (pos == posMaxSequence && pliste[pos].value == 0)                  /// we do a "mapping" for 0 here
        valueLine = "Unlimited";
    valueLine += emptyLine.substring(0,maxLength - valueLine.length());

    if (m32protocol) {
      jsonValueLine = valueLine;
      jsonValueLine.trim();
      switch (pos) {
        case posKochFilter:
                item = "Select Lesson";
                break;
        case posSnapRecall:
                item = "Recall Snapshot";
                break;
        case posSnapStore:
                item = "Store Snapshot";
                break;
        default: 
                item = itemText;
                item.trim();
                break;
      }
      if (pos < posKochFilter || pos == posSnapRecall || pos == posSnapStore) {
          jsonConfigShort(item, value, jsonValueLine);
      }
      else if (pos == posKochFilter)
          jsonMenu(MorseMenu::getMenuPath(MorsePreferences::menuPtr) + "/" + jsonValueLine, (unsigned int) MorsePreferences::menuPtr, 
              (m32state == menu_loop ? false : true), MorseMenu::isRemotelyExecutable(MorsePreferences::menuPtr));
    }
    if (! jsonOnly) 
      MorseOutput::printOnScroll(2, REGULAR, 1, valueLine);
}

int MorsePreferences::getValue(prefPos pos) {     /// a value to return for m32protocol
  switch (pos) {
    case posKochFilter:
          return (int) MorsePreferences::kochFilter;
          break;
    case posSnapRecall:
          if (MorsePreferences::memCounter && MorsePreferences::memPtr != MorsePreferences::memCounter)
            return MorsePreferences::memories[MorsePreferences::memPtr] +1;
          break;
    case posSnapStore:
          if (MorsePreferences::memPtr < 8)
            return MorsePreferences::memPtr+1;
          break;
  }
  return 0;
}

String MorsePreferences::getValueLine(prefPos pos) {
  String str; str.reserve(14);
  uint8_t mask;
  const int a = (int) QRG433;
  const int b = (int) QRG866;
  const int c = (int) QRG920;
  String milliWatt[] = {"10", "12.5", "16", "20", "25", "32", "40", "50", "63", "80", "100"};
  
  switch (pos) {
    case posKochFilter:
      str = koch.getNewChar();
      cleanUpProSigns(str);
      sprintf(numBuffer, "%2i char %s", MorsePreferences::kochFilter, str.c_str());
      str = String(numBuffer);
      break;
    case posLoraBand:
      switch (MorsePreferences::loraBand) {
          case 0: str = "433 MHz";
                  break;
          case 1: str = "868 MHz";
                  break;
          case 2: str = "920 MHz";
                  break;
      }
      break;
    case posLoraQRG:
      sprintf(numBuffer, "%6d kHz", MorsePreferences::loraQRG / 1000);
      str = String(numBuffer);
      switch (MorsePreferences::loraQRG) {
         case a:
         case b:
         case c: str += " DEF";
      }
      break;
    case posSnapRecall:
      if (MorsePreferences::memCounter) {
        if (MorsePreferences::memPtr == MorsePreferences::memCounter)
          str = "Cancel Recall";
        else {
          sprintf(numBuffer, "Snapshot %d", MorsePreferences::memories[MorsePreferences::memPtr] +1);
          str = String(numBuffer);
        }
      }
      else
        str = "NO SNAPSHOTS";
      break;
    case posSnapStore:
      mask = 1; mask = mask << MorsePreferences::memPtr;
      if (MorsePreferences::memPtr == 8)
        str = "Cancel Store";
      else {
        sprintf(numBuffer, "Snapshot %d", MorsePreferences::memPtr+1);
        str = String(numBuffer);
      }
      break;
    case posVAdjust:
      volt = (int16_t) (voltage_raw *  (MorsePreferences::vAdjust * 12.9));   // recalculate millivolts for new adjustment
      sprintf(numBuffer, "%4d mV", volt);
      str = String(numBuffer);
      break;
    case posHwConf:
      switch (hwConf) {
        case 1:   str = "Calibr. Batt.";
                  break;
        case 2:   str = "LoRa Config.";
                  break;
        default:  str = "Cancel";
                  break;
        }
      break;
    case posLoraPower:
      str = milliWatt[MorsePreferences::loraPower - 10] + " mW";
  }
  return str;
}

//// function to adjust the selected preference

boolean MorsePreferences::adjustKeyerPreference(prefPos pos) {        /// rotating the encoder changes the value, click returns to preferences menu
                                                                      /// returns true when a long button press ended it, and false when there was a short click
    MorseOutput::printOnScroll(2, INVERSE_BOLD, 0, ">");
    uint8_t seq;
    int8_t t;

    uint16_t val, maxi, mini, vstep, temp;
    
    while (true) {                            // we wait for single click = selection or long click = exit
        serialEvent();
        if (goToMenu) {
            jsonActivate(ACT_EXIT); 
            goToMenu = false;
            return true;
        } 
        pinMode(modeButtonPin, INPUT);

        Buttons::modeButton.Update();
        switch (Buttons::modeButton.clicks) {
          case -1 : // long press = return and exit pref menu
                    return true;
                    break;
          case  1 : // short press = set & return, but stay in pref menu
                    MorseOutput::printOnScroll(2, REGULAR, 0,  " ");
                    return false;
        }
        if (pos == posSnapRecall) {         // here we can delete a memory....
          Buttons:: volButton.Update();
          if (Buttons:: volButton.clicks) {
            if (MorsePreferences::memCounter) {
              clearMemory(MorsePreferences::memPtr);
              if (m32protocol)
                jsonActivate(ACT_CLEARED);
            }
            return true;
          }
        }
        if ((t=checkEncoder())) {                                            /// t == 1 or -1
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);         /// click;
            
            if (pos <= posSerialOut) {                                       /// "normal" procedure through preferences menu
                  /// val = (((val + maxi - 2*mini + vstep  + t*vstep) % (maxi - mini +vstep)) + mini); we calculate the new value, step up or down
                  val = pliste[pos].value;
                  mini = pliste[pos].minimum;
                  maxi = pliste[pos].maximum;
                  vstep = pliste[pos].stepValue;
                  if (mini == 0) {
                      temp = val + maxi + vstep + t*vstep;
                      pliste[pos].value = temp % (maxi + vstep);
                      
                      if (pliste[pos].value != 0) {
                        if (pos == posWordDoubler) {
                            pliste[posAutoStop].value = 0;
                            displayValueLine(posAutoStop, MorsePreferences::pliste[posAutoStop].parName, true);
                        }
                        else if (pos == posAutoStop) {
                            pliste[posWordDoubler].value = 0;
                            displayValueLine(posWordDoubler, MorsePreferences::pliste[posWordDoubler].parName,  true);
                        }
                      }
                  } else {
                      temp = val + maxi - 2*mini + vstep  + t*vstep;
                      pliste[pos].value = (temp % (maxi - mini +vstep)) + mini;
                  }  
                  if (pos == posKochSeq) 
                      MorsePreferences::handleKochSequence();
                  else if (pos == posCarouselStart && pliste[posKochSeq].value == 3) 
                      MorsePreferences::handleCarouselChange();
            } else {                                                        /// "strange" way of adjusting, outside preferences menu
                  switch (pos) {                      
                      case  posKochFilter: 
                                  MorsePreferences::kochFilter = constrain(MorsePreferences::kochFilter +t, MorsePreferences::kochMinimum, MorsePreferences::kochMaximum);
                                  break;
                      case posLoraBand:
                                  MorsePreferences::loraBand += (t+1);                              // set the LoRa band
                                  MorsePreferences::loraBand = constrain(MorsePreferences::loraBand-1, 0, 2);
                                  switch (MorsePreferences::loraBand) {
                                    case 0:  MorsePreferences::loraQRG = QRG433;
                                        break;
                                    case 1:  MorsePreferences::loraQRG = QRG866;
                                        break;
                                    case 2:  MorsePreferences::loraQRG = QRG920;
                                        break;
                                  }
                                  break;
                      case posLoraQRG:
                                  MorsePreferences::loraQRG += (t*1E5);
                                  switch (MorsePreferences::loraBand) {
                                    case 0: MorsePreferences::loraQRG = constrain(MorsePreferences::loraQRG, 433.65E6, 434.55E6);
                                            break;
                                    case 1: MorsePreferences::loraQRG = constrain(MorsePreferences::loraQRG, 866.25E6, 869.45E6);
                                            break;
                                    case 2: MorsePreferences::loraQRG = constrain(MorsePreferences::loraQRG, 920.25E6, 923.15E6);
                                            break;
                                  }
                                  break;
                      case posSnapRecall:
                                  if (MorsePreferences::memCounter) 
                                      MorsePreferences::memPtr = (MorsePreferences::memPtr +t + MorsePreferences::memCounter + 1) % (MorsePreferences::memCounter+1);
                                  break;
                      case posSnapStore:
                                  MorsePreferences::memPtr = (MorsePreferences::memPtr + t + 9) % 9;
                                  break;
                      case posVAdjust:
                                  MorsePreferences::vAdjust += t;
                                  MorsePreferences::vAdjust = constrain(MorsePreferences::vAdjust, 155, 254);
                                  break;
                      case posHwConf:
                                  hwConf += (t+3);
                                  hwConf = hwConf % 3;
                                  break;
                      case posLoraPower:
                                  MorsePreferences::loraPower += t;                              // set the LoRa band
                                  MorsePreferences::loraPower = constrain(MorsePreferences::loraPower, 10, 20);
                                  break;
                  }
            }
            displayValueLine(pos, itemLine, false);          /// now display the value
            Heltec.display -> display();                                                      // update the display
         }      // end if     (checkEncoder)
         checkShutDown(false);         // check for time out
    }    // end while(true)
}   // end of function


void MorsePreferences::handleKochSequence() {
    MorsePreferences::useCustomChars = false;
    switch (MorsePreferences::pliste[posKochSeq].value) {
      case 3:                                                 // LICW
              handleCarouselChange();
              break;
      case 4:                                                 // Custom Chars
              MorsePreferences::useCustomChars = true;
      default:
              MorsePreferences::kochCharsLength = MorsePreferences::kochMaximum = 51;
              MorsePreferences::kochMinimum = 1;
              MorsePreferences::kochFilter = constrain(MorsePreferences::kochFilter, MorsePreferences::kochMinimum, MorsePreferences::kochMaximum);
              koch.setup();
    }
}

void MorsePreferences::handleCarouselChange() {
      MorsePreferences::kochCharsLength = MorsePreferences::kochMaximum = koch.setupLICWkochChars(MorsePreferences::pliste[posCarouselStart].value);
      MorsePreferences::kochMinimum = kochCharsLength > 18 ? 19 : 1;
//DEBUG("@ 842: kMin: " + String(MorsePreferences::kochMinimum) + " kMax: " + String(MorsePreferences::kochMaximum));
      MorsePreferences::kochFilter = constrain(MorsePreferences::kochFilter, MorsePreferences::kochMinimum, MorsePreferences::kochMaximum);
      koch.setup();
}


/////////////// READING and WRITING parameters from / into Non Volatile Storage, using ESP32 preferences

void MorsePreferences::readPreferences(String repository) {
  unsigned int l = 15;
  char repName[l];
  uint8_t temp;
  uint32_t tempInt;

  boolean morserino = false;

  if (repository == "morserino")
    morserino = true;

  repository.toCharArray(repName, l);
  // DEBUG("@851 Reading from repository: " + String(repName));
  // read preferences from non-volatile storage
  // if version cannot be read, we have a new ESP32 and need to write the preferences first

  if (morserino)
    pref.begin(repName, false);                // open namespace in read/write mode
  else
    pref.begin(repName, true);                 // read only in all other cases

  // DEBUG("@ l.861 Free entries: " + String(pref.freeEntries()));
    /// new code for reading preferences values - we check if we have a value, and if yes, we use it; if no, we use and write a default value
    /// some things we get from permanent memory, only when NOT restoring from a snapshot

    if (morserino) {                           // == NOT from snapshot
    
      MorsePreferences::wlanSSID = pref.getString("wlanSSID");
      MorsePreferences::wlanPassword = pref.getString("wlanPassword");
      MorsePreferences::wlanTRXPeer = pref.getString("wlanTRXPeer", "");

      MorsePreferences::wlanSSID1 = pref.getString("wlanSSID1");
      MorsePreferences::wlanPassword1 = pref.getString("wlanPassword1");
      MorsePreferences::wlanTRXPeer1 = pref.getString("wlanTRXPeer1", "");
      MorsePreferences::wlanSSID2 = pref.getString("wlanSSID2");
      MorsePreferences::wlanPassword2 = pref.getString("wlanPassword2");
      MorsePreferences::wlanTRXPeer2 = pref.getString("wlanTRXPeer2", "");
      MorsePreferences::wlanSSID3 = pref.getString("wlanSSID3");
      MorsePreferences::wlanPassword3 = pref.getString("wlanPassword3");
      MorsePreferences::wlanTRXPeer3 = pref.getString("wlanTRXPeer3", "");

      if ((temp = pref.getUChar("brightness")))
         MorsePreferences::oledBrightness = temp;

      if ((temp = pref.getUChar("version_major")) != MorsePreferences::version_major)
         pref.putUChar("version_major", MorsePreferences::version_major);
      if ((temp = pref.getUChar("version_minor")) != MorsePreferences::version_minor)
         pref.putUChar("version_minor", MorsePreferences::version_minor);

      if ((temp = pref.getUChar("kochFilter")))
         MorsePreferences::kochFilter = temp;
      else
         pref.putUChar("kochFilter", MorsePreferences::kochFilter);
         
      if ((temp = pref.getUChar("wpm")))
        MorsePreferences::wpm = temp;
      else if (morserino)
        pref.putUChar("wpm", MorsePreferences::wpm);

      if ((temp = pref.getUChar("sidetoneVolume",255)) != 255)
        MorsePreferences::sidetoneVolume = temp;
      else if (morserino)
        pref.putUChar("sidetoneVolume", MorsePreferences::sidetoneVolume);

      if (temp = pref.getUChar("vAdjust"))
        MorsePreferences::vAdjust = temp;

      if (temp = pref.getUChar("loraBand"))
          MorsePreferences::loraBand = temp;
      else
        MorsePreferences::loraBand = 0;

      if (tempInt = pref.getUInt("loraQRG"))
        MorsePreferences::loraQRG = tempInt;
      else
        MorsePreferences::loraQRG = QRG433;

      if (temp = pref.getUChar("loraPower"))
          MorsePreferences::loraPower = temp;
      else
          MorsePreferences::loraPower = 14;
          
      MorsePreferences::snapShots = pref.getUChar("snapShots",0);
      updateMemory(MorsePreferences::snapShots);

      MorsePreferences::fileWordPointer = pref.getUInt("fileWordPtr",0); // do not read fileWordPointer from other snapshots! we never write anything there!
    }  // endif morserino

    
    MorsePreferences::useCustomChars = pref.getBool("useCustomChar");
    MorsePreferences::customCharSet = pref.getString("customCharSet", "");
    if ((temp = pref.getUChar("lastExecuted"))) {
       MorsePreferences::menuPtr = temp;
    // DEBUG("@942 read: temp = " + String(temp));
    }
    
    if ((temp = pref.getUChar("kochCharsLength")))
       MorsePreferences::kochCharsLength = temp;
    else if (morserino)
       pref.putUChar("kochCharsLength", MorsePreferences::kochCharsLength);

//// now we read the preferences into memory that are also restored from snapshots (with two exception: posTimeOut and posSerialOut)
    
    for (uint8_t i = 0; i <= posSerialOut; ++i) {
      if (!morserino)
          if (i == posTimeOut || i == posSerialOut)
            continue;

      if ((temp = pref.getUChar(prefName[i],255)) != 255) {         // we have something in the repository
// DEBUG("@942 " + String(prefName[i]) + " : " + String(temp));
         if (i == posTimeOut && temp > 3)
            temp = 0;
         if (pliste[i].stepValue !=1)
            if (uint8_t d = temp % pliste[i].stepValue)             // we bring odd values in line with the step increment
              temp -= d;
         temp = constrain(temp, pliste[i].minimum, pliste[i].maximum);
         MorsePreferences::pliste[i].value = temp;
      }
      else if (morserino)
         pref.putUChar(prefName[i], MorsePreferences::pliste[i].value);
    }
   pref.end();
   handleKochSequence();
   updateTimings();
}

void MorsePreferences::writePreferences(String repository) {
  unsigned int l = 15;
  char repName[l];
  uint8_t temp;
  uint32_t tempInt;

  boolean morserino = false;

  if (repository == "morserino")
    morserino = true;
//DEBUG("Writing to repository: " + repository);
  repository.toCharArray(repName, l);

  pref.begin(repName, false);                // open namespace in read/write mode
  //DEBUG("@ l.969 Free entries: " + String(pref.freeEntries()));
  if (morserino) {                                                    // the following things are not stored in snapshots anymore, 
                                                                      //only in the ""Morserino" permanent memory
     pref.putUChar("brightness", MorsePreferences::oledBrightness);  // if not snapshots, store current screen brightness

     if (MorsePreferences::pliste[posSerialOut].value != pref.getUChar("serialOut")) {
         pref.remove("serialOut");
         pref.putUChar("serialOut", MorsePreferences::pliste[posSerialOut].value);
     }
         
     if (MorsePreferences::kochFilter != pref.getUChar("kochFilter")) { 
        pref.putUChar("kochFilter", MorsePreferences::kochFilter);
        if (!MorsePreferences::useCustomChars)                          // we update these only if we do not use a custom character set!
          koch.setup();
     }
          
     if (MorsePreferences::wlanSSID != pref.getString("wlanSSID")) {
          pref.remove("wlanSSID");
         pref.putString("wlanSSID", MorsePreferences::wlanSSID);
     }
     if (MorsePreferences::wlanPassword != pref.getString("wlanPassword"))
         pref.putString("wlanPassword", MorsePreferences::wlanPassword);
     if (MorsePreferences::wlanTRXPeer != pref.getString("wlanTRXPeer"))
         pref.putString("wlanTRXPeer", MorsePreferences::wlanTRXPeer);
 
     if (MorsePreferences::wlanSSID1 != pref.getString("wlanSSID1"))
         pref.putString("wlanSSID1", MorsePreferences::wlanSSID1);
     if (MorsePreferences::wlanPassword1 != pref.getString("wlanPassword1"))
         pref.putString("wlanPassword1", MorsePreferences::wlanPassword1);
     if (MorsePreferences::wlanTRXPeer1 != pref.getString("wlanTRXPeer1"))
         pref.putString("wlanTRXPeer1", MorsePreferences::wlanTRXPeer1);
 
     if (MorsePreferences::wlanSSID2 != pref.getString("wlanSSID2"))
         pref.putString("wlanSSID2", MorsePreferences::wlanSSID2);
     if (MorsePreferences::wlanPassword2 != pref.getString("wlanPassword2"))
         pref.putString("wlanPassword2", MorsePreferences::wlanPassword2);
     if (MorsePreferences::wlanTRXPeer2 != pref.getString("wlanTRXPeer2"))
         pref.putString("wlanTRXPeer2", MorsePreferences::wlanTRXPeer2);
 
     if (MorsePreferences::wlanSSID3 != pref.getString("wlanSSID3"))
         pref.putString("wlanSSID3", MorsePreferences::wlanSSID3);
     if (MorsePreferences::wlanPassword3 != pref.getString("wlanPassword3"))
         pref.putString("wlanPassword3", MorsePreferences::wlanPassword3);
     if (MorsePreferences::wlanTRXPeer3 != pref.getString("wlanTRXPeer3"))
         pref.putString("wlanTRXPeer3", MorsePreferences::wlanTRXPeer3);

  } else {
      pref.remove("wlanSSID");
      pref.remove("wlanPassword");
      pref.remove("wlanTRXPeer");
      pref.remove("wlanSSID1");
      pref.remove("wlanPassword1");
      pref.remove("wlanTRXPeer1");
      pref.remove("wlanSSID2");
      pref.remove("wlanPassword2");
      pref.remove("wlanTRXPeer2");
      pref.remove("wlanSSID3");
      pref.remove("wlanPassword3");
      pref.remove("wlanTRXPeer3");

      pref.remove("lastExecuted");    // This is ONLY written to snapshots here (a separate function is used to store it in normal permanent memory)
      pref.putUChar("lastExecuted", MorsePreferences::menuPtr);   // store last executed command in snapshots
      //DEBUG("@1051: lastExecuted: " + String(pref.getUChar("lastExecuted")));
  }

  

  // now we write all other preferences into the respective repository

  if (MorsePreferences::useCustomChars != pref.getBool("useCustomChar")) {
      pref.remove("useCustomChar");
      pref.putBool("useCustomChar", MorsePreferences::useCustomChars);
  }
  if (MorsePreferences::customCharSet != pref.getString("customCharSet")) {
      pref.remove("customCharSet");
      pref.putString("customCharSet", MorsePreferences::customCharSet);
      koch.setup();
  }
  if (MorsePreferences::kochCharsLength != pref.getUChar("kochCharsLength")) {
      pref.remove("kochCharsLength");
      pref.putUChar("kochCharsLength", MorsePreferences::kochCharsLength);
      koch.setup();
  }


  for (uint8_t i = 0; i <= posSerialOut; ++i) {                                       // for all these preferences
        if (i == posTimeOut && !morserino)                                            // ignore timeout when writing to snapshot
            continue; 
        if (MorsePreferences::pliste[i].value != pref.getUChar(prefName[i],255) ) {     // stored value is different, 
//DEBUG("@1062 " + String(prefName[i]) + " old: " + String(pref.getUChar(prefName[i],255)) + " new: " + String(MorsePreferences::pliste[i].value));
            pref.putUChar(prefName[i], MorsePreferences::pliste[i].value);            // so we need to store new value
            switch (i) {                                                              // in certain cases we need to do something
              case posLoraChannel:
                    if (morserino)
                      LoRa.setSyncWord(MorsePreferences::pliste[posLoraChannel].value == 0 ? 0x27 : 0x66);
                    break;
              case posGoertzelBandwidth:
                    if (morserino)
                      Goertzel::setup();
                    break;
              case posKochSeq:
                    if (morserino && !MorsePreferences::useCustomChars)
                      koch.setup();
                    break;
              case posAbbrevLength:
              case posWordLength:
              case posCarouselStart:
                    if (morserino)
                      koch.setup();
                   break;
            }     // end of "special cases"
      }           // end of "stored value is different"
  }               // end of "for all these preferences"
  updateTimings();

  pref.end();
// DEBUG("end l. 1087");
}


boolean  MorsePreferences::recallSnapshot() {         // return true if we selected a real recall, false when it was cancelled
    //String snapname;
    String text;

    MorsePreferences::memPtr = 0;
    displayKeyerPreferencesMenu(posSnapRecall);
    if (!adjustKeyerPreference(posSnapRecall)) {
        //DEBUG("recall memPtr: " + String(memPtr));
        text = "Snap " + String(MorsePreferences::memories[MorsePreferences::memPtr]+1) + " RECALLD";
        if(MorsePreferences::memCounter) {
          if (MorsePreferences::memPtr != MorsePreferences::memCounter)  {
            doReadSnapshot(MorsePreferences::memPtr);
            MorseOutput::printOnScroll(2, BOLD, 0, text);
            if (m32protocol)
              jsonCreate("message", text, "");
            delay(1000);
            return true;
          }
          return false;
        }
    } return false;

}


void MorsePreferences::doReadSnapshot(uint8_t storePos) {
    String snapname; snapname.reserve(8);
    snapname = "snap" + String(MorsePreferences::memories[storePos]);
    readPreferences(snapname);
}



boolean MorsePreferences::storeSnapshot(uint8_t menu) {        // return true if we selected a real store, false when it was cancelled
    String text; text.reserve(20);

    MorsePreferences::memPtr = 0;
    displayKeyerPreferencesMenu(posSnapStore);
    adjustKeyerPreference(posSnapStore);
    Buttons:: volButton.Update();
        //DEBUG("store memPtr: " + String(memPtr));
    if (MorsePreferences::memPtr != 8)  {
        doWriteSnapshot(memPtr, menu);
        text = "Snap " + String(MorsePreferences::memPtr+1) + " STORED ";
        if (m32protocol)
          jsonCreate("message", text, "");
        MorseOutput::printOnScroll(2, BOLD, 0, text);
        delay(1000);
        return true;
      }
      return false;
}

void MorsePreferences::doWriteSnapshot(uint8_t storePos, uint8_t menuPos) {
      uint8_t mask = 1;
      String snapname; snapname.reserve(8);

      MorsePreferences::menuPtr = menuPos;     // also store last menu selection
      snapname = "snap" + String(storePos);
      writePreferences(snapname);
      /// insert the correct bit into p_snapShots & update memory variables
      mask = mask << storePos;
      MorsePreferences::snapShots = MorsePreferences::snapShots | mask;
      updateMemory(MorsePreferences::snapShots);
      pref.begin("morserino", false);             // open the namespace as read/write
      pref.remove("snapShots");
      pref.putUChar("snapShots", MorsePreferences::snapShots);
      pref.end();  
}


void MorsePreferences::updateMemory(uint8_t temp) {         // temp is a bitmap, 1 byte long, with a bit set for each existing snapshot
  uint8_t t = temp;
  MorsePreferences::memCounter = 0;                                           // create / update an array  indicating the snapshot numbers  (memories) that are in use
        for (uint8_t i = 0; i<8; ++i) {
          if (t & 1) {     // mask rightmost bit
            MorsePreferences::memories[MorsePreferences::memCounter] = i;
            ++MorsePreferences::memCounter;
          }
          t = t >> 1;   // shift one position to the right
        }
 //DEBUG("update Memory: positions = " + String(MorsePreferences::memCounter));     
}

void MorsePreferences::clearMemory(uint8_t ptr) {
  String text; text.reserve(24);

  text = doClearMemory(ptr);
    
  if (m32protocol)                                                            // output to m32protocol and screen
      jsonCreate("message", text, "");
  MorseOutput::printOnScroll(2, BOLD, 0, text);
  delay(900);
}

String MorsePreferences::doClearMemory(uint8_t ptr) {
  String snapName; snapName.reserve(12);
  String text ; text.reserve(24);
  uint8_t snapNumber;
  const unsigned int l = 12;
  char repName[l];

  snapNumber = MorsePreferences::memories[ptr];
  snapName = "snap" + String(snapNumber);
  text = "Snap " + String(snapNumber+1) + " CLEARED";
  snapName.toCharArray(repName, l);

  pref.begin(repName, false);           //// open namespace that will be cleared
  pref.clear();
  pref.end();
  
  MorsePreferences::snapShots &= ~(1 << MorsePreferences::memories[ptr]);     // clear the bit in MorsePreferences::snapShots
  updateMemory(MorsePreferences::snapShots);                                  // and update the array of snapshot numbers

  pref.begin("morserino", false);                                             // open the namespace as read/write
  pref.remove("snapShots");
  pref.putUChar("snapShots", MorsePreferences::snapShots);                    // wite the new value of the bitmap snapshots into permanent storage
  pref.end();  

  return text;
}

///////// read & write board version into NVS memory

void MorsePreferences::determineBoardVersion() {
    pref.begin("morserino", false);             // open the namespace as read/write
    MorsePreferences::boardVersion = pref.getUChar("boardVersion");
    delay(1000);
    if (MorsePreferences::boardVersion == 0) {                    // no board version had been set previously, so we determine and set it here
        const int oldbatt = 13;                                     // we measure voltage at pin 13; if V close to zero we have a 2.1 Heltec, so board 4

        analogSetAttenuation(ADC_0db);
        adcAttachPin(oldbatt);
        analogSetClockDiv(128);           //  this value was found by experimenting - no clue what it really does :-(
        analogSetPinAttenuation(oldbatt,ADC_11db);
        if(analogRead(oldbatt) > 1023) {
          MorsePreferences::boardVersion = 3;
          // DEBUG("boardV: 3");
        }
        else {MorsePreferences::boardVersion = 4; //DEBUG("boardV: 4");
        }
        pref.putUChar("boardVersion", MorsePreferences::boardVersion);
    }
    pref.end();
}


//////// System Setup / LoRa Setup ///// Called when BALCK knob is pressed @ startup

void MorsePreferences::loraSystemSetup() {
    MorsePreferences::displayKeyerPreferencesMenu(posLoraBand);
    MorsePreferences::adjustKeyerPreference(posLoraBand);
    MorsePreferences::displayKeyerPreferencesMenu(posLoraQRG);
    MorsePreferences::adjustKeyerPreference(posLoraQRG);
    MorsePreferences::displayKeyerPreferencesMenu(posLoraPower);
    MorsePreferences::adjustKeyerPreference(posLoraPower);
    /// now store chosen values in Preferences
    pref.begin("morserino", false);             // open the namespace as read/write
    pref.putUChar("loraBand", MorsePreferences::loraBand);
    pref.putUInt("loraQRG", MorsePreferences::loraQRG);
    pref.putUChar("loraPower", MorsePreferences::loraPower);
    pref.end();
}

/////// System Setup / Battery Measurement Calibration  /////// called from system config (black knob at start-up)
void MorsePreferences::calibrateVoltageMeasurement() {
  //voltage_raw = volt / 19.2 * MorsePreferences::vAdjust;
  //DEBUG("v_raw: " + String(voltage_raw));
  MorsePreferences::displayKeyerPreferencesMenu(posVAdjust);
  MorsePreferences::adjustKeyerPreference(posVAdjust);
  pref.begin("morserino", false);             // open the namespace as read/write
  //DEBUG("Store " + String(MorsePreferences::vAdjust));
  pref.putUChar("vAdjust", MorsePreferences::vAdjust);
  pref.end();
}


void MorsePreferences::fireCharSeen(boolean wpmOnly)
{
    pref.begin("morserino", false);             // open the namespace as read/write
    pref.putUChar("wpm", MorsePreferences::wpm);
    if (!wpmOnly)
    {
        pref.putUChar("tLeft", MorsePreferences::tLeft);
        pref.putUChar("tRight", MorsePreferences::tRight);
    }
    pref.end();
}

void MorsePreferences::writeWordPointer()
{
    pref.begin("morserino", false);              // open the namespace as read/write
    if ((MorsePreferences::fileWordPointer != pref.getUInt("fileWordPtr")))
    {   // update word pointer if necessary (if we ran player before)
        pref.putUInt("fileWordPtr", MorsePreferences::fileWordPointer);
    }
    pref.end();

}

void MorsePreferences::writeVolume()
{
    pref.begin("morserino", false);                     // open the namespace as read/write
    if (pref.getUChar("sidetoneVolume") != MorsePreferences::sidetoneVolume)
        pref.putUChar("sidetoneVolume", MorsePreferences::sidetoneVolume);  // store the last volume, if it has changed
    pref.end();
}

void MorsePreferences::writeLastExecuted(uint8_t menuPtr)
{
    pref.begin("morserino", false);             // open the namespace as read/write
    pref.putUChar("lastExecuted", menuPtr);   // store last executed command
    pref.end();                                 // close namespace
}

void MorsePreferences::writeWifiInfoMultiple(
  String ssid1, String passwd1, String trxpeer1,
  String ssid2, String passwd2, String trxpeer2,
  String ssid3, String passwd3, String trxpeer3
  )
{
    pref.begin("morserino", false);             // open the namespace as read/write

    if (ssid1 != "")
      MorsePreferences::wlanSSID1 = ssid1;
    if (passwd1 != "")
      MorsePreferences::wlanPassword1 = passwd1;
    //if (trxpeer != "")
      MorsePreferences::wlanTRXPeer1 = trxpeer1;

    if (MorsePreferences::wlanSSID1 != pref.getString("wlanSSID1"))
        pref.putString("wlanSSID1", MorsePreferences::wlanSSID1);
    if (MorsePreferences::wlanPassword1 != pref.getString("wlanPassword1"))
        pref.putString("wlanPassword1", MorsePreferences::wlanPassword1);
    if (MorsePreferences::wlanTRXPeer1 != pref.getString("wlanTRXPeer1"))
        pref.putString("wlanTRXPeer1", MorsePreferences::wlanTRXPeer1);

    if (ssid2 != "")
      MorsePreferences::wlanSSID2 = ssid2;
    if (passwd2 != "")
      MorsePreferences::wlanPassword2 = passwd2;
    //if (trxpeer != "")
      MorsePreferences::wlanTRXPeer2 = trxpeer2;

    if (MorsePreferences::wlanSSID2 != pref.getString("wlanSSID2"))
        pref.putString("wlanSSID2", MorsePreferences::wlanSSID2);
    if (MorsePreferences::wlanPassword2 != pref.getString("wlanPassword2"))
        pref.putString("wlanPassword2", MorsePreferences::wlanPassword2);
    if (MorsePreferences::wlanTRXPeer2 != pref.getString("wlanTRXPeer2"))
        pref.putString("wlanTRXPeer2", MorsePreferences::wlanTRXPeer2);

    if (ssid3 != "")
      MorsePreferences::wlanSSID3 = ssid3;
    if (passwd3 != "")
      MorsePreferences::wlanPassword3 = passwd3;
    //if (trxpeer != "")
      MorsePreferences::wlanTRXPeer3 = trxpeer3;

    if (MorsePreferences::wlanSSID3 != pref.getString("wlanSSID3"))
        pref.putString("wlanSSID3", MorsePreferences::wlanSSID3);
    if (MorsePreferences::wlanPassword3 != pref.getString("wlanPassword3"))
        pref.putString("wlanPassword3", MorsePreferences::wlanPassword3);
    if (MorsePreferences::wlanTRXPeer3 != pref.getString("wlanTRXPeer3"))
        pref.putString("wlanTRXPeer3", MorsePreferences::wlanTRXPeer3);

    pref.end();

    writeWifiInfo(ssid1, passwd1, trxpeer1);
}

void MorsePreferences::writeWifiInfo(String ssid, String passwd, String trxpeer)
{
    if (ssid != "")
      MorsePreferences::wlanSSID = ssid;
    if (passwd != "")
      MorsePreferences::wlanPassword = passwd;
    //if (trxpeer != "")
      MorsePreferences::wlanTRXPeer = trxpeer;
    pref.begin("morserino", false);             // open the namespace as read/write
    if (MorsePreferences::wlanSSID != pref.getString("wlanSSID"))
        pref.putString("wlanSSID", MorsePreferences::wlanSSID);
    if (MorsePreferences::wlanPassword != pref.getString("wlanPassword"))
        pref.putString("wlanPassword", MorsePreferences::wlanPassword);
    if (MorsePreferences::wlanTRXPeer != pref.getString("wlanTRXPeer"))
        pref.putString("wlanTRXPeer", MorsePreferences::wlanTRXPeer);

    pref.end();

}

void MorsePreferences::setCurrentOptions(prefPos *current, int size) {
  MorsePreferences::currentOptions = current;
  currentOptionSize = size;
}

//////// methods for class Koch ///////////////////////////////////////////////////////

Koch::Koch() {
}

void Koch::createWords(uint8_t maxl, uint8_t koch) {                  // this function creates an array of words that are compliant to Koch filter and max word length
  numberOfWords = 0;
  //DEBUG("ptr: " + String(maxl));
  //DEBUG("koch: " + String(koch));
  maxl = (maxl == 0 ? 0 : maxl+1);
  for (int i = EnglishWords::WORDS_POINTER[maxl]; i< EnglishWords::WORDS_NUMBER_OF_ELEMENTS; ++i) {     // do this for all words with max length maxl
      if (wordIsKoch(EnglishWords::words[i]) <= koch) {
          wordIndices[numberOfWords++] = i;
      }
  }
}

void Koch::createAbbr(uint8_t maxl, uint8_t koch) {                  // this function creates an array of abbrevs that are compliant to Koch filter and max word length
  numberOfAbbr = 0;
  maxl = (maxl == 0 ? 0 : maxl+1);

  //DEBUG("ptr: " + String(maxl));
  for (int i = Abbrev::ABBREV_POINTER[maxl]; i< Abbrev::ABBREV_NUMBER_OF_ELEMENTS; ++i) {     // do this for all words with max length maxl
    // DEBUG(Abbrev::abbreviations[i]);
      if (wordIsKoch(Abbrev::abbreviations[i]) <= koch)
          abbrIndices[numberOfAbbr++] = i;
  }
}

uint8_t Koch::wordIsKoch(String thisWord) {   /// returns the highest Koch sequence number of a word; if it is not in the charset, return a high number
  uint8_t thisKoch = 0;
  int index;
  uint8_t l = thisWord.length();
  String charSet;
  charSet.reserve(53);

  charSet = MorsePreferences::useCustomChars ? MorsePreferences::customCharSet : kochCharSet;
  //DEBUG("set: " + charSet + " word: " + thisWord);
  for ( int i = 0; i< l; ++i) {
    index = charSet.indexOf(thisWord.charAt(i))+1;
    if (index == 0)
      index = kochCharsLength + 1;
    thisKoch = _max(thisKoch, index);
    //DEBUG("thisKoch: " + String(thisKoch));
  }
  return thisKoch;
}


void Koch::setup() {                                                // create the Koch tables according to custom char or Koch filter, and length
  if (MorsePreferences::useCustomChars)
    setCustomChars(MorsePreferences::customCharSet);
  else
    setKochChars(MorsePreferences::pliste[posKochSeq].value);
  //// populate the array for abbreviations and words according to length and Koch filter
  createWords(MorsePreferences::pliste[posWordLength].value, MorsePreferences::useCustomChars ? kochCharsLength+1 : MorsePreferences::kochFilter) ;  //
  createAbbr(MorsePreferences::pliste[posAbbrevLength].value, MorsePreferences::useCustomChars ? kochCharsLength+1 : MorsePreferences::kochFilter);

  String charSet = getCharSet();
  uint8_t probability = 0;
  for (int i = 0; i < kochCharsLength; i++)
  {
    if (i >= charSet.length())
      probability = 0;
    else if (i > charSet.length() - 6)
      probability = 7 + i - charSet.length();
    else
      probability = 1;

    adaptiveProbabilities[i] = probability;
  }

  initSequence = getRandomCharSet();
  initSequenceIndex = 0;
}

void Koch::setKochChars(uint8_t sequence) { // define the demanded Koch character set: Koch sequenze: 0 = native/JLMC, 1 = LCWO, 2 = CW Academy, 3 = LICW, 4 = Custom
    switch (sequence) {
        case 1: kochCharSet = lcwoKochChars;
                break;
        case 2: kochCharSet = cwacKochChars;
                break;
        case 3: setupLICWkochChars(MorsePreferences::pliste[posCarouselStart].value);
                kochCharSet = licwKochChars;
                break;
        default:kochCharSet = morserinoKochChars;
                break;
    }
}



uint8_t Koch::setupLICWkochChars(uint8_t start) {                              // set up the string of characters used for LICW carousel Koch training, return length
    // depends on: carouselStart
    //uint8_t start = MorsePreferences::pliste[posCarouselStart].value;
    uint8_t l;
    String temp;
    temp.reserve(45);
    if (start < 6)      // BC 1
      {
        temp = licwAllKochChars.substring(3*start,18) + (start > 0 ? licwAllKochChars.substring(0, 3*start) : "");
        l = 18;
      }
      else              // BC 2
      {
        temp = licwAllKochChars.substring(0,18) + licwAllKochChars.substring(3*start,44) +
                        (start > 6 ? licwAllKochChars.substring(18, 3*start) : "");
        l = 44;
      }
    // DEBUG("Start: " + String(start) + "licwKochChars: " + temp);
    licwKochChars = temp;
    return l;
}

void Koch::setCustomChars(String chars) {          // define the custom character set
   MorsePreferences::customCharSet = chars;
}

String Koch::getNewChar() {                     // for Koch Learn New Character
  return String(kochCharSet.charAt(MorsePreferences::kochFilter - 1));
}

String Koch::getKochChar(uint8_t i) {           // get a String consisting of a single character at pos i in kochCharSet (i starting with 0)
  return String(kochCharSet.charAt(i));
}

String Koch::getRandomChar(int maxl) {                  // get a random character word for Koch, with max length maxl
    String result = "";
    result.reserve(7);

    if (MorsePreferences::useCustomChars) {
        for (int i = 0; i < maxl; ++i )
            result += MorsePreferences::customCharSet.charAt(random(MorsePreferences::customCharSet.length()));
    }
    else {
        int endk =  MorsePreferences::kochFilter;               // kochChars = "mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=KA+SNE@:"
        for (int i = 0; i < maxl; ++i) {                        //              1   5    1    5    2    5    3    5    4    5    5
            if (random(3))                                      // in Koch mode, we generate the last third of the chars learned  a bit more often
                result += kochCharSet.charAt(random(endk));
            else
                result += kochCharSet.charAt(random(2*endk/3, endk));
        }
    }
    return result;
}

String Koch::getRandomWord() {                       // get a random english word for Koch (preselected with max length and Koch filter (or custom character filter)
  if (numberOfWords == 0)
    return getRandomChar(1);

  uint16_t index = wordIndices[random(numberOfWords)];
  return EnglishWords::words[index];
}

String Koch::getRandomAbbrev() {
    if (numberOfAbbr == 0)
    return getRandomChar(1);

    uint16_t index = abbrIndices[random(numberOfAbbr)];
    return Abbrev::abbreviations[index];
}

String Koch::getAdaptiveChar(int maxl) {
  String result = getInitChar(maxl);
  String charSet = getCharSet();
  result.reserve(7);
  int16_t probabilitySum = getProbabilitySum();

  while (result.length() < maxl)
  {
    int16_t randomOffset = random(probabilitySum);

    for (int j = 0; j < charSet.length(); j++)
    {
      randomOffset -= adaptiveProbabilities[j];
      if (randomOffset < 0)
      {
        result += charSet.charAt(j);
        break;
      }
    }
  }

  return result;
}

int16_t Koch::getProbabilitySum()
{
  int16_t sum = 0;
  for (int i = 0; i < kochCharsLength; i++)
  {
    sum += adaptiveProbabilities[i];
  }
  return sum;
}

String Koch::getInitChar(int maxl)
{
  String result;

  if (initSequenceIndex >= initSequence.length())
    return result;

  result.reserve(maxl);

  while (result.length() < maxl && initSequenceIndex < initSequence.length())
  {
    result += initSequence.charAt(initSequenceIndex);
    initSequenceIndex++;
        }

  return result;
}

String Koch::getCharSet()
{
  if (MorsePreferences::useCustomChars)
    return MorsePreferences::customCharSet;
  else
    return kochCharSet.substring(0, MorsePreferences::kochFilter);
}

String Koch::getRandomCharSet()
{
  String charSet = getCharSet();
  String randomCharSet;
  randomCharSet.reserve(charSet.length());

  for (int i = 0; i < charSet.length(); i++)
  {
    int charIndex = random(charSet.length() - i);
    randomCharSet += charSet.charAt(charIndex);
    charSet.setCharAt(charIndex, charSet.charAt(charSet.length() - i - 1));
  }

  return randomCharSet;
}

void Koch::increaseWordProbability(String& expected, String& received)
{
  int failedIndex = getFailedCharIndex(expected, received);
  if (failedIndex == -1)
    return;

  increaseCharProbability(expected.charAt(failedIndex), 4);

  if (failedIndex > 0 && expected.charAt(failedIndex - 1) != expected.charAt(failedIndex))
    increaseCharProbability(expected.charAt(failedIndex - 1), 2);
  if ((failedIndex + 1) < expected.length() && expected.charAt(failedIndex + 1) != expected.charAt(failedIndex))
    increaseCharProbability(expected.charAt(failedIndex + 1), 2);
}

int Koch::getFailedCharIndex(String& expected, String& received)
{
  for (int i = 0; i < expected.length(); i++)
  {
    if (i >= received.length())
      return i;

    if (expected.charAt(i) != received.charAt(i))
      return i;
  }

  return -1;
}

void Koch::increaseCharProbability(char c, uint8_t count)
{
  String charSet = getCharSet();
  int increaseIndex = charSet.indexOf(c);

  adaptiveProbabilities[increaseIndex] += count;

  if (adaptiveProbabilities[increaseIndex] > charSet.length())
    adaptiveProbabilities[increaseIndex] = charSet.length();
}

void Koch::decreaseWordProbability(String& word)
{
  for (int i = 0; i < word.length(); i++)
  {
    decreaseCharProbability(word.charAt(i));
  }
}

void Koch::decreaseCharProbability(char c)
{
  String charSet = getCharSet();
  int decreaseIndex = charSet.indexOf(c);

  if (adaptiveProbabilities[decreaseIndex] > 1)
    adaptiveProbabilities[decreaseIndex]--;
}
