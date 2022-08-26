#ifndef PREFS_H
#define PREFS_H

#include <Arduino.h>
#include <Preferences.h>   // ESP 32 library for storing things in non-volatile storage


#include "ClickButton.h"   // button control library
#include "morsedefs.h"
#include "abbrev.h"
#include "english_words.h"
#include "goertzel.h"

const int kochCharsLength = 50;

extern int8_t hwConf;

extern String cleanUpProSigns( String &input );
//extern int16_t batteryVoltage();
//extern int16_t volt;
extern void updateTimings();
extern String cleanUpProSigns( String &input );
//extern void setupGoertzel();
extern int IRAM_ATTR checkEncoder();

extern void keyOut(boolean,  boolean, int, int);
extern void checkShutDown(boolean);
extern void cleanStartSettings();

extern String getCustomChars();

namespace MorsePreferences
{
  // the preferences variable and their defaults
  
  extern uint8_t version_major;
  extern uint8_t version_minor;
  extern uint8_t sidetoneFreq;
  extern uint8_t sidetoneVolume;
  extern boolean didah;
  extern uint8_t keyermode;
  extern uint8_t interCharSpace;
  extern boolean reversePolarity;
  extern uint8_t ACSlength;

  extern boolean encoderClicks;
  extern uint8_t randomLength;
  extern uint8_t randomOption;
  extern uint8_t callLength;
  extern uint8_t abbrevLength;
  extern uint8_t wordLength;
  extern uint8_t trainerDisplay;
  extern uint8_t curtisBTiming;
  extern uint8_t curtisBDotTiming;
  extern uint8_t interWordSpace;
  
  extern uint8_t echoRepeats;
  extern uint8_t echoDisplay;
  extern uint8_t kochFilter;
  extern boolean wordDoubler;
  extern uint8_t echoToneShift;
  extern boolean echoConf;
  extern uint8_t keyTrainerMode;
  extern uint8_t loraTrainerMode;
  extern uint8_t goertzelBandwidth;
  extern boolean speedAdapt;
  extern uint8_t latency;
  extern uint8_t randomFile;
  extern boolean lcwoKochSeq;
  extern boolean cwacKochSeq;
  extern boolean extAudioOnDecode;
  extern uint8_t timeOut;
  extern boolean quickStart;
  extern boolean autoStopMode;
  extern uint8_t loraSyncW;
  extern uint8_t maxSequence;
  extern uint8_t serialOut;

  extern boolean useCustomCharSet;
  extern String  customCharSet;
  
  ///// stored in preferences, but not adjustable through preferences menu:
  extern uint8_t responsePause;
  extern uint8_t wpm;
  extern uint8_t menuPtr;
  //current network config
  extern String  wlanSSID;
  extern String  wlanPassword;
  extern String  wlanTRXPeer; 

  // config for up to three networks
  extern String  wlanSSID1;
  extern String  wlanPassword1;
  extern String  wlanTRXPeer1; 
  
  extern String  wlanSSID2;
  extern String  wlanPassword2;
  extern String  wlanTRXPeer2; 
  
  extern String  wlanSSID3;
  extern String  wlanPassword3;
  extern String  wlanTRXPeer3; 

  extern uint32_t fileWordPointer;
  extern uint8_t promptPause;
  extern uint8_t tLeft;
  extern uint8_t tRight;
  extern uint8_t vAdjust;
  extern uint8_t loraBand;
 #define QRG433 434.15E6
 #define QRG866 869.15E6
 #define QRG920 920.55E6
  extern uint32_t loraQRG;
  extern uint8_t snapShots;
  extern uint8_t boardVersion;
  extern uint8_t oledBrightness;
  
  ////// end of variables stored in preferences
  
  //// for adjusting preferences
  
  
  enum prefPos  { posClicks, posPitch, posExtPaddles, posPolarity,
                  posCurtisMode, posCurtisBDahTiming, posCurtisBDotTiming, posACS,
                  posEchoToneShift, posInterWordSpace, posInterCharSpace, posRandomOption,
                  posRandomLength, posCallLength, posAbbrevLength, posWordLength,
                  posTrainerDisplay, posWordDoubler, posEchoDisplay, posEchoRepeats,  posEchoConf,
                  posKeyTrainerMode, posLoraTrainerMode, posGoertzelBandwidth, posSpeedAdapt,
                  posKochSeq, posKochFilter, posLatency, posRandomFile, posExtAudioOnDecode, posTimeOut, 
                  posQuickStart, posAutoStop,posMaxSequence, posLoraSyncW,   posSerialOut,
                  posLoraBand, posLoraQRG, posSnapRecall, posSnapStore,  posVAdjust, posHwConf
                };
  
  extern const String prefOption[];
  extern  prefPos keyerOptions[];
  extern  prefPos generatorOptions[];
  extern  prefPos playerOptions[];
  extern  prefPos echoPlayerOptions[];
  extern  prefPos echoTrainerOptions[];
  extern  prefPos kochGenOptions[];
  extern  prefPos kochEchoOptions[];
  extern  prefPos loraTrxOptions[];
  extern  prefPos wifiTrxOptions[];
  extern  prefPos extTrxOptions[];
  extern  prefPos decoderOptions[];
  extern  prefPos allOptions[];
  
  extern prefPos *currentOptions;

  extern int keyerOptionsSize;
  extern int generatorOptionsSize;
  extern int playerOptionsSize;
  extern int echoPlayerOptionsSize;
  extern int echoTrainerOptionsSize;
  extern int kochGenOptionsSize;
  extern int kochEchoOptionsSize;
  extern int loraTrxOptionsSize;
  extern int wifiTrxOptionsSize;
  extern int extTrxOptionsSize;
  extern int decoderOptionsSize;
  extern int allOptionsSize;
  
 
    
  void updateMemory(uint8_t temp);
  void clearMemory(uint8_t ptr);
  boolean  recallSnapshot();
  boolean storeSnapshot(uint8_t menu);
  boolean setupPreferences(uint8_t atMenu);
  void displayKeyerPreferencesMenu(int pos);
  boolean adjustKeyerPreference(prefPos pos);
  void readPreferences(String repository);
  void writePreferences(String repository);
  void createKochWords(uint8_t maxl, uint8_t koch);
  uint8_t wordIsKoch(String thisWord);
  void createKochAbbr(uint8_t maxl, uint8_t koch);
  void setKochChars(boolean lcwo);
  void setCustomChars(String);
  void kochSetup();
  void loraSystemSetup();
  void determineBoardVersion();
  void calibrateVoltageMeasurement();
  void writeWordPointer();
  void writeVolume();
  void writeLastExecuted(uint8_t menuPtr);
  void writeWifiInfo(String, String, String);
  void writeWifiInfoMultiple(String, String, String, String, String, String, String, String, String);
  void fireCharSeen(boolean wpmOnly);
  void setCurrentOptions(prefPos *current, int size);
}

namespace internal {
  void displayCurtisMode();
  void displayCurtisBTiming();
  void displayCurtisBDotTiming();
  void displayACS();
  void displayPitch();
  void displayClicks();
  void displayUseStraightKey();
  void displayExtPaddles();
  void displayPolarity();
  void displayLatency();
  void displayInterWordSpace();
  void displayInterCharSpace();
  void displayRandomOption();
  void displayRandomLength();
  void displayCallLength();
  void displayAbbrevLength();
  void displayWordLength();
  void displayMaxSequence();
  void displayTrainerDisplay();
  void displayEchoDisplay();
  void displayKeyTrainerMode();
  void displayLoraTrainerMode();
  void displayLoraSyncW();
  void displayEchoRepeats();
  void displayEchoToneShift();
  void displayEchoConf();
  void displayKochFilter();
  void displayWordDoubler();
  void displayRandomFile();
  void displayGoertzelBandwidth();
  void displaySpeedAdapt();
  void displayKochSeq();
  void displayExtAudioOnDecode();
  void displayTimeOut();
  void displayQuickStart();
  void displayAutoStop();
  void displaySerialOut();
  void displayVAdjust();
  void displayLoraBand();
  void displayLoraQRG();
  void displaySnapRecall();
  void displaySnapStore();
  void displayHwConf();
}

class Koch {
  private:
    uint16_t wordIndices[EnglishWords::WORDS_NUMBER_OF_ELEMENTS];
    uint16_t numberOfWords;
    uint16_t abbrIndices[Abbrev::ABBREV_NUMBER_OF_ELEMENTS];
    uint16_t numberOfAbbr;
    String kochCharSet;
    const String lcwoKochChars =      "kmuresnaptlwi.jz=foy,vg5/q92h38b?47c1d60x-K+ASNE@:";
    const String cwacKochChars =      "teanois14rhdl25ucmw36?fypg79/bvkj80=xqz.,-K+ASNE@:";
    void createWords(uint8_t, uint8_t);
    void createAbbr(uint8_t, uint8_t);
    uint8_t wordIsKoch(String);

    uint8_t adaptiveProbabilities[kochCharsLength];
    String initSequence;
    uint8_t initSequenceIndex;

  public:
    const String morserinoKochChars = "mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=K+SNAE@:";

    Koch();
    void setup();
    String getNewChar();
    String getRandomChar(int);
    String getRandomWord();
    String getRandomAbbrev();
    String getAdaptiveChar(int);
    String getCharSet();
    String getRandomCharSet();
    String getInitChar(int);
    void setKochChars(boolean, boolean);
    void setCustomChars(String chars);
    int16_t getProbabilitySum();
    void increaseWordProbability(String& expected, String& received);
    int getFailedCharIndex(String& expected, String& received);
    void increaseCharProbability(char c, uint8_t count);
    void decreaseWordProbability(String& word);
    void decreaseCharProbability(char c);
};

extern Koch koch;

#endif
