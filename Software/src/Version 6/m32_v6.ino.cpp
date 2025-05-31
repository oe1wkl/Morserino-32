# 1 "/var/folders/r5/jl5df8ps48sg5ll0fpbp01gh0000gn/T/tmppuqyo2fd"
#include <Arduino.h>
# 1 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
# 29 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
#include "morsedefs.h"

#include "wklfonts.h"


#include "abbrev.h"
#include "english_words.h"
#include "MorseJSON.h"
#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "MorseWiFi.h"
#include "goertzel.h"
#include "MorseDecoder.h"

#ifdef LORA_RADIOLIB
#include <RadioLib.h>
#ifdef RADIO_SX1262
        
# 47 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
#pragma message ("Compiling with Radiolib SX1262")
# 47 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"

RADIO radio = new Module(LoRa_nss, LoRa_dio1, LoRa_nrst, LoRa_busy);
#else
        
# 50 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
#pragma message ("Compiling with Radiolib SX1278")
# 50 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"

RADIO radio = new Module(LoRa_nss, LoRa_dio0, LoRa_nrst, LoRa_dio1);
#endif
#endif

volatile bool loraReceived = false;
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void packetReceived();
void IRAM_ATTR codec_isr();
void IRAM_ATTR powerpath_isr();
int checkEncoder();
void DEBUG (String s);
void setup();
boolean key_was_pressed_at_start();
boolean checkKey ();
void displayStartUp(uint16_t volt);
void loop();
void updateManualSpeed();
void checkStopFlag();
void cleanStartSettings();
boolean doPaddleIambic (boolean dit, boolean dah);
boolean checkPaddles();
void updatePaddleLatch(boolean dit, boolean dah);
void clearPaddleLatches ();
void setDITstate();
void setDAHstate();
void togglePolarity ();
uint8_t readSensors(int left, int right, boolean init);
void initSensors();
String getRandomWord( int maxLength);
String getRandomAbbrev( int maxLength);
String getRandomChars(int maxLength, int option);
String getRandomCall(int maxLength);
String generateCWword(String symbols);
int indexOfConstStr(const char* string, char c);
void generateCW ();
unsigned int wordDoublerICS();
int pitch();
void dispGeneratedChar();
void fetchNewWord();
void displayDecodedMorse(String symbol, boolean keyed);
void displayGeneratedMorse(FONT_ATTRIB style, String s);
void SerialOutMorse(String s, uint8_t origin);
void displayCWspeed();
void updateTopLine();
String getKeyerModeSymbol();
void echoTrainerEval();
void updateTimings();
void changeSpeed( int t);
void changeVolume( int t);
void keyTransmitter(boolean noTx);
String cleanUpProSigns( String &input );
int16_t batteryVoltage();
double ReadVoltage(byte pin);
void checkShutDown(boolean enforce);
void shutMeDown();
void cwForTx (int element);
void sendWithLora();
void sendWithWifi();
void onLoraReceive();
void onWifiReceive(AsyncUDPPacket packet);
void onEspnowRecv(const uint8_t* mac, const uint8_t* data, uint8_t len, signed int rssi, bool broadcast);
String CWwordToClearText(String cwword);
String encodeProSigns( String &input );
uint8_t cwBuWrite(int rssi, String packet);
boolean cwBuReady();
uint8_t cwBuRead(uint8_t* buIndex);
void storePacket(int rssi, String packet);
uint8_t decodePacket(int* rssi, int* wpm, String* cwword);
void keyOut(boolean on, boolean fromHere, int f, int volume);
void audioLevelAdjust();
void memoryKeyer();
void dispMem(int8_t memNo);
void preparePlay(int8_t memNo);
String getM32PWord();
String getWord();
String getCustomChars();
String cleanUpText(String text);
String utf8umlaut(String s);
void skipWords(uint32_t count);
void serialEvent();
void serialDecode(String input);
void m32Get(String type, String token, String value);
void m32Put(String type, String token, String value);
boolean setParameter(String token, String value);
void setSsid(int i, String str);
void setPassword(int i, String str);
void setTrxPeer(int i, String str);
void selectWifi(int i);
void stopPlayCw();
#line 59 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
void packetReceived() {
  loraReceived = true;
}

#ifdef CONFIG_BLUETOOTH_KEYBOARD
#include "MorseBluetooth.h"
#endif
using namespace Buttons;






uint8_t batteryPin;
int leftPin, rightPin;



uint16_t volt;
double voltage_raw;

ClickButton Buttons::modeButton(modeButtonPin);
ClickButton Buttons::volButton(volButtonPin);

Decoder keyDecoder(USE_KEY);
Decoder audioDecoder(USE_AUDIO);
M32MorseTable keyerTable;
Koch koch;



ESP32Encoder rotaryEncoder;
# 100 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
morserinoMode morseState = morseKeyer;


loops m32state;
boolean goToMenu = false;
boolean executeMenu = false;
boolean executeNow = false;
boolean playCW = false;
boolean repeatPlayCW = false;
String playCWBuffer;
uint8_t playCWIndex = 0;
uint8_t playCWMaxIndex = 0;




boolean quickStart;




encoderMode encoderState = speedSettingMode;







boolean m32protocol = false;
String inputString = "";
boolean stringComplete = false;
String vsn, brd;


int8_t ptr = 0;
uint8_t memList[9];
int8_t maxMemCount = 0;


unsigned int interCharacterSpace, interWordSpace;
unsigned int halfICS;
unsigned int effWpm;
touch_value_t lUntouched = 0;
touch_value_t rUntouched = 0;

boolean alternatePitch = false;

enum AutoStopModes
  {
      nextword, halt, repeatword
  };

AutoStopModes autoStop = halt;

GEN_TYPE generatorMode = RANDOMS;


File file;


int8_t hwConf = 1;

boolean kochActive = false;



#define DIT_L 0x01
#define DAH_L 0x02
#define DIT_LAST 0x04



unsigned char keyerControl = 0;



boolean DIT_FIRST = false;
unsigned int ditLength ;
unsigned int dahLength ;
KEYERSTATES keyerState;
unsigned long charCounter = 25;
uint8_t sensor;
boolean leftKey, rightKey;


unsigned long interWordTimer = 0;
unsigned long acsTimer = 0;



const char CWchars[] = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKEBäöüH";
# 203 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
String CWword = "";
String clearText = "";

int repeats = 0;

int rxDitLength = 0;
int rxDahLength = 0;
int rxInterCharacterSpace = 0;
int rxInterWordSpace = 0;


boolean genIsActive= false;
boolean startFirst = true;
boolean firstTime = true;

uint8_t wordCounter = 0;
uint16_t errCounter = 0;
boolean stopFlag = false;
boolean echoStop = false;
String lastWord = "";

unsigned long genTimer;

enum MORSE_TYPE {KEY_DOWN, KEY_UP };
unsigned char generatorState;

const String continueMsg4Json = "Continue with paddle";

#ifndef CONFIG_DISPLAYWRAPPER
const String continueMsg4Disp = "Continue w/ Paddle";
#else
const String continueMsg4Disp = continueMsg4Json+ " ";
#endif




byte poolPair[2];

const byte pool[][2] = {

               {B01000000, 2},
               {B10000000, 4},
               {B10100000, 4},
               {B10000000, 3},
               {B00000000, 1},
               {B00100000, 4},
               {B11000000, 3},
               {B00000000, 4},
               {B00000000, 2},
               {B01110000, 4},
               {B10100000, 3},
               {B01000000, 4},
               {B11000000, 2},
               {B10000000, 2},
               {B11100000, 3},
               {B01100000, 4},
               {B11010000, 4},
               {B01000000, 3},
               {B00000000, 3},
               {B10000000, 1},
               {B00100000, 3},
               {B00010000, 4},
               {B01100000, 3},
               {B10010000, 4},
               {B10110000, 4},
               {B11000000, 4},

               {B11111000, 5},
               {B01111000, 5},
               {B00111000, 5},
               {B00011000, 5},
               {B00001000, 5},
               {B00000000, 5},
               {B10000000, 5},
               {B11000000, 5},
               {B11100000, 5},
               {B11110000, 5},

               {B01010100, 6},
               {B11001100, 6},
               {B11100000, 6},
               {B10000100, 6},
               {B10010000, 5},
               {B10001000, 5},
               {B00110000, 6},
               {B01101000, 6},
               {B01010000, 5},

               {B01000000, 5},
               {B10101000, 5},
               {B10110000, 5},
               {B00010100, 6},
               {B00010000, 5},
               {B10001010, 7},

               {B01010000, 4},
               {B11100000, 4},
               {B00110000, 4},
               {B11110000, 4}

            };





String echoResponse = "";

echoStates echoTrainerState = START_ECHO;
String echoTrainerPrompt, echoTrainerWord;




boolean lastWasKey = true;
boolean speedChanged = false;



char cwTxBuffer[80];

uint8_t cwRxBuffer[1024];
uint16_t byteBuFree = sizeof(cwRxBuffer);
uint8_t nextBuWrite = 0;
uint8_t nextBuRead = 0;

uint8_t cwTxSerial;





IPAddress peerIP;

#ifdef CONFIG_TLV320AIC3100_INT
volatile bool codec_event = false;
void IRAM_ATTR codec_isr() {
  codec_event = digitalRead(CONFIG_TLV320AIC3100_INT) == HIGH ? true : false;
}
#endif

#ifdef CONFIG_MCP73871
volatile bool powerpath_event = true;
void IRAM_ATTR powerpath_isr() {
  powerpath_event = true;
}
#endif

uint8_t scrollTop;
# 404 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
int checkEncoder()
{
    static long int oldPosition = 0;
    long newPosition = rotaryEncoder.getCount() / 2 ;
    long diff;


    if (newPosition == oldPosition)
        return 0;
    else {

        diff = newPosition - oldPosition;
        oldPosition = newPosition;
        if (diff > 0)
            return 1;
        else
            return -1;
    }
}




void DEBUG (String s) {
  if (!MorsePreferences::pliste[posSerialOut].value || IGNORE_SERIALOUT)
    Serial.println(s);
}



void setup()
{
# 446 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
  Serial.begin(115200);
  delay(50);

  inputString.reserve(255);


  MorsePreferences::determineBoardVersion();

  if (MorsePreferences::boardVersion == 3) {
    batteryPin = 13;
    leftPin = 33;
    rightPin = 32;
  } else {
    batteryPin = 37;
#ifndef INTERNAL_PULLUP
    Buttons::modeButton.activeHigh = HIGH;
#endif
    leftPin = 32;
    rightPin = 33;
  }

#ifdef PIN_PADDLE_LEFT
  leftPin = PIN_PADDLE_LEFT;
#endif
#ifdef PIN_PADDLE_RIGHT
  rightPin = PIN_PADDLE_RIGHT;
#endif

#ifdef PIN_BATTERY
  batteryPin = PIN_BATTERY;
#endif





  MorsePreferences::readPreferences("morserino");
  koch.setup();


  volt = batteryVoltage();





  pinMode(PinCLK,INPUT_PULLUP);
  pinMode(PinDT,INPUT_PULLUP);
  pinMode(keyerPin, OUTPUT);
#ifdef INTERNAL_PULLUP
  pinMode(leftPin, INPUT_PULLUP);
  pinMode(rightPin, INPUT_PULLUP);
#else
  pinMode(leftPin, INPUT);
  pinMode(rightPin, INPUT);
#endif

  analogSetAttenuation(ADC_0db);


#ifdef INTERNAL_PULLUP
pinMode(modeButtonPin, INPUT_PULLUP);
#else
pinMode(modeButtonPin, INPUT);
#endif

#ifndef VEXT_ON_VALUE
#define VEXT_ON_VALUE LOW
#endif

#ifdef PIN_VEXT
pinMode(PIN_VEXT, OUTPUT);
digitalWrite(PIN_VEXT, VEXT_ON_VALUE);
#endif


  MorseOutput::initDisplay();

  MorseOutput::setBrightness(MorsePreferences::oledBrightness);
  MorseOutput::clearDisplay();
  scrollTop = MorseOutput::getScrollTop();
  MorseOutput::soundSetup();

#ifdef CONFIG_TLV320AIC3100_INT
  pinMode(CONFIG_TLV320AIC3100_INT, INPUT);
  attachInterrupt(CONFIG_TLV320AIC3100_INT, codec_isr, RISING);
#endif

#ifdef CONFIG_MCP73871
  pinMode(CONFIG_MCP_PG_PIN, INPUT_PULLUP);
  pinMode(CONFIG_MCP_STAT1_PIN, INPUT_PULLUP);
  pinMode(CONFIG_MCP_STAT2_PIN, INPUT_PULLUP);
  attachInterrupt(CONFIG_MCP_PG_PIN, powerpath_isr, CHANGE);
  attachInterrupt(CONFIG_MCP_STAT1_PIN, powerpath_isr, CHANGE);
  attachInterrupt(CONFIG_MCP_STAT2_PIN, powerpath_isr, CHANGE);
#endif

  rotaryEncoder.attachHalfQuad ( PinDT, PinCLK );
  rotaryEncoder.setCount ( 0 );




  pinMode(volButtonPin, INPUT_PULLUP);
#ifdef INTERNAL_PULLUP
  pinMode(modeButtonPin, INPUT_PULLUP);
#else
  pinMode(modeButtonPin, INPUT);
#endif





  Buttons::modeButton.debounceTime = 11;
  Buttons::modeButton.multiclickTime = 220;
  Buttons::modeButton.longClickTime = 350;

  Buttons::volButton.debounceTime = 11;
  Buttons::volButton.multiclickTime = 220;
  Buttons::volButton.longClickTime = 350;

  MorseOutput::printOnStatusLine( true, 0, "Init...pse wait...");


  if (MorsePreferences::useEspNow) {
      quickEspNow.onDataRcvd (onEspnowRecv);
      MorseMenu::setupESPNow();
  }


  if (SPIFFS.begin(false)) {
      if (key_was_pressed_at_start()) {
         MorsePreferences::displayKeyerPreferencesMenu(posHwConf);
         MorsePreferences::adjustKeyerPreference(posHwConf);

         switch (hwConf) {
            case 1:
                    MorsePreferences::calibrateVoltageMeasurement();
                    break;
            case 2: MorsePreferences::flipScreen();
                    ESP.restart();
                    break;
            case 3: MorsePreferences::loraSystemSetup();
                    break;
            default: break;
         }
      }
  }
  analogSetAttenuation(ADC_0db);
  adcAttachPin(audioInPin);


  initSensors();




  quickStart = MorsePreferences::pliste[posQuickStart].value;



#ifdef LORA_RADIOLIB
  SPI.begin(LoRa_SCK, LoRa_MISO, LoRa_MOSI, LoRa_nss);
  Serial.print(F("[SX12xx] Initializing ... "));
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) Serial.println("ERROR: radiolib begin()");
  radio.setFrequency(MorsePreferences::loraQRG/1000000.0);
  radio.setBandwidth(250.0);
  radio.setSpreadingFactor(7);
  radio.setSyncWord(MorsePreferences::pliste[posLoraChannel].value == 0 ? 0x27 : 0x66);
  radio.setOutputPower(MorsePreferences::loraPower);
  radio.setCRC(false);
  radio.setPacketReceivedAction(packetReceived);
#endif

#ifdef CONFIG_BLUETOOTH_KEYBOARD
  if ((MorsePreferences::pliste[posBluetoothOut].value) != 0) {

    MorseBluetooth::initializeBluetooth();
  }
#endif


  cwTxSerial = random(64);



    #define FORMAT_SPIFFS_IF_FAILED true

    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        DEBUG("SPIFFS Mount Failed");
        return;
    }

  const char * defaultFile = "This is just an initial dummy file for the player. Dies ist nur die anfänglich enthaltene Standarddatei für den Player.\n"
                             "Did you not upload your own file? Hast du keine eigene Datei hochgeladen?";

    if (!SPIFFS.exists("/player.txt")) {
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
    while (Serial.available())
      Serial.read();
    inputString = "";
    MorseMenu::menu_();


  }


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
      if (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY)
        ext = (uint8_t) !digitalRead(leftPin);
      else
        ext = (uint8_t) !(digitalRead(leftPin) && digitalRead(rightPin));

      if (sensor || ext)
        return true;
      else
        return false;
}



void displayStartUp(uint16_t volt) {
  String s;
  s.reserve(18);
  s = PROJECTNAME + String(" ");
  MorseOutput::clearDisplay();
  #ifdef CONFIG_DISPLAYWRAPPER
  MorseOutput::dispM32Logo();
  delay(1800);
  MorseOutput::clearDisplay();
  #endif

#ifndef LORA_DISABLED
  s += String(MorsePreferences::loraQRG / 10000);
#endif
  MorseOutput::printOnStatusLine( true, 0, s);
  s = "V." ;
  vsn = String(VERSION_MAJOR) + "." + String(VERSION_MINOR) ;
  if (VERSION_PATCH != 0)
    vsn = vsn + "." + String(VERSION_PATCH);
  if (BETA)
    vsn += " beta";
  s += vsn;
  MorseOutput::printOnScroll(0, REGULAR, 0, s );
  MorseOutput::printOnScroll(1, REGULAR, 0, "© 2018-2025");



#ifndef SKIP_BATTERY_PROTECT
#ifdef CONFIG_MCP73871
  uint8_t powerpath_state = (digitalRead(CONFIG_MCP_STAT1_PIN)<<2) + ( digitalRead(CONFIG_MCP_STAT2_PIN) << 1) + digitalRead(CONFIG_MCP_PG_PIN);
  if (powerpath_state == 3)
    MorseOutput::displayEmptyBattery(shutMeDown);
  else
#else
  if (volt > 1000 && volt < 2800)
    MorseOutput::displayEmptyBattery(shutMeDown);
  else
#endif
#endif
    MorseOutput::displayBatteryStatus(volt);




#define ST(A) #A
#define STR(A) ST(A)
#ifdef HW_NAME

  if (STR(HW_NAME) == "original") {
      if (MorsePreferences::boardVersion == 3)
          brd = "M32 1st edition";
      else if (MorsePreferences::boardVersion == 4)
          brd = "M32 2nd edition";
      else
          brd = "unknown";
    }
    else
      brd = STR(HW_NAME);

#else
    brd = "Unknown Device";
#endif

  delay(2000);

}



void loop() {
   int t;

   m32state = active_loop;

   if (playCW) {
                  if (checkPaddles()) {
                    stopPlayCw();
                    while (checkPaddles())
                      ;
                    return;
                  }
   }


   switch (keyerState) {
      case DIT:
      case DAH:
      case KEY_START:
               break;
      default: checkPaddles();
               break;
   }

   switch (morseState) {
      case morseKeyer: if (doPaddleIambic(leftKey, rightKey)) {
                              return;
                          }
                          if (playCW)
                              generateCW();
                          break;

      case loraTrx:
      case wifiTrx: if (doPaddleIambic(leftKey, rightKey)) {
                              return;
                          }
                          generateCW();
                          break;

      case morseTrx: if (doPaddleIambic(leftKey, rightKey)) {
                              return;
                          }
                          if (playCW)
                            generateCW();
                          audioDecoder.decode();
                          if (speedChanged) {
                            speedChanged = false;
                            displayCWspeed();
                          }
                          break;
      case morseGenerator:
                          if (MorsePreferences::pliste[posAutoStop].value) {
                              if ((autoStop == halt)) {
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

                              while (checkPaddles() )
                                  ;
                              genIsActive= true;
                              break;
                              }
                          }
                          if (leftKey || rightKey || executeNow) {

                              while (checkPaddles() )
                                ;
                              delay(15);
                              executeNow = false;
                              genIsActive = !genIsActive;
                              if (genIsActive)
                                  cleanStartSettings();
                              else {
                                  keyOut(false, true, 0, 0);
                                  MorseOutput::printOnStatusLine( true, 0, continueMsg4Disp);
                                  if (m32protocol)
                                    MorseJSON::jsonCreate("message", continueMsg4Json, "");
                              }
                          } else {
                              checkStopFlag();
                          }
                          if (genIsActive)
                            generateCW();
                          break;
      case echoTrainer:
                          checkStopFlag();
                          if (!genIsActive&& (leftKey || rightKey)) {

                              while (checkPaddles() )
                                  ;
                              genIsActive = !genIsActive;

                              cleanStartSettings();
                          }
                          if (genIsActive)
                          switch (echoTrainerState) {
                            case START_ECHO:
                            case SEND_WORD:
                            case REPEAT_WORD: echoResponse = ""; generateCW();
                                                break;
                            case EVAL_ANSWER: echoTrainerEval();
                                                break;
                            case COMPLETE_ANSWER:
                            case GET_ANSWER: if (doPaddleIambic(leftKey, rightKey))
                                                return;
                                                break;
                            }
                            break;
      case morseDecoder:
                         keyDecoder.decode();
                         audioDecoder.decode();
                         if (speedChanged) {
                            speedChanged = false;
                            displayCWspeed();
                          }
      default: break;


  }




    serialEvent();
    if (goToMenu) {
        MorseJSON::jsonActivate(ACT_EXIT);
        goToMenu = false;
        MorseMenu::menu_();
        return;
    }

    Buttons::modeButton.Update();
    Buttons::volButton.Update();

    switch (Buttons::volButton.clicks) {
      case 1: if (encoderState == scrollMode) {
                    if (morseState != morseDecoder)
                        encoderState = speedSettingMode;
                    else
                        encoderState = volumeSettingMode;
                    MorseOutput::relPos = MorseOutput::maxPos;

                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                    MorseOutput::displayScrollBar(false);
                }
                else if (encoderState == volumeSettingMode && morseState != morseDecoder) {
                  encoderState = speedSettingMode;
                  MorsePreferences::writeVolume();
                  displayCWspeed();
                  MorseOutput::displayVolume(true, MorsePreferences::sidetoneVolume);

                }
                else {
                  encoderState = volumeSettingMode;
                  displayCWspeed();
                  MorseOutput::displayVolume(false, MorsePreferences::sidetoneVolume);
                }
                break;
      case -1: if (encoderState == scrollMode) {
                    encoderState = (morseState == morseDecoder ? volumeSettingMode : speedSettingMode);
                    MorseOutput::relPos = MorseOutput::maxPos;

                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                    MorseOutput::displayScrollBar(false);
                }
                else {
                    encoderState = scrollMode;
                    MorseOutput::displayScrollBar(true);
                }
                break;
      case 2: MorseOutput::decreaseBrightness();
                break;
    }

    switch (Buttons::modeButton.clicks) {
       case -1: if (m32protocol)
                      MorseJSON::jsonActivate(ACT_EXIT);
                  MorseMenu::menu_();
                  return;
       case 1: if (encoderState == memSelMode) {
                    if (ptr != 0) {
                      preparePlay(memList[ptr]);
                      if (m32protocol)
                        MorseJSON::jsonOK();
                    }
                    encoderState = speedSettingMode;
                    updateTopLine();
                 }
                 else if (morseState == morseGenerator || morseState == echoTrainer) {
                  genIsActive = !genIsActive;
                  if (!genIsActive) {
                        keyOut(false, true, 0, 0);
                        MorseOutput::printOnStatusLine( true, 0, continueMsg4Disp);
                        if (m32protocol)
                            MorseJSON::jsonCreate("message", continueMsg4Json, "");
                  }
                  else {
                    cleanStartSettings();
                  }

              } else if (morseState == morseKeyer || morseState == morseTrx) {
                    if (m32protocol)
                        MorseJSON::jsonCreate("message", "Select Memory", "");
                    memoryKeyer();
              }
              break;
       case 2: MorsePreferences::setupPreferences(MorsePreferences::menuPtr);
                MorseOutput::clearDisplay();
                updateTopLine();
                if (morseState == morseGenerator || morseState == echoTrainer)
                    stopFlag = true;
                else
                    stopFlag = false;


     default: break;
    }


     if ((t = checkEncoder())) {

        MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
        switch (encoderState) {
          case speedSettingMode:
                                  changeSpeed(t);
                                  break;
          case volumeSettingMode:
                                  changeVolume(t);
                                  break;
          case scrollMode:
                                  if (t == 1 && MorseOutput::relPos < MorseOutput::maxPos ) {
                                    ++MorseOutput::relPos;

                                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                                  }
                                  else if (t == -1 && MorseOutput::relPos > 0) {
                                    --MorseOutput::relPos;

                                    MorseOutput::refreshScrollArea(MorseOutput::relPos);
                                  }

                                  MorseOutput::displayScrollBar(true);
                                  break;
           case memSelMode: ptr +=t;
                                  if (ptr == -1)
                                    ptr = maxMemCount;
                                  if (ptr > maxMemCount)
                                    ptr = 0;
                                  dispMem(memList[ptr]);
                                  break;
          }
    }
    checkShutDown(false);
#ifdef LORA_RADIOLIB
    if(loraReceived) {
      loraReceived=false;
      onLoraReceive();
    }
#endif

#ifdef CONFIG_TLV320AIC3100_INT
    if (codec_event) {
      codec_event = false;
      MorseOutput::soundEventHandler();
    }
#endif

#ifdef CONFIG_MCP73871
  if (powerpath_event) {
    powerpath_event = false;
    uint8_t powerpath_state = (digitalRead(CONFIG_MCP_STAT1_PIN)<<2) + ( digitalRead(CONFIG_MCP_STAT2_PIN) << 1) + digitalRead(CONFIG_MCP_PG_PIN);
    Serial.print("Powerpath state change:");
    switch (powerpath_state) {
      case 0:
        Serial.println("FAULT");
        break;
      case 2:
        Serial.println("Charging");
        break;
      case 3:
        Serial.println("Low Battery");
        break;
      case 4:
        Serial.println("Charge complete standby");
        break;
      case 6:
        Serial.println("Shutdown No Battery Present");
        break;
      case 7:
        Serial.println("Shutdown No Input Power Present");
        break;
      default:
        Serial.print("Unknown State: ");
        Serial.println(powerpath_state);
        break;
      }
    }
#endif
}


void updateManualSpeed() {
  if (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY && speedChanged) {
    speedChanged = false;
    displayCWspeed();
  }
}

void checkStopFlag() {
    if (stopFlag) {
      lastWord = clearText;
      genIsActive = stopFlag = false;
      keyOut(false, true, 0, 0);
      if (morseState == echoTrainer) {
        MorseOutput::clearStatusLine();
        MorseOutput::printOnStatusLine( true, 0, String(errCounter) + " errs (" + String(wordCounter-2) + " wds)" );
        delay(5000);
      }
      wordCounter = 1; errCounter = 0;


      MorseOutput::printOnStatusLine( true, 0, continueMsg4Disp);
      if (m32protocol)
        MorseJSON::jsonCreate("message", continueMsg4Json, "");
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
    interWordTimer = 4294967000;
    genTimer = millis()-1;
    errCounter = wordCounter = 0;
    startFirst = true;
    autoStop = nextword;
    keyOut(false, true, 0, 0);
    updateTopLine();
}





boolean doPaddleIambic (boolean dit, boolean dah) {
  boolean paddleSwap;
  static long ktimer;
  static long curtistimer;
  static long latencytimer;
  static long corrTime;
  unsigned int pitch;

  if (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY) {
    keyDecoder.decode();
    updateManualSpeed();
    return false;
  }
  if (MorsePreferences::pliste[posPolarity].value == 0) {
      paddleSwap = dit; dit = dah; dah = paddleSwap;
  }


  switch (keyerState) {
     case IDLE_STATE:

         if (millis() > interWordTimer) {
             if (morseState == loraTrx) {
                 cwForTx(3);
                 sendWithLora();
             }
             else if (morseState == wifiTrx) {
                 cwForTx(3);
                 sendWithWifi();
             }

             displayDecodedMorse(keyerTable.retrieveSymbol(), true);
             interWordTimer = 4294967000;
             if (echoTrainerState == COMPLETE_ANSWER) {
                echoTrainerState = EVAL_ANSWER;
                return false;
             }
         }


        if (dit || dah ) {
            updatePaddleLatch(dit, dah);
            if (morseState == echoTrainer) {
                echoTrainerState = COMPLETE_ANSWER;
             }
            keyerTable.resetTable();
            if (dit) {
                setDITstate();
                DIT_FIRST = true;
            }
            else {
                setDAHstate();
                DIT_FIRST = false;
            }
        }
        else {
           if (echoTrainerState == GET_ANSWER && millis() > genTimer) {
            echoTrainerState = EVAL_ANSWER;
         }
         return false;
        }
        break;

    case DIT:

            if ( MorsePreferences::pliste[posACS].value > 0 && (millis() <= acsTimer))
              break;
            clearPaddleLatches();
            keyerControl |= DIT_LAST;

            ktimer = ditLength;
            switch ( MorsePreferences::pliste[posCurtisMode].value ) {
              case ULTIMATIC:
              case IAMBICB: curtistimer = 2 + (ditLength * MorsePreferences::pliste[posCurtisBDotTiming].value / 100);
                             break;
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = ditLength;
                             break;
            }
            keyerState = KEY_START;
            break;

    case DAH:
            if ( MorsePreferences::pliste[posACS].value > 0 && (millis() <= acsTimer))
              break;
            clearPaddleLatches();
            keyerControl &= ~(DIT_LAST);

            ktimer = dahLength;
            switch (MorsePreferences::pliste[posCurtisMode].value) {
              case ULTIMATIC:
              case IAMBICB: curtistimer = 2 + (dahLength * MorsePreferences::pliste[posCurtisBDahTiming].value / 100);
                             break;
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = dahLength;
                             break;
            }
            keyerState = KEY_START;
            break;



    case KEY_START:

          pitch = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
          if ((morseState == echoTrainer || morseState == loraTrx || morseState == wifiTrx) && MorsePreferences::pliste[posEchoToneShift].value != 0) {
             pitch = (MorsePreferences::pliste[posEchoToneShift].value == 1 ? pitch * 18 / 17 : pitch * 17 / 18);
          }

           keyOut(true, true, pitch, MorsePreferences::sidetoneVolume);
           corrTime = millis() - 6;
           ktimer +=corrTime;
           curtistimer += corrTime;
           keyerState = KEYED;
           break;

    case KEYED:

           if (millis() >= ktimer) {
               keyOut(false, true, 0, 0);
               ktimer = millis() + ditLength -1;
               latencytimer = millis() + ((MorsePreferences::pliste[posLatency].value) * ditLength / 8);
               keyerState = INTER_ELEMENT;
            }
            else if (millis() >= curtistimer ) {
                 if (keyerControl & DIT_LAST)
                    updatePaddleLatch(false, dah);
                 else
                    updatePaddleLatch(dit, false);

            }
            break;

    case INTER_ELEMENT:

            if (millis() < latencytimer) {
              if (keyerControl & DIT_LAST)
                    updatePaddleLatch(false, dah);
              else
                    updatePaddleLatch(dit, false);
            }
            else {
                updatePaddleLatch(dit, dah);
                if (millis() >= ktimer) {
                    switch(keyerControl) {
                          case 3:
                          case 7:
                                  switch (MorsePreferences::pliste[posCurtisMode].value) {
                                      case STRAIGHTKEY: break;
                                      case NONSQUEEZE: if (DIT_FIRST)
                                                               setDITstate();
                                                        else
                                                               setDAHstate();
                                                        break;
                                      case ULTIMATIC: if (DIT_FIRST)
                                                               setDAHstate();
                                                        else
                                                               setDITstate();
                                                        break;
                                      default: if (keyerControl & DIT_LAST)
                                                            setDAHstate();
                                                        else
                                                            setDITstate();
                                   }
                                   break;

                          case 1:
                          case 5:
                                   setDITstate();
                                   break;

                          case 2:
                          case 6:
                                   setDAHstate();
                                   break;

                          case 0:
                          case 4:
                                   keyerState = IDLE_STATE;
                                   displayDecodedMorse(keyerTable.retrieveSymbol(), true);

                                   if (morseState == loraTrx || morseState == wifiTrx)
                                      cwForTx(0);

                                   ++charCounter;

                                   if (charCounter == 12) {
                                        MorsePreferences::fireCharSeen(false);
                                   }
                                   if (MorsePreferences::pliste[posACS].value > 0)
                                        acsTimer = millis() + (MorsePreferences::pliste[posACS].value + 1) * ditLength;
                                   if (morseState == morseKeyer || morseState == loraTrx || morseState == wifiTrx || morseState == morseTrx)

                                      interWordTimer = millis() + ditLength * (MorsePreferences::pliste[posInterWordSpace].value -1);
                                   else if (morseState == echoTrainer)
                                       interWordTimer = millis() + 2*interCharacterSpace + ditLength + interWordSpace/8;
                                   else
                                       interWordTimer = millis() + interWordSpace;



                                   keyerControl = 0;
                          break;
                    }
                }
            }
  }

  if (keyerControl & 3)
    return true;
  else
    return false;
}







boolean checkPaddles() {

  static boolean oldL = false, newL, oldR = false, newR;
  int left, right;
  static long lTimer = 0, rTimer = 0;
  const int debDelay = 512;



  left = MorsePreferences::pliste[posExtPddlPolarity].value ? rightPin : leftPin;
  right = MorsePreferences::pliste[posExtPddlPolarity].value ? leftPin : rightPin;
  sensor = readSensors(LEFT, RIGHT, false);
  newL = (sensor >> 1);
  newR = (sensor & 0x01);

  newL = newL | (!digitalRead(left)) ;
  if (MorsePreferences::pliste[posCurtisMode].value != STRAIGHTKEY) {
      newR = newR | (!digitalRead(right)) ;
  }

  if ((MorsePreferences::pliste[posCurtisMode].value == NONSQUEEZE) && newL && newR)
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






void updatePaddleLatch(boolean dit, boolean dah)
{
    if (dit)
      keyerControl |= DIT_L;
    if (dah)
      keyerControl |= DAH_L;
}


void clearPaddleLatches ()
{
    keyerControl &= ~(DIT_L + DAH_L);
}


void setDITstate() {
  keyerState = DIT;
  keyerTable.recordDit();
  if (morseState == loraTrx || morseState == wifiTrx)
      cwForTx(1);
}

void setDAHstate() {
  keyerState = DAH;
  keyerTable.recordDah();
  if (morseState == loraTrx || morseState == wifiTrx)
      cwForTx(2);
}



void togglePolarity () {
      MorsePreferences::pliste[posPolarity].value = MorsePreferences::pliste[posPolarity].value ? 0 : 1;

}
# 1433 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
uint8_t readSensors(int left, int right, boolean init) {
#ifdef TOUCHPADDLES_DISABLED
  return 0;
#else


  touch_value_t v, lValue, rValue;

  while ( !(v=touchRead(left)) )
    ;
  lValue = v;
   while ( !(v=touchRead(right)) )
    ;
  rValue = v;

  if (init == false) {
    if (sizeof(touch_value_t) < 4) {
      if (lValue < (MorsePreferences::tLeft+10)) {
          MorsePreferences::tLeft = ( 7*MorsePreferences::tLeft + ((lValue+lUntouched) / SENS_FACTOR) ) / 8;
      }
      if (rValue < (MorsePreferences::tRight+10)) {
          MorsePreferences::tRight = ( 7*MorsePreferences::tRight + ((rValue+rUntouched) / SENS_FACTOR) ) / 8;
      }
      return ( lValue < MorsePreferences::tLeft ? 2 : 0 ) + (rValue < MorsePreferences::tRight ? 1 : 0 );
    } else {
      return ( lValue > MorsePreferences::tLeft ? 2 : 0 ) + (rValue > MorsePreferences::tRight ? 1 : 0 );
    }
  } else {


    DEBUG("@1334: lValue, rValue: " + String (lValue) + " " + String(rValue));
    if (sizeof(touch_value_t) < 4) {
        if (lValue < 32 || rValue < 32)
          return 3;
        else
          return 0;
          }
    else {
        if (lValue > 45000 || rValue > 45000)
          return 3;
        else
          return 0;
    }
  }
#endif
}


void initSensors() {
#ifndef TOUCHPADDLES_DISABLED
  touch_value_t v;
  lUntouched = rUntouched = 60;
  for (int i=0; i<8; ++i) {
      while ( !(v=touchRead(LEFT)) )
        ;
        lUntouched += v;

       while ( !(v=touchRead(RIGHT)) )
        ;
        rUntouched += v;

  }
  lUntouched /= 8;
  rUntouched /= 8;
  if (sizeof(touch_value_t) < 4) {
    MorsePreferences::tLeft = lUntouched - 9;
    MorsePreferences::tRight = rUntouched - 9;
  } else {


    MorsePreferences::tLeft = lUntouched + 3000;
    MorsePreferences::tRight = rUntouched + 3000;
  }
#endif
}


String getRandomWord( int maxLength) {
    if (maxLength > 5)
      maxLength = 0;
    else if (maxLength != 0)
      ++maxLength;
    if (kochActive)
        return koch.getRandomWord();
    else
#ifdef CONFIG_ENGLISH_OXFORD
        return getEnglishWord(maxLength == 0 ? 100 : maxLength);
#else
        return EnglishWords::words[random(EnglishWords::WORDS_POINTER[maxLength], EnglishWords::WORDS_NUMBER_OF_ELEMENTS)];
#endif
}

String getRandomAbbrev( int maxLength) {
    if (maxLength > 5)
      maxLength = 0;
    else if (maxLength != 0)
      ++maxLength;
    if (kochActive)
        return koch.getRandomAbbrev();
    else
        return Abbrev::abbreviations[random(Abbrev::ABBREV_POINTER[maxLength], Abbrev::ABBREV_NUMBER_OF_ELEMENTS)];
}
# 1551 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
String getRandomChars(int maxLength, int option) {
    String result = "";
    result.reserve(7);
    int s = 0, e = 51;
    int i;

    if (maxLength > 6) {
      maxLength = random(2, maxLength - 3);
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
          default: s = 0; break;
        }
        switch (option) {
          case OPT_ALPHA:
                                e = 26; break;
          case OPT_ALNUM:
          case OPT_NUM:
                                e = 36; break;
          case OPT_ALNUMPUNCT:
          case OPT_NUMPUNCT:
          case OPT_PUNCT:
                                e = 45; break;
          default: e = 51; break;
        }

        for (i = 0; i < maxLength; ++i)

          result += CWchars[random(s,e)];
  }
  return result;
}


String getRandomCall(int maxLength) {
  const byte prefixType[] = {1,0,1,2,3,1};
  byte prefix;
  String call = ""; call.reserve(16);
  unsigned int l = 0;


  if (maxLength > 4)
      maxLength = 4;
  if (maxLength != 0)
      maxLength += 2;
  if (maxLength == 3)
      prefix = 0;
  else
      prefix = prefixType[random(0,6)];

  switch (prefix) {
      case 1: call += CWchars[random(0,26)];
              ++l;
      case 0: call += CWchars[random(0,26)];
              ++l;
              break;
      case 2: call += CWchars[random(0,26)];
              call += CWchars[random(26,36)];
              l = 2;
              break;
      case 3: call += CWchars[random(26,23)];
              call += CWchars[random(0,26)];
              l = 2;
              break;
    }

    call += CWchars[random(26,36)];
    ++l;

    if (maxLength == 3)
        prefix = 1;
    else if (maxLength == 0) {
        prefix = random(1,4);
        prefix = (prefix == 2 ? prefix : random(1,4));
    }
    else {
        prefix = _min(maxLength - l, 3);
    }
    while (prefix--) {
      call += CWchars[random(0,26)];
      ++l;
    }

    if (maxLength == 0 )
      if (! random(0,8)) {
      call += "/";
      call += ( random(0,2) ? "m" : "p" );
    }

    return call;
}





String generateCWword(String symbols) {
  int pointer;
  byte bitMask, NoE;

  String result = "";

  int l = symbols.length();

  for (int i = 0; i<l; ++i) {
    char c = symbols.charAt(i);


    pointer = indexOfConstStr(CWchars, c);
    NoE = pool[pointer][1];
    bitMask = pool[pointer][0];
    for (int j=0; j<NoE; ++j) {
        result += (bitMask & B10000000 ? "2" : "1" );
        bitMask = bitMask << 1;


      }
      result += "0";
  }
  result.remove(result.length()-1);
  return result;
}


int indexOfConstStr(const char* string, char c) {
    char *e = strchr(string, c);
    if (e == NULL) {
        return -1;
    }
    return (int)(e - string);
}

void generateCW () {

  static int l;
  static char c;
  boolean silentEcho;

  switch (generatorState) {
    case KEY_UP:
            if (millis() < genTimer)
                return;

            if (startFirst == true) {
                CWword = "";
            }
            l = CWword.length();

            if (l==0) {
                if (clearText.length() > 0) {


                if (MorsePreferences::pliste[posGeneratorDisplay].value == DISPLAY_BY_WORD &&
                    (morseState == loraTrx || morseState == wifiTrx || morseState == morseGenerator || playCW == true))
                  {
                      displayGeneratedMorse(morseState == morseGenerator ? REGULAR : BOLD,cleanUpProSigns(clearText));

                  }
                }
                fetchNewWord();

                if (CWword.length() == 0)
                  return;
                if ((morseState == echoTrainer)) {
                  displayGeneratedMorse(REGULAR, "\n");
                }
            }
            c = CWword[0];
            CWword.remove(0,1);
            if (c == '0' || !CWword.length()) {
                   if (c == '0') {
                      c = CWword[0];
                      CWword.remove(0,1);
                      if (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value >= 1)
                          cwForTx(0);
                      }
            }




            if (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value == DISP_ONLY)
                genTimer = millis() + 2;
            else if (morseState != loraTrx && morseState != wifiTrx)
                genTimer = millis() + (c == '1' ? ditLength-6 : dahLength-6);
            else
                genTimer = millis() + (c == '1' ? rxDitLength : rxDahLength);
            if (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value >= 1)
                c == '1' ? cwForTx(1) : cwForTx(2) ;

            if (generatorMode == KOCH_LEARN)
                displayGeneratedMorse(BOLD, c == '1' ? "." : "-");
            silentEcho = (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value == DISP_ONLY);

            if (silentEcho || stopFlag)
              ;
            else {
                keyOut(true, (morseState != loraTrx && morseState != wifiTrx), pitch(), MorsePreferences::sidetoneVolume);
            }
            generatorState = KEY_DOWN;

            break;
    case KEY_DOWN:
            if (millis() < genTimer)
                return;


           keyOut(false, (morseState != loraTrx && morseState != wifiTrx), 0, 0);
            if (! CWword.length()) {
                if (morseState == morseGenerator)
                    autoStop = MorsePreferences::pliste[posAutoStop].value ? halt : nextword;
                dispGeneratedChar();
                if (morseState == echoTrainer) {
                    switch (echoTrainerState) {
                        case START_ECHO: echoTrainerState = SEND_WORD;
                                          genTimer = millis() + 300 + interCharacterSpace + interWordSpace / 8;


                                          break;
                        case REPEAT_WORD:

                        case SEND_WORD: if (echoStop)
                                                break;
                                          else {
                                                echoTrainerState = GET_ANSWER;
                                                if (MorsePreferences::pliste[posEchoDisplay].value != CODE_ONLY) {
                                                    displayGeneratedMorse(REGULAR, " ");
                                                    displayGeneratedMorse(INVERSE_REGULAR, ">");
                                                }
                                                ++repeats;

                                                genTimer = millis() + 1400 + interCharacterSpace + interWordSpace / 3;

                                          }
                        default: break;
                    }
                }
                else {
                      genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxInterWordSpace : interWordSpace) ;
                      if (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value >= 1) {
                          cwForTx(0);
                          cwForTx(3);

                      if (MorsePreferences::pliste[posLoraCwTransmit].value == 1)
                          sendWithWifi();
                          else sendWithLora();
                          delay(interCharacterSpace+ditLength);
                      }
                }
             }
             else if ((c = CWword[0]) == '0') {


                dispGeneratedChar();
                if (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value == DISP_ONLY)
                    genTimer = millis() +1;
                else
                    genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxInterCharacterSpace : wordDoublerICS());
             }
             else {
                genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxDitLength : ditLength);
             }
             generatorState = KEY_UP;
             break;
  }
}

unsigned int wordDoublerICS() {
    if (morseState != morseGenerator || (MorsePreferences::pliste[posWordDoubler].value == 0) || ( MorsePreferences::pliste[posWordDoubler].value != 0 && firstTime == false))
      return interCharacterSpace;

    switch (MorsePreferences::pliste[posWordDoubler].value) {

      case 2: return halfICS;
              break;
      case 3: return 3*ditLength;
              break;
      default: return interCharacterSpace;
              break;
    }
}

int pitch() {
    int p = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
    if (alternatePitch && morseState == morseGenerator)
      p = (MorsePreferences::pliste[posEchoToneShift].value == 1 ? p * 18 / 17 : p * 17 / 18);
    return p;
}





void dispGeneratedChar() {
  static String charString;
  charString.reserve(10);

  if (generatorMode == KOCH_LEARN ||
          (MorsePreferences::pliste[posGeneratorDisplay].value == DISPLAY_BY_CHAR &&
          (morseState == loraTrx || morseState == wifiTrx || morseState == morseGenerator || playCW)) ||
          (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value != CODE_ONLY ))
      {
        if (clearText.charAt(0) == 0xC3) {
          charString = String(clearText.charAt(0)) + String(clearText.charAt(1));
          clearText.remove(0,2);
        }
        else {
          charString = clearText.charAt(0);
          clearText.remove(0,1);
        }
        if (generatorMode == KOCH_LEARN) {
            displayGeneratedMorse(REGULAR," ");

        }
        displayGeneratedMorse((morseState == loraTrx || morseState == wifiTrx || generatorMode == KOCH_LEARN) ? BOLD : REGULAR, cleanUpProSigns(charString));
        if (generatorMode == KOCH_LEARN)
            displayGeneratedMorse(REGULAR," ");
      }

      ++charCounter;


     if (charCounter == 12) {
        MorsePreferences::fireCharSeen(true);
     }
}

void fetchNewWord() {
  int rssi, rxWpm, rv;
  char numBuffer[16];


    if (morseState == loraTrx || morseState == wifiTrx) {
       MorseOutput::updateSMeter(0);
       startFirst = false;

        if (cwBuReady()) {
            uint8_t header = decodePacket(&rssi, &rxWpm, &CWword);
            if (header == 0) {
              DEBUG("Invalid LoRa packet: EOW within packet!");
              return;
            }
            if ((header >> 6) != 1)
              return;
            if ((rxWpm < 5) || (rxWpm >60))
              return;

            displayGeneratedMorse(BOLD, " ");
            clearText = CWwordToClearText(CWword);
            rxDitLength = 1200 / rxWpm ;
            rxDahLength = 3* rxDitLength ;
            rxInterCharacterSpace = 3 * rxDitLength;
            rxInterWordSpace = 7 * rxDitLength;
            sprintf(numBuffer, "%2ir", rxWpm);
            MorseOutput::printOnStatusLine( true, 4, numBuffer);
            MorseOutput::printOnStatusLine( true, 9, "s");
            MorseOutput::updateSMeter(rssi);
       }
       else return;

    }
    else if (playCW) {
        clearText = getM32PWord();
        if (clearText == "") {
          playCW = false;
          return;
        }
        clearText.replace("T", "");
        if (clearText.indexOf('P') != -1) {
            genTimer = 3 * interWordSpace + millis();
            clearText = "";
        }
        CWword = generateCWword(clearText);
        displayGeneratedMorse(REGULAR, " ");
        return;
    }
    else {
    if ((morseState == morseGenerator) ) {
        displayGeneratedMorse(REGULAR, " ");
    }

    if (generatorMode == KOCH_LEARN) {
        startFirst = false;
        echoTrainerState = SEND_WORD;
    }
    if (startFirst == true) {
        clearText = "vvvA";
        startFirst = false;
    } else if (morseState == morseGenerator && MorsePreferences::pliste[posWordDoubler].value != 0 && firstTime == false) {
        clearText = echoTrainerWord;
        firstTime = true;
    } else if (morseState == morseGenerator && autoStop == repeatword) {
        clearText = echoTrainerWord;
        autoStop = nextword;
    } else if (morseState == echoTrainer) {
        interWordTimer = 4294967000;

        switch (echoTrainerState) {
            case REPEAT_WORD: if (MorsePreferences::pliste[posEchoRepeats].value == 7 || repeats <= MorsePreferences::pliste[posEchoRepeats].value)
                                    clearText = echoTrainerWord;
                                else {
                                    clearText = echoTrainerWord;
                                    if (generatorMode != KOCH_LEARN) {
                                        displayGeneratedMorse(INVERSE_REGULAR, cleanUpProSigns(clearText));
                                        displayGeneratedMorse(REGULAR, " ");
                                    }
                                    goto randomGenerate;
                                }
                                break;

            case SEND_WORD: goto randomGenerate;
            default: break;
        }
    } else {

      randomGenerate: repeats = 0;
                            clearText = "";
                            if ((MorsePreferences::pliste[posMaxSequence].value != 0) && (generatorMode != KOCH_LEARN))
                              if ( morseState == echoTrainer || ((morseState == morseGenerator) && !MorsePreferences::pliste[posAutoStop].value) ) {

                                ++ wordCounter;
                                int limit = 1 + MorsePreferences::pliste[posMaxSequence].value;
                                if (wordCounter == limit) {
                                  clearText = "+";
                                  echoStop = true;
                                  if (echoTrainerState == REPEAT_WORD)
                                    echoTrainerState = SEND_WORD;
                                }
                                else if (wordCounter == (limit+1)) {
                                    stopFlag = true;
                                    echoStop = false;

                                }
                            }
                            if (clearText != "+") {
                                switch (generatorMode) {
                                      case RANDOMS: clearText = getRandomChars(MorsePreferences::pliste[posRandomLength].value, MorsePreferences::pliste[posRandomOption].value);
                                                      break;
                                      case CALLS: clearText = getRandomCall(MorsePreferences::pliste[posCallLength].value);
                                                      break;
                                      case ABBREVS: clearText = getRandomAbbrev(MorsePreferences::pliste[posAbbrevLength].value);
                                                      break;
                                      case WORDS: clearText = getRandomWord(MorsePreferences::pliste[posWordLength].value);
                                                      break;
                                      case KOCH_LEARN:clearText = koch.getNewChar();
                                                      break;
                                      case MIXED: rv = random(4);
                                                      switch (rv) {
                                                        case 0: clearText = getRandomWord(MorsePreferences::pliste[posWordLength].value);
                                                                  break;
                                                        case 1: clearText = getRandomAbbrev(MorsePreferences::pliste[posAbbrevLength].value);
                                                                    break;
                                                        case 2: clearText = getRandomCall(MorsePreferences::pliste[posCallLength].value);
                                                                  break;
                                                        case 3: clearText = getRandomChars(1,OPT_PUNCTPRO);
                                                                  break;
                                                      }
                                                      break;
                                      case KOCH_MIXED:rv = random(3);
                                                      switch (rv) {
                                                        case 0: clearText = getRandomWord(MorsePreferences::pliste[posWordLength].value);
                                                                  break;
                                                        case 1: clearText = getRandomAbbrev(MorsePreferences::pliste[posAbbrevLength].value);
                                                                    break;
                                                        case 2: clearText = getRandomChars(MorsePreferences::pliste[posRandomLength].value, OPT_KOCH);
                                                                  break;
                                                      }
                                                      break;
                                      case KOCH_ADAPTIVE:clearText = getRandomChars(MorsePreferences::pliste[posRandomLength].value, OPT_KOCH_ADAPTIVE);
                                                      break;
                                      case PLAYER: if (MorsePreferences::pliste[posRandomFile].value != 0)
                                                          skipWords(random(256));
                                                      if (lastWord == "")
                                                          clearText = getWord();
                                                      else {
                                                          clearText = lastWord;
                                                          lastWord = "";
                                                      }


                                                      break;
                                    }
                            }
                            firstTime = false;
      }


      if (clearText.indexOf('P') != -1) {
        genTimer = 3 * interWordSpace + millis();
        clearText = "";

      }
      if (clearText.indexOf('T') != -1) {
        alternatePitch = !alternatePitch;
        clearText = "";

      }
      CWword = generateCWword(clearText);
      echoTrainerWord = clearText;
    }
}




void displayDecodedMorse(String symbol, boolean keyed) {
  String tmp_str = symbol;
  if (MorsePreferences::pliste[posOutputCase].value) {
    tmp_str.toUpperCase();
  }

  MorseOutput::printToScroll( REGULAR, tmp_str, true, encoderState == scrollMode);

  SerialOutMorse(tmp_str, keyed ? 0b001 : 0b010);

#ifdef CONFIG_BLUETOOTH_KEYBOARD
  if ((MorsePreferences::pliste[posBluetoothOut].value & 0x6) >= 0x2)
    MorseBluetooth::bluetoothTypeString(tmp_str);
#endif

  if (morseState == echoTrainer) {
    symbol.replace("<as>", "S");
    symbol.replace("<ka>", "A");
    symbol.replace("<kn>", "N");
    symbol.replace("<sk>", "K");
    symbol.replace("<ve>", "E");
    symbol.replace("<ch>", "H");
    symbol.replace("<bk>", "B");
    symbol.replace("<err>", "R");
    if (symbol == "R")

      echoResponse="";
    else if (symbol != " ")
      echoResponse.concat(symbol);

  }
}





void displayGeneratedMorse(FONT_ATTRIB style, String s)
{
 if (MorsePreferences::pliste[posOutputCase].value) {
  s.toUpperCase();
 }
 MorseOutput::printToScroll(style, s, true, encoderState == scrollMode);
 SerialOutMorse(s, 0b100);
#ifdef CONFIG_BLUETOOTH_KEYBOARD
 if ((MorsePreferences::pliste[posBluetoothOut].value & 0x6) >= 0x2)
  MorseBluetooth::bluetoothTypeString(s);
#endif
}



void SerialOutMorse(String s, uint8_t origin) {
  uint8_t bitmap = (MorsePreferences::pliste[posSerialOut].value < 5 ? MorsePreferences::pliste[posSerialOut].value : 7);
  if (origin & bitmap)
      Serial.print(s);
}





void displayCWspeed() {
  char numBuf[16];
  uint8_t wpm;

  wpm = lastWasKey ? keyDecoder.getWpm() : audioDecoder.getWpm();
  if ((morseState == echoTrainer && MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY))
    sprintf(numBuf, "(%2i)", wpm);
  else if (( morseState == morseGenerator || morseState == echoTrainer ))
    sprintf(numBuf, "(%2i)", effWpm);
  else if (morseState == morseTrx )
    sprintf(numBuf, "r%2is", audioDecoder.getWpm());
  else sprintf(numBuf, "    ");

  MorseOutput::printOnStatusLine(false, 3, numBuf);

  if (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY &&
      (morseState == morseKeyer || morseState == loraTrx || morseState == wifiTrx ))
        sprintf(numBuf, "%2i", wpm);
  else
    sprintf(numBuf, "%2i", (morseState == morseDecoder ? wpm : MorsePreferences::wpm));
  MorseOutput::printOnStatusLine(encoderState == speedSettingMode ? true : false, 7, numBuf);
  MorseOutput::printOnStatusLine(false, 10, "WpM");
  MorseOutput::refreshDisplay();
}


void updateTopLine() {
  String symbol;
  symbol.reserve(5);

  MorseOutput::clearStatusLine();

  if (morseState == morseGenerator) {
    if (MorsePreferences::pliste[posWordDoubler].value != 0)
      symbol = "x2 ";
    else if (MorsePreferences::pliste[posAutoStop].value)
      symbol = "<> ";
    else
      symbol = "   ";
    MorseOutput::printOnStatusLine(true, 1, symbol);
  }
  else
    MorseOutput::printOnStatusLine(false, 2, getKeyerModeSymbol() + " ");

  displayCWspeed();
  if ((morseState == loraTrx ) || (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value == 2))
    MorseOutput::dispLoraLogo();
  else if ((morseState == wifiTrx) || (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value == 1))
      MorseOutput::dispWifiLogo();

  MorseOutput::displayVolume(encoderState == speedSettingMode, MorsePreferences::sidetoneVolume);
  MorseOutput::refreshDisplay();
}


String getKeyerModeSymbol() {
  const char symbols[] = " ABUNS";
  return String(symbols[MorsePreferences::pliste[posCurtisMode].value]);
}


void echoTrainerEval() {
    int i;
    delay(interCharacterSpace / 2);

    if (echoResponse == echoTrainerWord) {
      echoTrainerState = SEND_WORD;
      displayGeneratedMorse(BOLD, "OK");
      if (MorsePreferences::pliste[posEchoConf].value) {
          MorseOutput::soundSignalOK();
      }
      if (kochActive){
        koch.decreaseWordProbability(echoTrainerWord);
      }
      delay(16*ditLength + interWordSpace/12);
      if (MorsePreferences::pliste[posSpeedAdapt].value)
          changeSpeed(1);
    } else {
      echoTrainerState = REPEAT_WORD;
      if (generatorMode != KOCH_LEARN || echoResponse != "") {
          ++errCounter;
          displayGeneratedMorse(BOLD, "ERR");
          if (MorsePreferences::pliste[posEchoConf].value) {
              MorseOutput::soundSignalERR();
          }
          if (kochActive){
            koch.increaseWordProbability(echoTrainerWord, echoResponse);
          }
      }
      delay(16*ditLength + interWordSpace/12);
      if (MorsePreferences::pliste[posSpeedAdapt].value)
          changeSpeed(-1);
    }
    echoResponse = "";
    clearPaddleLatches();
}


void updateTimings() {
  ditLength = 1200 / MorsePreferences::wpm;
  dahLength = 3 * ditLength;
  interCharacterSpace = MorsePreferences::pliste[posInterCharSpace].value * ditLength;
  halfICS = (3 + MorsePreferences::pliste[posInterCharSpace].value) * ditLength / 2;
  interWordSpace = _max(MorsePreferences::pliste[posInterWordSpace].value, MorsePreferences::pliste[posInterCharSpace].value+4) * ditLength;
  effWpm = 60000 / (31 * ditLength + 4 * interCharacterSpace + interWordSpace );
}

void changeSpeed( int t) {
  MorsePreferences::wpm += t;
  MorsePreferences::wpm = constrain(MorsePreferences::wpm, MorsePreferences::wpmMin, MorsePreferences::wpmMax);
  updateTimings();
  if (m32state != menu_loop)
      displayCWspeed();
  charCounter = 0;
  if (m32protocol)
      MorseJSON::jsonControl("speed", MorsePreferences::wpm, MorsePreferences::wpmMin, MorsePreferences::wpmMax, false);
}


void changeVolume( int t) {
    MorsePreferences::sidetoneVolume += t+1;
    MorsePreferences::sidetoneVolume = constrain(MorsePreferences::sidetoneVolume, 1, 20) -1;

    if (m32state != menu_loop)
        MorseOutput::displayVolume((encoderState == volumeSettingMode ? false : true), MorsePreferences::sidetoneVolume);
    if (m32protocol)
      MorseJSON::jsonControl("volume", MorsePreferences::sidetoneVolume, MorsePreferences::volumeMax, MorsePreferences::volumeMin, false);
}

void keyTransmitter(boolean noTx) {
  if (noTx )
      return;
   digitalWrite(keyerPin, HIGH);
#ifdef CONFIG_BLUETOOTH_KEYBOARD
   if ((MorsePreferences::pliste[posBluetoothOut].value & 0x1) == 0x1)
      MorseBluetooth::bluetoothTypeLCTRL(true);
#endif
}

String cleanUpProSigns( String &input ) {

    input.replace("S", "<as>");
    input.replace("A", "<ka>");
    input.replace("N", "<kn>");
    input.replace("K", "<sk>");
    input.replace("E", "<ve>");
    input.replace("B", "<bk>");
    input.replace("H", "ch");
    input.replace("R", "<err>");
    input.replace("U", "*");

    return input;
}



int16_t batteryVoltage() {
#ifdef PIN_ADC_CTRL
  #define ADC_Ctrl PIN_ADC_CTRL
  pinMode(ADC_Ctrl,OUTPUT);
#endif
#ifdef PIN_VEXT

      if (MorsePreferences::boardVersion == 3)
         digitalWrite(PIN_VEXT,VEXT_ON_VALUE);

      else if (MorsePreferences::boardVersion == 4)
         digitalWrite(PIN_VEXT,! VEXT_ON_VALUE);
#endif
#ifdef ARDUINO_heltec_wifi_kit_32_V3
      digitalWrite(ADC_Ctrl,LOW);
        delay(100);
#endif
      double v= 0; int counts = 4;
      for (int i=0; i<counts ; ++i) {
         v+= ReadVoltage(batteryPin);
         delay(8);
      }
#ifdef ARDUINO_heltec_wifi_kit_32_V3
        digitalWrite(ADC_Ctrl,HIGH);
#endif
      v /= counts;

  #ifndef ARDUINO_heltec_wifi_kit_32_V3
      if (MorsePreferences::boardVersion == 4)
        v *= 1.1;
  #endif

      voltage_raw = v;
  #ifndef ARDUINO_heltec_wifi_kit_32_V3
      v *= (MorsePreferences::vAdjust * VOLT_CALIBRATE);
  #else
      v = v - 200 + MorsePreferences::vAdjust;
      v *= VOLT_CALIBRATE;
  #endif
      return (int16_t) v;

}



double ReadVoltage(byte pin){
  adcAttachPin(batteryPin);
  analogSetClockDiv(128);
  analogSetPinAttenuation(batteryPin,ADC_11db);
  double reading = analogRead(pin);
  analogSetClockDiv(1);


  if(reading < 4 || reading > 4092)
    return 0;

#ifndef ARDUINO_heltec_wifi_kit_32_V3

  return (-0.000000000000016 * pow(reading,4) + 0.000000000118171 * pow(reading,3)- 0.000000301211691 * pow(reading,2)+ 0.001109019271794 * reading + 0.034143524634089);

#else

  if (reading < 0.838)
    reading = reading - (reading-616)/4.5;
  else
    reading = reading - (1006 - reading)/4.5;

  return reading;

#endif
}



void checkShutDown(boolean enforce) {
  unsigned long timeOut;
  if (m32protocol && !enforce)
    return;
  if (MorsePreferences::pliste[posTimeOut].value || enforce) {
      timeOut = 300000UL * MorsePreferences::pliste[posTimeOut].value;
      if ((millis() - MorseOutput::TOTcounter) > timeOut || enforce == true ) {
          MorseOutput::clearDisplay();
          MorseOutput::printOnScroll(1, INVERSE_BOLD, 0, "Power OFF...");
          MorseOutput::printOnScroll(2, REGULAR, 0, "FN to turn ON");
          if (m32protocol)
                  MorseJSON::jsonCreate("message", "Power off", "");
          MorseOutput::refreshDisplay();
          delay (1500);
          shutMeDown();
      }
  }
}

void shutMeDown() {
  MorseOutput::sleep();
  if (m32protocol)
    MorseJSON::jsonError("M32 SLEEP SHUTDOWN BY USER");

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
#ifdef LORA_RADIOLIB
  radio.sleep();
#endif
  WiFi.disconnect(true, false);
  delay(100);
  MorseOutput::soundSuspend();
#ifdef PIN_VEXT
  digitalWrite(PIN_VEXT, ! VEXT_ON_VALUE);
  delay(100);
#endif





  esp_deep_sleep_start();
  esp_restart();
return;
}
# 2416 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
void cwForTx (int element) {
  static int pairCounter = 0;
  uint8_t temp;
  uint8_t header;
  uint8_t wpm;

  if (pairCounter == 0) {
      for (int i=0; i<sizeof(cwTxBuffer); ++i)
          cwTxBuffer[i] = (char) 0;

      header = ++cwTxSerial % 64;

      header += CW_TRX_PROTO_VERSION * 64;
      cwTxBuffer[0] = header;
      wpm = (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY && morseState != morseGenerator) ? keyDecoder.getWpm() : MorsePreferences::wpm;
      temp = wpm * 4;
      cwTxBuffer[1] |= temp;
      pairCounter = 7;
      }
      else if (pairCounter > sizeof(cwTxBuffer)*4 -4) {
          return;
      }

  temp = element & B011;

  if (temp && (temp != 3)) {
      temp = temp << (2*(3-(pairCounter % 4)));
      cwTxBuffer[pairCounter/4] |= temp;
  }




  if (temp != 3)
      ++pairCounter;
  else {
      --pairCounter;
      if (pairCounter % 4 != 0) {
          temp = temp << (2*(3-(pairCounter % 4)));
          cwTxBuffer[pairCounter/4] |= temp;
      }
      pairCounter = 0;
  }
}


void sendWithLora() {
#ifdef LORA_RADIOLIB
  int state = radio.transmit(cwTxBuffer);
  if (state != RADIOLIB_ERR_NONE) {
    DEBUG(F("Lora TX ERROR!"));
  }
  if (morseState == loraTrx)
    loraReceived=false;
    radio.startReceive();
#endif
}

void sendWithWifi() {
# 2486 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
  if (!MorsePreferences::useEspNow) {
    MorseWiFi::audp.writeTo((uint8_t*)cwTxBuffer, strlen(cwTxBuffer), peerIP, MORSERINOPORT);



  }
  else {
      quickEspNow.send (ESPNOW_BROADCAST_ADDRESS, (uint8_t*)cwTxBuffer, strlen(cwTxBuffer));
  }
}

void onLoraReceive(){
#ifdef LORA_RADIOLIB
  int packetSize = radio.getPacketLength();
  u_int maxl = sizeof(cwTxBuffer) < packetSize ? sizeof(cwTxBuffer) : packetSize;
  String result;
  String reason = "";
  reason.reserve(9);
  result.reserve(sizeof(cwTxBuffer));
  result = "";


  int state = radio.readData(result, maxl);
  if (state == RADIOLIB_ERR_NONE) {




    if (sizeof(cwTxBuffer) < packetSize)
      reason = "LENGTH";
    if ((result.charAt(0) & 0b11000000) != 0b01000000) {
      reason = "PROT.VER";
      goto error;
    }
    if ((result.charAt(1) & 0b00000011) && packetSize <= sizeof(cwTxBuffer)) {

        storePacket(radio.getRSSI(), result);
        return;
    }
    if (reason == "")
      reason = "EOFC";
    error: DEBUG("Invalid LoRa Packet: " + reason + "! Discarded...");
  } else {
    DEBUG(F("[SX12xx] INVALID PACKET!"));
  }
#endif
}

void onWifiReceive(AsyncUDPPacket packet) {
  u_int maxl = sizeof(cwTxBuffer) < packet.length() ? sizeof(cwTxBuffer) : packet.length();
  String result;

  result.reserve(sizeof(cwTxBuffer));
  result = "";


  if (packet.length() == 0)
    return;
  for (int i = 0; i < maxl; i++) {
    result += (char)packet.data()[i];
  }




  if (packet.length() <= sizeof(cwTxBuffer))
      storePacket(WiFi.RSSI(), result);
  else
      DEBUG("UDP Packet longer than sizeof(cwTxBuffer) bytes! Discarded...");
}

void onEspnowRecv(const uint8_t* mac, const uint8_t* data, uint8_t len, signed int rssi, bool broadcast)
{
    if (morseState != wifiTrx)
    return;
  u_int maxl = sizeof(cwTxBuffer) < len ? sizeof(cwTxBuffer) : len;
  String result;

  result.reserve(sizeof(cwTxBuffer));
  result = "";

  if (len == 0 || !broadcast)
    return;
  for (int i = 0; i < maxl; i++) {
    result += (char)data[i];
  }


  if (len <= sizeof(cwTxBuffer))
      storePacket(rssi, result);
  else
      DEBUG("ESPNOW Packet longer than sizeof(cwTxBuffer) bytes! Discarded...");
}

String CWwordToClearText(String cwword) {
  int ptr = 0;
  String result;
  result.reserve(40);
  String symbol;
  symbol.reserve(5);

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

    input.replace("<as>","S");
    input.replace("<ka>","A");
    input.replace("<kn>","N");
    input.replace("<sk>","K");
    input.replace("<ve>","E");
    input.replace("<bk>","B");
    input.replace("<ch>","H");
    input.replace("<err>","R");
    input.replace("*", "U");

    return input;
}
# 2653 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
uint8_t cwBuWrite(int rssi, String packet) {



  uint8_t l, posRssi;

  posRssi = (uint8_t) abs(rssi);
  l = 2 + packet.length();
  if (byteBuFree < l)
      return 0;
  cwRxBuffer[nextBuWrite++] = l;
  cwRxBuffer[nextBuWrite++] = posRssi;
  for (int i = 0; i < packet.length(); ++i) {
    cwRxBuffer [nextBuWrite++] = packet[i];
  }
  byteBuFree -= l;
  return l;
}

boolean cwBuReady() {
  if (byteBuFree == sizeof(cwRxBuffer))
    return (false);
  else {
    return true;
  }
}


uint8_t cwBuRead(uint8_t* buIndex) {

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



void storePacket(int rssi, String packet) {
  if (cwBuWrite(rssi, packet) == 0)
    DEBUG("cw Buffer full");
}
# 2711 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
uint8_t decodePacket(int* rssi, int* wpm, String* cwword) {
  uint8_t l, c, header=0;
  uint8_t index = 0;
  int i;

  l = cwBuRead(&index);
  for (i = 0; i < l; ++i) {
    c = cwRxBuffer[index+i];

    switch (i) {
      case 0: * rssi = (int) (-1 * c);
                break;
      case 1: header = c;
                break;
      case 2: *wpm = (uint8_t) (c >> 2);

                *cwword = (char) ((c & B011) +48);
                break;
      default:
                for (int j = 0; j < 4; ++j) {
                    char cc = ((c >> 2*(3-j)) & B011) ;
                    if (cc != 3) {
                        *cwword += (char) (cc + 48);
                    }
                    else goto endloop;
                }
                break;
    }
  }
  endloop:
    if (i < (l-1)) {
      *cwword = "";
      return 0;
    }

  return header;
}




void keyOut(boolean on, boolean fromHere, int f, int volume) {




  static boolean intTone = false;
  static boolean extTone = false;

  static int intPitch, extPitch;

  boolean noTx = true;
  switch (MorsePreferences::pliste[posKeyExternalTx].value) {
      case 0:
              break;
      case 1: if ((morseState == morseKeyer || morseState == morseTrx) && fromHere)
                noTx = false;
              break;
      case 2: if ((morseState == morseKeyer || morseState == morseGenerator) && fromHere)
                noTx = false;
              break;
      case 3: if ( ((morseState == morseKeyer || morseState == morseGenerator || morseState == morseTrx) && fromHere) ||
                   ((morseState == loraTrx || morseState == wifiTrx) && !fromHere) )
                noTx = false;
      default: break;
  }


  if (on) {
      if (fromHere) {
        intPitch = f;
        intTone = true;
        MorseOutput::pwmTone(intPitch, volume, true);
      } else {
        extTone = true;
        extPitch = f;
        if (!intTone)
          MorseOutput::pwmTone(extPitch, volume, MorsePreferences::pliste[posExtAudioOnDecode].value);
        }
      keyTransmitter(noTx);

  } else {
        if (fromHere) {
          intTone = false;
          if (extTone)
            MorseOutput::pwmTone(extPitch, volume, MorsePreferences::pliste[posExtAudioOnDecode].value);
          else
            MorseOutput::pwmNoTone(volume);
        } else {
          extTone = false;
          if (!intTone)
            MorseOutput::pwmNoTone(volume);
        }
        digitalWrite(keyerPin, LOW);
#ifdef CONFIG_BLUETOOTH_KEYBOARD
        if ((MorsePreferences::pliste[posBluetoothOut].value & 0x1) == 0x1)
          MorseBluetooth::bluetoothTypeLCTRL(false);
#endif
  }
}



void audioLevelAdjust() {
    uint16_t i, maxi, mini;
    uint16_t dataSize = 1216;
    uint16_t testData[dataSize];

  #ifdef CONFIG_SOUND_I2S
    return;
  #endif

    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, BOLD, 0, "Audio In Adj.");
    MorseOutput::printOnScroll(1, REGULAR, 0, "End with FN");

    keyOut(true, true, 698, 0);
    while (true) {
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks)
            break;
        for (i = 0; i < dataSize ; ++i)
            testData[i] = analogRead(audioInPin);
        maxi = mini = testData[0];
        for (i = 1; i< dataSize ; ++i) {
            if (testData[i] < mini)
              mini = testData[i];
            if (testData[i] > maxi)
              maxi = testData[i];
        }

        MorseOutput::showVolumeScope(mini, maxi);
    }
    keyOut(false, true, 698, 0);

}



void memoryKeyer() {


    memList[0] = maxMemCount = 0;
    for (int i = 0; i<8; ++i) {
      if (MorsePreferences::cwMemMask & 1 << i) {
        memList[maxMemCount+1] = i+1;
        ++maxMemCount;
      }
    }




    MorseOutput::clearStatusLine();
    if (maxMemCount == 0) {
      MorseOutput::printOnStatusLine(true, 0, "No memories stored");
      if (m32protocol)
          MorseJSON::jsonCreate("message", "No memories stored!", "");
      delay(500);
      updateTopLine();
      return;
    } else {
      dispMem(memList[ptr]);
      encoderState = memSelMode;
      return;
    }
}


void dispMem(int8_t memNo) {
  MorseOutput::clearStatusLine();
  if (memNo == 0) {
    MorseOutput::printOnStatusLine(true, 0, "EXIT");
    if (m32protocol) MorseJSON::jsonCreate("message", "Click to Exit", "");
  }
  else {
    String Number = (memNo < 3 ? "R" : "_") + String(memNo) + ": " ;
    String Value = Number + String(MorsePreferences::cwMem[memNo-1]);
    Value = Value.substring(0,18);
    MorseOutput::printOnStatusLine(false, 0, Value);
    if (m32protocol) MorseJSON::jsonCreate("message", "Memory " + Value, "");
    }
}


void preparePlay(int8_t memNo) {
    playCWBuffer = String(MorsePreferences::cwMem[memNo-1]);
    playCWIndex = 0;
    playCWMaxIndex = playCWBuffer.length();
    startFirst = false;
    playCW = true;
    if ((memNo < 3))
            repeatPlayCW = true;
}



String getM32PWord() {
  String result = "";
  result.reserve(128);
  byte c;

  if (playCWIndex >= playCWMaxIndex)
    if (repeatPlayCW)
      playCWIndex = 0;
    else
      return "";
  while (playCWIndex < playCWMaxIndex) {
    c = playCWBuffer.charAt(playCWIndex);
    ++playCWIndex;
    if (isSpace(c))
      break;
    else
      result += (char) c;
  }
  result = cleanUpText(result);
  return result;
}




String getWord() {
  String result = "";
  result.reserve(250);
  byte c;
  static boolean eof = false;

  if (eof) {
    eof = false;

    return result;
  }
  while (file.available()) {
      c=file.read();
      if (!isSpace(c))
        result += (char) c;
      else if (result.length() > 0) {
        ++MorsePreferences::fileWordPointer;
        result = cleanUpText(result);
        if (result.indexOf('C') != -1) {
          result = "";
          while (file.available()) {
            char c = file.read();
            if (c == '\n') {
              break;
            }
          }
        }
        else return result;
      }
    }
    eof = true;

    file.close(); file = SPIFFS.open("/player.txt");
    MorsePreferences::fileWordPointer = 0;
    while (!file.available())
      ;
    return result;
}



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

      for (unsigned int i = 0; i<w.length(); ++i) {
        if ( usedChars.indexOf(c = w.charAt(i)) == -1 )
          usedChars += c;
      }
  }
  file.close(); file = SPIFFS.open("/player.txt");

  return usedChars;
}


String cleanUpText(String text) {
  String result = "";
  char c;
  result.reserve(128);
  text.toLowerCase();
  text = utf8umlaut(text);

  for (unsigned int i = 0; i<text.length(); ++i) {
    if ((koch.morserinoKochChars.indexOf(c = text.charAt(i)) != -1) || c == 'P' || c == 'T' || c == 'C')
      result += c;
  }
  return result;
}


String utf8umlaut(String s) {
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
      s.replace("<bk>", "B");
      s.replace("<bt>", "=");
      s.replace("<as>", "S");
      s.replace("<ka>", "A");
      s.replace("<kn>", "N");
      s.replace("<sk>", "K");
      s.replace("<ve>", "E");
      s.replace("<c>", "C");
      s.replace("<p>", "P");
      s.replace("<t>", "T");
      s.replace("\\ar", "+");
      s.replace("\\bk", "B");
      s.replace("\\bt", "=");
      s.replace("\\as", "S");
      s.replace("\\ka", "A");
      s.replace("\\kn", "N");
      s.replace("\\sk", "K");
      s.replace("\\ve", "E");
      s.replace("\\c", "C");
      s.replace("\\p", "P");
      s.replace("\\t", "T");
      return s;
}


void skipWords(uint32_t count) {
  while (count > 0) {
    getWord();
    --count;
  }
}
# 3063 "/Users/wkraml/Documents/GitHub/Morserino-32/Software/src/Version 6/m32_v6.ino"
void serialEvent() {
      while (Serial.available()) {

        char inChar = (char)Serial.read();

        inputString += inChar;


        if (inChar == '\n') {
          stringComplete = true;
          break;
        }
      }
      if (stringComplete) {
              inputString.trim();

              if (m32protocol)
                serialDecode(inputString);
              else {
                inputString.toLowerCase();
                if (inputString == "put device/protocol/on") {
                  m32protocol = true;
                  MorseJSON::jsonDevice(brd,vsn);
                }
              }

          inputString = "";
          stringComplete = false;
      }
}






void serialDecode(String input) {
  String command;
  String firstArg, secondArg, thirdArg;

command.reserve(12);
firstArg.reserve(20);
secondArg.reserve(20);
thirdArg.reserve(20);

  if (!m32protocol)
    return;
  input.trim();
  int blank = input.indexOf(" ");

  if (blank != -1) {
    command = input.substring(0,blank);
    command.toLowerCase();
    String argument = input.substring(blank+1);
    int slash = argument.indexOf("/");
    if (slash != -1) {
      firstArg = argument.substring(0,slash);
      firstArg.toLowerCase();
      secondArg = argument.substring(slash+1);

      slash = secondArg.indexOf("/");
          if (slash != -1) {
            thirdArg = secondArg.substring(slash+1);
            secondArg = secondArg.substring(0,slash);
            secondArg.toLowerCase();
          }
          else {
            secondArg.toLowerCase();
            thirdArg = "";
          }

      }
      else {
        firstArg = argument;
        firstArg.toLowerCase();
        secondArg = "";
      }


  }
  else {
    MorseJSON::jsonError("MISSING ARGUMENT");
    return;
  }


  if (command == "get")
    m32Get(firstArg, secondArg, thirdArg);
  else if (command == "put")
    m32Put(firstArg, secondArg, thirdArg);
  else
    MorseJSON::jsonError("COMMAND NOT RECOGNIZED");
}


void m32Get(String type, String token, String value) {


    if (type == "") {
        MorseJSON::jsonError("ARGUMENT(S) MISSING");
        return;
    }
    if (type == "control") {
        if (token == "speed")
          MorseJSON::jsonControl("speed", MorsePreferences::wpm, MorsePreferences::wpmMin, MorsePreferences::wpmMax, true);
        else if (token == "volume")
          MorseJSON::jsonControl("volume", MorsePreferences::sidetoneVolume, MorsePreferences::volumeMax, MorsePreferences::volumeMin, true);
        else
          MorseJSON::jsonError("INVALID ARGUMENT");
    }
    else if (type == "controls")
        MorseJSON::jsonControls();
    else if (type == "device")
        MorseJSON::jsonDevice(brd,vsn);
    else if (type == "menu")

        MorseJSON::jsonMenu(MorseMenu::getMenuPath(MorsePreferences::newMenuPtr), (int)MorsePreferences::newMenuPtr,
              (m32state == menu_loop ? false : true), MorseMenu::isRemotelyExecutable(MorsePreferences::newMenuPtr));
    else if (type == "menus")
        MorseJSON::jsonMenuList();
    else if (type == "config")
        MorseJSON::jsonParameter(token);
    else if (type == "configs")
        MorseJSON::jsonParameterList();
    else if (type == "snapshots")
        MorseJSON::jsonSnapshots();
    else if (type == "file") {
            if (token == "size")
                MorseJSON::jsonFileStats();
            else if (token == "text")
                MorseJSON::jsonFileText();
            else if (token == "first line" || token == "")
                MorseJSON::jsonFileFirstLine();
    }
    else if (type == "wifi") {
      MorseJSON::jsonGetWifi();
    }
    else if (type == "kochlesson") {
      MorseJSON::jsonGetKoch();
    }
    else if (type == "cw") {
      if (token == "memories") {
        MorseJSON::jsonGetCwStores();
      }
      else if (token == "memory") {
        MorseJSON::jsonGetCwStore(value);
      }
      else
        MorseJSON::jsonError("GET CW " + token + ": INVALID ARGUMENT");
    }
    else

        MorseJSON::jsonError("GET " + type + ": UNKNOWN COMMAND");
}


void m32Put(String type, String token, String value) {


    uint8_t number;
    if (token == "" || type == "") {
          MorseJSON::jsonError("NOT ENOUGH ARGUMENTS");
          return;
    }

    if (type == "control") {
         if (token == "speed") {
            int i = value.toInt() - MorsePreferences::wpm;
            changeSpeed(i);
         } else if (token == "volume") {
            int i = value.toInt() - MorsePreferences::sidetoneVolume;
            changeVolume(i);
         } else
            MorseJSON::jsonError("INVALID NAME " + token);
    }

    else if (type == "device") {
      if (token == "protocol") {
        if (value == "off") {
            MorseJSON::jsonCreate("end m32protocol", "Goodbye!", "");
            m32protocol = false;
        }
        else if (value == "on")
            MorseJSON::jsonDevice(brd,vsn);
        else
            MorseJSON::jsonError("INVALID Value " + value);
      }
      else MorseJSON::jsonError("INVALID NAME " + token);
    }

    else if (type == "config") {
      if (setParameter(token, value))
        MorseJSON::jsonError("ERROR setting parameter " + token);
      else
        MorseJSON::jsonOK();
    }

    else if (type == "snapshot") {
      if (token == "store") {
        int i = value.toInt() -1;
        if (i >= 0 && i <= 7 ) {
            MorsePreferences::doWriteSnapshot(i, MorsePreferences::menuPtr);
            MorseJSON::jsonOK();
        }
        else {
            MorseJSON::jsonError("INVALID SNAPSHOT NUMBER");

        }
      }
      else if (token == "clear" || token == "recall") {
         int i = value.toInt() - 1;
         if (i >= 0 && i <= 7 && MorsePreferences::memCounter > 0) {
            for (uint8_t y = 0; y < MorsePreferences::memCounter; ++y) {
              if (MorsePreferences::memories[y] == i) {
                if (token == "clear")
                    MorsePreferences::doClearMemory(y);
                else {
                    MorsePreferences::doReadSnapshot(y);
                    MorsePreferences::newMenuPtr = MorsePreferences::menuPtr;
                }
                MorseJSON::jsonOK();
                return;
              }
            }
            MorseJSON::jsonError("NO SUCH SNAPSHOT");
         } else
            MorseJSON::jsonError("NO SUCH SNAPSHOT");
      }
      else
        MorseJSON::jsonError("INVALID ACTION snapshot " + token);
    }

    else if (type == "file") {
      if (token == "new") {
        File file = SPIFFS.open("/player.txt", "w");
        file.println(value);
        file.close();
        MorsePreferences::fileWordPointer = 0;
        MorsePreferences::writeWordPointer();
        MorseJSON::jsonOK();
      }
      else if (token == "append") {
        File file = SPIFFS.open("/player.txt", "a");
        file.println(value);
        file.close();
        MorsePreferences::fileWordPointer = 0;
        MorsePreferences::writeWordPointer();
        MorseJSON::jsonOK();
      }
      else
        MorseJSON::jsonError("INVALID ACTION file " + token);
    }

    else if (type == "wifi") {
      String n = value.substring(0,1);
      int nr = n.toInt();

      if (nr < 0 || nr > 3)
        return (MorseJSON::jsonError("INVALID WIFI NUMBER"));
      if (nr == 0 && token != "select")
        return (MorseJSON::jsonError("INVALID WIFI NUMBER"));

      if (value.indexOf("/") == 1)
        value = value.substring(2);
      else value == "";

      if (token == "select")
        selectWifi(nr);
      else if (token == "ssid")
        setSsid(nr, value);
      else if (token == "password")
        setPassword(nr, value);
      else if (token == "trxpeer")
        setTrxPeer(nr, value);
      else
        MorseJSON::jsonError("INVALID PROPERTY " + token);
    }

    else if (type == "menu") {
      if (token == "stop") {
        goToMenu = true;
        MorseJSON::jsonOK();
      }
      else if (token == "start" || token == "start now") {
        if (value =="") {
          goToMenu = false;
          if (m32state == menu_loop) {
              executeMenu = true;
              if (token == "start now" && MorseMenu::isRemotelyExecutable(MorsePreferences::menuPtr))
                executeNow = true;
              MorseJSON::jsonOK();
          }
        }
        else {
          uint8_t nr = (char) value.toInt();
          if (nr > 0 && nr <= 42 && MorseMenu::isRemotelyExecutable(nr)) {
              MorsePreferences::newMenuPtr = nr;
              goToMenu = false;
              if (m32state == menu_loop) {
                  executeMenu = true;
                  if (token == "start now")
                    executeNow = true;
                  MorseJSON::jsonOK();
              }
          }
          else
            MorseJSON::jsonError("NOT EXECUTABLE - Menu No " + value);
        }
      }
      else if (token == "set") {
        uint8_t nr = (char) value.toInt();
        if (nr > 0 && nr <= 42) {
            goToMenu = true;
            MorsePreferences::menuPtr = nr;
            MorsePreferences::newMenuPtr = MorsePreferences::menuPtr;
            MorseJSON::jsonOK();
        }
        else
            MorseJSON::jsonError("INVALID argument " + value);
      }
      else
         MorseJSON::jsonError("INVALID ACTION menu " + token);
    }

    else if (type == "kochlesson") {
        uint8_t nr = (char) token.toInt();
        if (nr >= MorsePreferences::kochMinimum && nr <= MorsePreferences::kochMaximum) {
          MorsePreferences::kochFilter = nr;
          MorseJSON::jsonOK();
        }
        else
          MorseJSON::jsonError("INVALID KOCH LESSON " + token);
    }

    else if (type == "cw") {
      if (token == "play" || token == "repeat" || token == "recall") {
        if (m32state != menu_loop && (morseState == morseKeyer || morseState == morseTrx)) {
          if (token == "recall") {

            number = value.toInt();
;
            if (number < 1 || number > 8 || (MorsePreferences::cwMemMask & 1 << (number-1)) == 0)
              return (MorseJSON::jsonError("INVALID CW MEMORY NUMBER"));

            playCWBuffer = String(MorsePreferences::cwMem[number-1]);

          }
          else {
              playCWBuffer = value;
          }
          playCWIndex = 0;
          playCWMaxIndex = playCWBuffer.length();
          startFirst = false;
          playCW = true;
          if ((token == "repeat") || (token == "recall" && number < 3))
            repeatPlayCW = true;
          MorseJSON::jsonOK();

        }
        else
          MorseJSON::jsonError("CW/PLAY: Keyer not active");
      }
      else if (token == "stop")
        stopPlayCw();

      else if (token == "store") {


          int number = value.toInt();
          if (number < 1 || number > 8)
            return (MorseJSON::jsonError("INVALID CW MEMORY NUMBER"));

          if (value.indexOf("/") == 1)
            value = value.substring(2);
          else if (value.length() != 0)
            return (MorseJSON::jsonError("INVALID CW MEMORY ARGUMENT"));
          else value = "";


          value = value.substring(0,47);
          value.toCharArray(MorsePreferences::cwMem[number-1],48);
          if (value == "")
            MorsePreferences::cwMemMask &= ~(1 << (number-1));
          else
            MorsePreferences::cwMemMask |= 1 << (number-1);

          MorsePreferences::setCwMem(number, value);
          MorseJSON::jsonOK();
        }
        else
        MorseJSON::jsonError("CW: INVALID ARGUMENT " + token);
    }
    else
       MorseJSON::jsonError("COMMAND " + type + " NOT YET IMPLEMENTED");
}


boolean setParameter(String token, String value) {
    String pname; pname.reserve(20);
    bool found = false;
    int v = (int) value.toInt();


    for (uint8_t i = 0; i <= posSerialOut; ++i) {
      pname = MorsePreferences::pliste[i].parName;
      pname.toLowerCase();
      if (token != pname)
        continue;

      if (v < MorsePreferences::pliste[i].minimum || v > MorsePreferences::pliste[i].maximum)
        return true;
      MorsePreferences::pliste[i].value = v;

      if (v != 0) {
                        if (i == posWordDoubler) {
                            MorsePreferences::pliste[posAutoStop].value = 0;
                            MorsePreferences::displayValueLine(posAutoStop, MorsePreferences::pliste[posAutoStop].parName, true);
                        }
                        else if (i == posAutoStop) {
                            MorsePreferences::pliste[posWordDoubler].value = 0;
                            MorsePreferences::displayValueLine(posWordDoubler, MorsePreferences::pliste[posWordDoubler].parName, true);
                        }
                      }


      if (i == posKochSeq)
            MorsePreferences::handleKochSequence();
      else if (i == posCarouselStart)
            MorsePreferences::handleCarouselChange();
      found = true;
      break;
    }

    MorsePreferences::writePreferences("morserino");
    if (found)
      return false;
    else
      return true;
}


void setSsid(int i, String str) {
  switch (i) {
    case 1:
        MorsePreferences::wlanSSID1 = str;
        break;
    case 2:
        MorsePreferences::wlanSSID2 = str;
        break;
    case 3:
        MorsePreferences::wlanSSID3 = str;
        break;
    default:
        return;
        break;
  }
  MorsePreferences::writePreferences("morserino");
  MorseJSON::jsonOK();
}


void setPassword(int i, String str) {
  switch (i) {
    case 1:
        MorsePreferences::wlanPassword1 = str;
        break;
    case 2:
        MorsePreferences::wlanPassword2 = str;
        break;
    case 3:
        MorsePreferences::wlanPassword3 = str;
        break;
    default:
        return;
        break;
  }
  MorsePreferences::writePreferences("morserino");
  MorseJSON::jsonOK();

}


void setTrxPeer(int i, String str) {
  switch (i) {
    case 1:
        MorsePreferences::wlanTRXPeer1 = str;
        break;
    case 2:
        MorsePreferences::wlanTRXPeer2 = str;
        break;
    case 3:
        MorsePreferences::wlanTRXPeer3 = str;
        break;
    default:
        return;
        break;
  }
  MorsePreferences::writePreferences("morserino");
  MorseJSON::jsonOK();
}


void selectWifi(int i) {
    switch(i) {
      case 0: MorsePreferences::useEspNow = true;
        break;
      case 1: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID1, MorsePreferences::wlanPassword1, MorsePreferences::wlanTRXPeer1);
              MorsePreferences::useEspNow = false;
        break;
      case 2: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID2, MorsePreferences::wlanPassword2, MorsePreferences::wlanTRXPeer2);
              MorsePreferences::useEspNow = false;
      break;
      case 3: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID3, MorsePreferences::wlanPassword3, MorsePreferences::wlanTRXPeer3);
               MorsePreferences::useEspNow = false;
        break;
  }
  MorseJSON::jsonOK();
  ESP.restart();
}


void stopPlayCw() {
            keyOut(false,true,0,0);
            playCW = false;
            repeatPlayCW = false;
            playCWBuffer = clearText = CWword = "";
            playCWMaxIndex = 0;
            startFirst = true;
            MorseJSON::jsonOK();
}