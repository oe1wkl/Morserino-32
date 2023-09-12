#ifndef PREFS_H
#define PREFS_H

#include <Arduino.h>
#include <Preferences.h>   // ESP 32 library for storing things in non-volatile storage


#include "ClickButton.h"   // button control library
#include "morsedefs.h"
#include "MorseMenu.h"
#include "abbrev.h"
#include "english_words.h"
#include "goertzel.h"


extern boolean m32protocol;
extern int8_t hwConf;
extern loops m32state;
extern boolean goToMenu;

extern String cleanUpProSigns( String &input );
//extern int16_t batteryVoltage();
//extern int16_t volt;
extern void updateTimings();
extern String cleanUpProSigns( String &input );

extern int IRAM_ATTR checkEncoder();

extern void keyOut(boolean,  boolean, int, int);
extern void checkShutDown(boolean);
extern void cleanStartSettings();
extern void serialEvent();
extern void jsonCreate(String, String,String);
extern void jsonConfigShort(String,int, String);

extern void jsonError(String);
extern void jsonActivate(actMessage);
extern void jsonMenu(String,unsigned int,bool,bool);
extern String getCustomChars();


namespace MorsePreferences
{
  //String extraItems[8];
  // the preferences variable and their defaults
  
  extern boolean useCustomChars;
  extern String  customCharSet;

  extern uint8_t version_major;
  extern uint8_t version_minor;
  extern uint8_t sidetoneVolume;
  extern const uint8_t volumeMin;
  extern const uint8_t volumeMax;
 
  extern uint8_t wpm;
  extern const uint8_t wpmMin;
  extern const uint8_t wpmMax;

  extern uint8_t kochFilter;
  extern uint8_t kochCharsLength;
  extern uint8_t kochMinimum;
  extern uint8_t kochMaximum;

/// variables for managing snapshots
  extern uint8_t memories[8];           // contains snapshot numbers 0..7 for each stored snapshot
  extern uint8_t memCounter;            // contains number of stored snapshots
  extern uint8_t memPtr;
  
  ///// stored in preferences, but not adjustable through preferences menu:
  extern uint8_t responsePause;
  extern uint8_t menuPtr;
  extern uint8_t newMenuPtr;
  
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
  extern uint8_t loraPower;
  extern uint8_t snapShots;
  extern uint8_t boardVersion;
  extern uint8_t oledBrightness;
  
  ////// end of variables stored in preferences
  
  //// for adjusting preferences
  

  #define MAX_MAP_ELEMENTS 15

  struct parameter {
    uint8_t value;
     const uint8_t minimum;
     const uint8_t maximum;
     const uint8_t stepValue;
     const char *parName;
     const char *parDescript;
     const boolean isMapped;
     const char *mapping[MAX_MAP_ELEMENTS];
  };
  
  extern parameter pliste[];

  extern const String prefOption[];
  extern const String prefDescript[];
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
  
 
    
  void updateMemory(uint8_t);
  void clearMemory(uint8_t);
  String doClearMemory(uint8_t);
  boolean  recallSnapshot();
  boolean storeSnapshot(uint8_t);
  boolean setupPreferences(uint8_t);
  void displayKeyerPreferencesMenu(prefPos);
  void displayValueLine(prefPos, String, bool);
  String getValueLine(prefPos);
  int getValue(prefPos);
  boolean adjustKeyerPreference(prefPos);
  void readPreferences(String repository);
  void writePreferences(String repository);
  void doWriteSnapshot(uint8_t, uint8_t);
  void doReadSnapshot(uint8_t);
  void createKochWords(uint8_t maxl, uint8_t koch);
  uint8_t wordIsKoch(String thisWord);
  void createKochAbbr(uint8_t maxl, uint8_t koch);
  void handleKochSequence();
  void handleCarouselChange();
  void setCustomChars(String);
  //void kochSetup();
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


class Koch {
  private:
    uint16_t wordIndices[EnglishWords::WORDS_NUMBER_OF_ELEMENTS];
    uint16_t numberOfWords;
    uint16_t abbrIndices[Abbrev::ABBREV_NUMBER_OF_ELEMENTS];
    uint16_t numberOfAbbr;
    String kochCharSet;
    String licwKochChars;
    const String lcwoKochChars =      "kmuresnaptlwi.jz=foy,vg5/q92h38b?47c1d60x-K+ASNEB@:";
    const String cwacKochChars =      "teanois14rhdl25ucmw36?fypg79/bvkj80=xqz.,-K+ASNEB@:";
    //uint8_t kochCharsLength;
    void createWords(uint8_t, uint8_t);
    void createAbbr(uint8_t, uint8_t);
    uint8_t wordIsKoch(String);

    uint8_t adaptiveProbabilities[51];
    String initSequence;
    uint8_t initSequenceIndex;

  public:
    const String morserinoKochChars = "mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=K+SNAEB@:";
    const String licwAllKochChars =   "reatinpgslcdhofuwbkmy59,qxv73?+K=16.zj/28B40-ASNE@:";
    
    Koch();
    void setup();
    String getNewChar();
    String getKochChar(uint8_t);
    String getRandomChar(int);
    String getRandomWord();
    String getRandomAbbrev();
    String getAdaptiveChar(int);
    String getCharSet();
    String getRandomCharSet();
    String getInitChar(int);
    void setKochChars(uint8_t);
    uint8_t setupLICWkochChars(uint8_t);
    void setCustomChars(String chars);
    int16_t getProbabilitySum();
    void increaseWordProbability(String& expected, String& received);
    int getFailedCharIndex(String& expected, String& received);
    void increaseCharProbability(char c, uint8_t count);
    void decreaseWordProbability(String& word);
    void decreaseCharProbability(char c);
    uint8_t adjustForCarousel(uint8_t offset);
};

extern Koch koch;

#endif
