#ifndef MORSEPREFERENCES_H_
#define MORSEPREFERENCES_H_

/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                            ***
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
extern volatile bool powerpath_event;
//extern uint16_t volt;
extern int16_t batteryVoltage();

extern String cleanUpProSigns( String &input );
//extern int16_t batteryVoltage();
//extern int16_t volt;
extern void updateTimings();
// Phase F/L6: value-only speed/volume cores (clamp + timings/codec + serial
// protocol, no drawing). Interactive modes that own their own HUD — the games —
// route in-game speed/volume through these instead of poking the prefs directly,
// so timings/codec/protocol stay consistent without touching the classic line.
extern void changeSpeedValue(int t);
extern void changeVolumeValue(int t);

extern int IRAM_ATTR checkEncoder();

extern void keyOut(boolean,  boolean, int, int);
extern void checkShutDown(boolean);
extern void cleanStartSettings();
extern void serialEvent();

extern String getCustomChars();
extern const char CWchars[];    // m32_v6.ino; first 51 bytes are the plain (non-Koch) RANDOMS char pool - see the ruler comment there


#define MAX_FILE_PARTS 16
 
struct FilePart {
    uint32_t  startOffset;               // byte offset in file where this part begins
    uint32_t  endOffset;                 // byte offset where this part ends (= next part's start or EOF)
    uint32_t  wordPointer;               // persistent word pointer for this part
    char      name[24];                  // chapter name (truncated to 23 chars + null)
};
 

namespace MorsePreferences
{
  //String extraItems[8];
  // the preferences variable and their defaults

  extern boolean useCustomChars;
  extern String  customCharSet;

  // "Practice Set": a persisted, device-picked character pool for the plain
  // CW Generator / Echo Trainer "Practice Set" leaves. Entirely separate
  // from the Koch Trainer's useCustomChars/customCharSet lesson data above -
  // its own NVS key ("practiceChars"), never touched by koch.setup()/Koch::*.
  extern boolean usePracticeChars;
  extern String  practiceCharSet;

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

  /// variables for managing file parts in player mode
  extern uint8_t  filePartCount;       // 0 = single file (no parts), 1+ = number of parts
  extern uint8_t  filePartSelected;    // currently selected part index (0-based)
  extern FilePart fileParts[MAX_FILE_PARTS];

  void scanFileParts();                // scan player.txt for part separators
  void writeFilePartData();            // persist part data to NVS
  void readFilePartData();             // load part data from NVS
  static boolean confirmDelete(uint8_t);


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

  extern boolean useEspNow;
  extern uint8_t wlanChoice;
  
  #ifdef CONFIG_TFT
  struct themes {
    uint16_t foreground;
    uint16_t background;
    uint16_t morse;            // M6: colour for CW transcription text (per theme)
    uint16_t ok;               // M6: echo-trainer OK result colour (green, per theme)
    uint16_t err;              // M6: echo-trainer ERR result colour (red, per theme)
  };
  extern themes themeList[];                 // theme for display wrapper
  #endif

  extern uint32_t fileWordPointer;
  extern uint8_t promptPause;
  extern touch_value_t tLeft;
  extern touch_value_t tRight;
  extern uint8_t vAdjust;
  extern uint8_t loraBand;
 #define QRG433 434.15E6
 #define QRG866 869.15E6
 #define QRG920 920.55E6
  extern uint32_t loraQRG;
  extern uint8_t loraPower;
  extern boolean leftHanded;
  extern boolean cn3Mechanical;   // CN3 connector: false = capacitive touch paddle (default), true = mechanical paddle/key
  extern uint8_t snapShots;
  extern uint8_t boardVersion;
  extern uint8_t oledBrightness;
  extern char cwMem[][48];
  extern uint8_t cwMemMask;

  ////// end of variables stored in preferences

  //// for adjusting preferences


  #define MAX_MAP_ELEMENTS 15
  #define DEFAULT_VADJUST 210
  
  struct parameter {
     uint8_t value;
     const uint8_t minimum;
     const uint8_t maximum;
     const uint8_t stepValue;
     const char *parName;
     const char *parDescript;
     const boolean isMapped;
     const char *mapping[MAX_MAP_ELEMENTS];
     const char *spokenName;   // V9.0 audio accessibility: ear-friendly label for TTS;
                               // nullptr ==> fall back to parName. Only the cryptic,
                               // display-cramped parNames need to set this.
  };

  extern parameter pliste[];

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
#ifdef CONFIG_QSO_BOT
  extern  prefPos qsoBotOptions[];
#endif
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
#ifdef CONFIG_QSO_BOT
  extern int qsoBotOptionsSize;
#endif
  extern int decoderOptionsSize;
  extern int allOptionsSize;

  void updateMemory(uint8_t);
  void clearMemory(uint8_t);
  String doClearMemory(uint8_t);
  boolean  recallSnapshot();
  boolean storeSnapshot(uint8_t);
  boolean setupPreferences(uint8_t);
  void displayKeyerPreferencesMenu(prefPos);
  void displayValueLine(prefPos pos, const String& itemText, boolean jsonOnly, boolean withHeading = true);
  String getValueLine(prefPos);
  int getValue(prefPos);
  boolean adjustKeyerPreference(prefPos);
  void editPlayerIdentity(prefPos pos);
  void resetGameScores();
  void editPracticeChars();
  void setPracticeChars(const String& chars);   // assign + persist; shared with the serial "practicechars" command
  void readPreferences(const char* repository);
  void readScreenPref();
  void readVoltagePref();
  void writePreferences(const char* repository);
  boolean storedInSnapshot(prefPos pos);               // does this preference belong in a snapshot (training settings only)?
  boolean decodeSnapshot(const char* ns, uint8_t vals[], uint8_t &lastExec,
                         uint8_t &kochLen, uint8_t &useCustom, String &customSet);  // read snapshot (blob or legacy) into a prefPos-indexed value map
  void checkNvsSpace();                                // boot check: warn on display when NVS entries run low
  void convertLegacySnapshots();                       // boot migration: per-key snapshots -> blob format (no-op once converted)
  void resetDefaults();                                // reset all preferences to default values
  boolean doWriteSnapshot(uint8_t, uint8_t);           // returns false if the snapshot did not reach NVS
  void doReadSnapshot(uint8_t);
  void createKochWords(uint8_t maxl, uint8_t koch);
  uint8_t wordIsKoch(String thisWord);
  void createKochAbbr(uint8_t maxl, uint8_t koch);
  boolean handleKochSequence();
  void handleCarouselChange();
  void setCustomChars(String);
  //void kochSetup();
  void loraSystemSetup();
  void flipScreen();
#ifdef CONFIG_CN3_PADDLE
  void cn3PaddleConfig();     // Hardware Config menu: choose Touch / Mechanical for the CN3 connector

  // Hardware-Config boot menu slots: the hwConf rotary cycles through numbered slots
  //   0 = Cancel, 1 = Calibr. Batt., 2 = Flip Screen, 3 = Reset Defaults, [4 = LoRa Config.]
  // The LoRa slot only exists when LoRa is enabled. The CN3 Paddle slot is appended
  // after the last existing slot; these macros keep the label switch (getValueLine),
  // the rotary logic (adjustKeyerPreference) and the action dispatch (setup() in the .ino)
  // in sync.
  #ifndef LORA_DISABLED
    #define HWCONF_CN3_SLOT 5     // ..., 4 = LoRa Config., 5 = CN3 Paddle
    #define HWCONF_NUM_SLOTS 6
  #else
    #define HWCONF_CN3_SLOT 4     // no LoRa slot, so CN3 Paddle takes slot 4
    #define HWCONF_NUM_SLOTS 5
  #endif
#endif
  void determineBoardVersion();
  void calibrateVoltageMeasurement();
  void setVoltageAdjust(uint8_t);
  void writeWordPointer();
  void writeVolume();
  void writeLastExecuted(uint8_t menuPtr);
  void writeWifiInfo(const String&, const String&, const String&);
  void writeWifiInfoMultiple(const String&, const String&, const String&, const String&, const String&, const String&, const String&, const String&, const String&);
  void writeBrightnessPreference(uint8_t);
  void fireCharSeen(boolean wpmOnly);
  void setCurrentOptions(prefPos *current, int size);
  void setCwMem(uint8_t, String);
  void getCwMem();
}


class Koch {
  private:
    uint16_t wordIndices[EnglishWords::WORDS_NUMBER_OF_ELEMENTS];
    uint16_t numberOfWords;
    uint16_t abbrIndices[Abbrev::ABBREV_NUMBER_OF_ELEMENTS];
    uint16_t numberOfAbbr;
    String kochCharSet;
    String licwKochChars;
    const char* const lcwoKochChars =      "kmuresnaptlwi.jz=foy,vg5/q92h38b?47c1d60x-K+ASNEB@:";
    const char* const cwacKochChars =      "teanois14rhdl25ucmw36?fy,pgq79/bv+kj80=xzBK.-ASNE@:";
    //uint8_t kochCharsLength;
    void createWords(uint8_t, uint8_t);
    void createAbbr(uint8_t, uint8_t);
    uint8_t wordIsKoch(String);

    uint8_t adaptiveProbabilities[51];
    String initSequence;
    uint8_t initSequenceIndex;

  public:
    const char* const morserinoKochChars = "mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-=K+SNAEB@:";
    const char* const licwAllKochChars =   "reatinpgslcdhofuwbkmy59,qxv73?+K=16.zj/28B40-ASNE@:";

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
    void setCustomChars(const String& chars);
    int16_t getProbabilitySum();
    void increaseWordProbability(String& expected, String& received);
    int getFailedCharIndex(String& expected, String& received);
    void increaseCharProbability(char c, uint8_t count);
    void decreaseWordProbability(String& word);
    void decreaseCharProbability(char c);
    uint8_t adjustForCarousel(uint8_t offset);
};

extern Koch koch;
extern const char * prefName[];

#endif /* #ifndef MORSEPREFERENCES_H_ */