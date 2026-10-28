# 1 "/tmp/tmp6rs4y27n"
#include <Arduino.h>
# 1 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
# 29 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
#include "morsedefs.h"

#include "wklfonts.h"


#include "abbrev.h"
#include "english_words.h"
#include "callsign_prefixes.h"
#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "MorseWiFi.h"
#ifdef KIP_SPIKE
#include "M32KipSpike.h"
#endif
#ifdef CONFIG_M32KIP
#include "MorseKipKeyer.h"
#endif
#include "goertzel.h"
#include "MorseDecoder.h"
#include "MorseJSON.h"
#include "M32ProtocolOut.h"
#include "MorseVoice.h"
#include "MorseWinKeyer.h"
#ifdef CONFIG_BLE_SERIAL
#include "MorseBleSerial.h"
#endif
#include <mbedtls/base64.h>





#if defined(CONFIG_CW_GAME) || defined(CONFIG_QSO_BOT)
#include "MorseGame.h"
#endif
#ifdef CONFIG_CW_GAME
#include "MorsePileup.h"
#include "MorseRadioCave.h"
#include "MorseMorsel.h"
#include "MorseGridNet.h"
#endif

#ifdef CONFIG_TFT
#include "MorseGameMode.h"
#endif

#ifdef CONFIG_PRACTICE_STATS
#include "MorsePracticeStats.h"
#endif

#ifdef CONFIG_MCP73871
#include "analog.h"
#include "esp_timer.h"
#endif

#ifdef LORA_RADIOLIB
#include <RadioLib.h>
#ifdef RADIO_SX1262
        
# 88 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
#pragma message ("Compiling with Radiolib SX1262")
# 88 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"

RADIO radio = new Module(LoRa_nss, LoRa_dio1, LoRa_nrst, LoRa_busy);
#else
        
# 91 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
#pragma message ("Compiling with Radiolib SX1278")
# 91 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"

RADIO radio = new Module(LoRa_nss, LoRa_dio0, LoRa_nrst, LoRa_dio1);
#endif
#endif

volatile bool loraReceived = false;
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void packetReceived();
void IRAM_ATTR codec_isr();
int checkEncoder();
void DEBUG (const String& s);
void setup();
boolean key_was_pressed_at_start();
boolean checkKey ();
String shortDate(char const *date);
static void splashPause(uint32_t ms);
static void announceSplash(uint16_t v);
static void announceEmptyBattery();
static void announceSplash(uint16_t v);
static void announceEmptyBattery();
void displayStartUp(uint16_t volt);
void loop();
void updateManualSpeed();
void showPausePrompt(boolean armReminder);
void pauseTrainer();
void checkStopFlag();
void cleanStartSettings();
void ultimaticSelect(boolean dit, boolean dah);
boolean doPaddleIambic (boolean dit, boolean dah);
boolean checkPaddles();
void updatePaddleLatch(boolean dit, boolean dah);
void clearPaddleLatches ();
void setDITstate();
void setDAHstate();
uint8_t readSensors(int left, int right, boolean init);
void initSensors();
String getRandomWord(uint8_t lengthPref);
String getRandomAbbrev( int maxLength);
String getRandomChars(int maxLength, int option);
static void readPrefix(int index, char* buf, uint8_t* cont, uint8_t* weight);
static bool endsWithDigit(const char* s);
static uint8_t getContinentMask(uint8_t prefValue);
String getRandomCall(int maxLength);
String generateCWword(const String& symbols);
int indexOfConstStr(const char* string, char c);
void generateCW ();
unsigned int wordDoublerICS();
int pitch();
void dispGeneratedChar();
void fetchNewWord();
void displayDecodedMorse(String symbol, boolean keyed);
void displayGeneratedMorse(FONT_ATTRIB style, const String& s);
void SerialOutMorse(const String& s, uint8_t origin);
void displayCWspeed();
void displayPreviewChar();
void updateTopLine();
void repaintModeScreen();
String getKeyerModeSymbol();
void echoTrainerEval();
void updateTimings();
void changeSpeedValue( int t);
void changeSpeed( int t);
void changeVolumeValue( int t);
void changeVolume( int t);
void changePreviewChar( int t);
void keyTransmitter(boolean noTx);
String cleanUpProSigns( const String &input );
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
const char* decoderSetLetters();
uint8_t decoderCharSet();
String utf8ToCodes(const String& s);
String codesToUtf8(const String& s);
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
void toUpperCaseM32(String &s);
String utf8umlaut(const String& s);
void skipWords(uint32_t count);
void armForeignUsbRxDetect();
void serialEvent();
static void bleConsentPrompt();
boolean bleTopMenuService();
void bleSerialEvent();
void serialDecode(String input);
void m32Get(String type, String token, String value);
void m32Put(String type, String token, String value);
boolean setParameter(String token, String value);
void setSsid(int i, String str);
void setPassword(int i, String str);
void setTrxPeer(int i, String str);
void selectWifi(int i);
void stopPlayCw();
#line 100 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void packetReceived() {
  loraReceived = true;
}

#ifdef CONFIG_BLUETOOTH_KEYBOARD
#include "MorseBluetooth.h"
#include "MorseUsbIO.h"
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
# 142 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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
volatile int8_t remoteFilePartSelect = -1;



boolean quickStart;

#ifdef CONFIG_TFT
# 181 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
#define REBOOT_MAGIC_MEMORY 0xC0FFEE42u
RTC_NOINIT_ATTR uint32_t rebootMagic;
RTC_NOINIT_ATTR uint8_t rebootMenuPtr;
bool memoryReboot = false;
#endif




encoderMode encoderState = speedSettingMode;







boolean m32protocol = false;
#ifdef CONFIG_BLE_SERIAL
volatile bool bleProtocol = false;

String bleInputString;
bool bleDiscardLine = false;
#endif
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


#ifndef CONFIG_MCP73871
int8_t hwConf = 1;
#else
int8_t hwConf = 2;
#endif
boolean kochActive = false;



#define DIT_L 0x01
#define DAH_L 0x02
#define DIT_LAST 0x04



unsigned char keyerControl = 0;



boolean DIT_FIRST = false;
boolean DIT_LAST_CLOSED = false;



boolean ultDitMemory = false, ultDahMemory = false;






unsigned int ditLength ;
unsigned int dahLength ;
KEYERSTATES keyerState;
unsigned long charCounter = 25;
uint8_t sensor;
boolean leftKey, rightKey;


unsigned long interWordTimer = 0;
unsigned long acsTimer = 0;


File uploadFile;
bool uploadActive = false;
uint32_t uploadBytesWritten = 0;
# 290 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
const char CWchars[] = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKEB" "\xE4\xF6\xFC" "H" "()"
                       "\xE9\xE8\xE0\xE7\xF1\xE5\xE6\xF8";
# 303 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
int CWwordPos = 0;
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






boolean frameWordForDisplay = false;

uint8_t wordCounter = 0;
uint16_t errCounter = 0;
boolean stopFlag = false;
boolean echoStop = false;




#define STOP_SUMMARY_MS 5000
#define STOP_SUMMARY_IDLE 0
unsigned long stopSummaryUntil = STOP_SUMMARY_IDLE;
String lastWord = "";

unsigned long genTimer;






extern const int KEYOUT_EDGE_MS;
const int KEYOUT_EDGE_MS = 6;

enum MORSE_TYPE {KEY_DOWN, KEY_UP };
unsigned char generatorState;

const char* const continueMsg4Json = "Continue with paddle";
#ifndef CONFIG_TFT
const char* const continueMsg4Disp = "Continue w/ Paddle";
#else
const char* const continueMsg4Disp = "Continue with paddle ";
#endif



#define PAUSE_PROMPT_QUIET_MS 10000
#define PAUSE_PROMPT_IDLE 4294967000UL
# 373 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
unsigned long pausePromptDeadline = PAUSE_PROMPT_IDLE;




byte poolPair[2];

extern const byte pool[][2];
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
               {B11110000, 4},

               {B10110000, 5},
               {B10110100, 6},
               {B00100000, 5},
               {B01001000, 5},
               {B01101000, 5},
               {B10100000, 5},
               {B11011000, 5},
               {B01101000, 5},
               {B01010000, 4},
               {B11100000, 4}
            };
static_assert(sizeof(pool) / sizeof(pool[0]) == sizeof(CWchars) - 1, "CWchars[] and pool[] out of step");





String echoResponse = "";

echoStates echoTrainerState = START_ECHO;
unsigned long echoEvalTimer = 0;
boolean echoEvalCorrect = false;
String echoTrainerPrompt, echoTrainerWord;
uint8_t echoPromptSpeed = 0;




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
# 504 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
  codec_event = true;
}
#endif
# 524 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
uint8_t scrollTop;



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
        delay (10);
        if (diff > 0)
            return 1;
        else
            return -1;
    }
}




void DEBUG (const String& s) {
#ifdef CONFIG_WINKEYER
  if (MorseWinKeyer::ownsSerial())
    return;
#endif
  if (!MorsePreferences::pliste[posSerialOut].value || IGNORE_SERIALOUT)
    Serial.println(s);
}



void setup()
{
#ifdef CONFIG_TFT





   memoryReboot = (rebootMagic == REBOOT_MAGIC_MEMORY);
   uint8_t resumeMenuPtr = memoryReboot ? rebootMenuPtr : 0;
   rebootMagic = 0;
   rebootMenuPtr = 0;
#endif
# 587 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT




  Serial.setTxBufferSize(1024);



  Serial.setRxBufferSize(2048);
#endif
#ifdef CONFIG_USB_HOSTIO
  MorseUsbIO::begin();
#endif
  Serial.begin(115200);
  armForeignUsbRxDetect();
  delay(30);

  inputString.reserve(400);

#ifdef ORIGINAL_M32
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
#endif

#ifdef PIN_PADDLE_LEFT
  leftPin = PIN_PADDLE_LEFT;
#endif
#ifdef PIN_PADDLE_RIGHT
  rightPin = PIN_PADDLE_RIGHT;
#endif

#ifdef PIN_BATTERY
  batteryPin = PIN_BATTERY;
#endif

#ifndef VEXT_ON_VALUE
#define VEXT_ON_VALUE LOW
#endif

#ifndef VEXT_SETTLE_MS
#define VEXT_SETTLE_MS 100
#endif






#ifndef BL_GATE_AT_BOOT
#define BL_GATE_AT_BOOT 1
#endif





#define BL_LEDC_CHANNEL 7
#define BL_LEDC_FREQ 44100
#define BL_LEDC_BITS 9

#ifdef PIN_VEXT
pinMode(PIN_VEXT, OUTPUT);
#endif

  MorsePreferences::readVoltagePref() ;
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


#ifdef PIN_VEXT

digitalWrite(PIN_VEXT, VEXT_ON_VALUE);
delay(VEXT_SETTLE_MS);
#endif


  MorsePreferences::readScreenPref();







#if defined(CONFIG_TFT) && BL_GATE_AT_BOOT
# 717 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
  ledcSetup(BL_LEDC_CHANNEL, BL_LEDC_FREQ, BL_LEDC_BITS);
  ledcAttachPin(TFT_BL, BL_LEDC_CHANNEL);
  MorseOutput::setBrightness(0);
#endif
  MorseOutput::initDisplay();
#ifdef CONFIG_TFT
  MorseOutput::setTheme(MorsePreferences::pliste[posTheme].value);
#endif

#ifdef CONFIG_TFT
# 737 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
  MorseGameMode::warmup();

#ifdef CONFIG_CW_GAME



  MorseRadioCave::warmup();
  MorseMorsel::warmup();
#endif
#endif

#if defined(CONFIG_TFT) && BL_GATE_AT_BOOT



  MorseOutput::clearDisplay();
  MorseOutput::setBrightness(MorsePreferences::oledBrightness);
#else
  MorseOutput::setBrightness(MorsePreferences::oledBrightness);
  MorseOutput::clearDisplay();
#endif
  scrollTop = MorseOutput::getScrollTop();






  MorseMenu::wifiWarmup();
#ifdef CONFIG_TLV320AIC3100_RST
  pinMode(CONFIG_TLV320AIC3100_RST, OUTPUT);
  digitalWrite(CONFIG_TLV320AIC3100_RST, LOW);
  delay(50);
  digitalWrite(CONFIG_TLV320AIC3100_RST, HIGH);
  delay(50);
#endif

  MorseOutput::soundSetup();

#ifdef CONFIG_TLV320AIC3100_INT
  pinMode(CONFIG_TLV320AIC3100_INT, INPUT);
  attachInterrupt(CONFIG_TLV320AIC3100_INT, codec_isr, RISING);
#endif

#ifdef CONFIG_MCP73871
  pinMode(CONFIG_MCP_PG_PIN, INPUT_PULLUP);
  pinMode(CONFIG_MCP_STAT1_PIN, INPUT_PULLUP);
  pinMode(CONFIG_MCP_STAT2_PIN, INPUT_PULLUP);



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
#ifndef CONFIG_TFT
  MorseOutput::printOnStatusLine( true, 0, "Init...pse wait...");
#endif



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
            case 3: MorsePreferences::resetDefaults();
                    ESP.restart();
                    break;
#ifndef LORA_DISABLED
            case 4: MorsePreferences::loraSystemSetup();
                    break;
#endif
#ifdef CONFIG_CN3_PADDLE
            case HWCONF_CN3_SLOT:
                    MorsePreferences::cn3PaddleConfig();
                    break;
#endif
            default: break;
         }
      }
  }

#ifndef CONFIG_I2S_DATA_IN_PIN
  analogSetAttenuation(ADC_0db);
  adcAttachPin(audioInPin);
#endif







  {
    Preferences resetPref;
    resetPref.begin("morserino", false);
    if (resetPref.getUChar("factoryReset", 0)) {
        resetPref.remove("factoryReset");
        resetPref.end();
        MorsePreferences::resetDefaults();
    } else {
        resetPref.end();
    }
  }







  MorsePreferences::captureDefaults();




  MorsePreferences::readPreferences("morserino");
  MorsePreferences::recordResetReason();
  MorsePreferences::readFilePartData();
  koch.setup();

  MorsePreferences::convertLegacySnapshots();

  #ifdef CONFIG_TFT
  MorseOutput::setTheme(MorsePreferences::pliste[posTheme].value);
  #endif
  #ifdef CONFIG_SCROLL_FONT_SIZE
# 902 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
  MorseOutput::applyScrollFontGeometry();
  #endif

  #ifdef CONFIG_SOUND_I2S
  MorseOutput::setSidetoneEnvelope(MorsePreferences::pliste[posSidetoneShape].value);
  #endif

  MorseOutput::setBrightness(MorsePreferences::oledBrightness);


  initSensors();


#ifdef CONFIG_TFT






  if (memoryReboot && resumeMenuPtr != 0) {
    MorsePreferences::menuPtr = resumeMenuPtr;
    MorsePreferences::newMenuPtr = resumeMenuPtr;
    quickStart = true;
  } else {
    quickStart = MorsePreferences::pliste[posQuickStart].value;
  }
#else
  quickStart = MorsePreferences::pliste[posQuickStart].value;
#endif



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

#ifdef CONFIG_PRACTICE_STATS
    MorsePracticeStats::begin();
#endif

#ifdef CONFIG_TFT




    if (!memoryReboot) {
      displayStartUp(volt);
      MorsePreferences::checkNvsSpace();
    }
#else
    displayStartUp(volt);
    MorsePreferences::checkNvsSpace();
#endif

    while (Serial.available())
      Serial.read();

    inputString = "";

#ifdef CONFIG_BLE_SERIAL
    bleInputString.reserve(400);
    if (MorsePreferences::pliste[posBluetoothOut].value == BLT_USE_SERIAL_PROT)
      MorseBleSerial::init();
#endif



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



String shortDate(char const *date) {
    static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int day, year, month = 0;
    char buff[7];
    if (sscanf(date, "%3s %d %d", buff, &day, &year) != 3) return "000000";
    char *p = strstr(month_names, buff);
    if (p) month = (p - month_names) / 3 + 1;
    snprintf(buff, sizeof(buff), "%02d%02d%02d", year % 100, month, day);
    return String (buff);
}



static void splashPause(uint32_t ms) {
#ifdef CONFIG_AUDIO_A11Y
  uint32_t start = millis();
  do {
    MorseVoice::tick();
    delay(5);
  } while (millis() - start < ms);
#else
  delay(ms);
#endif
}

#ifdef CONFIG_AUDIO_A11Y






static void announceSplash(uint16_t v) {
#ifdef CONFIG_TLV320AIC3100



  MorseOutput::soundSetVolume(MorsePreferences::sidetoneVolume);
#endif






  MorseVoice::warnClipStore();
  if (!MorseVoice::clipStoreUsable())



      return;



  MorseVoice::announce("Morserino 32 accessibility edition");
  MorseVoice::announceMore("version");
  MorseVoice::announceMore(String(VERSION_MAJOR));
  MorseVoice::announceMore("point");
  MorseVoice::announceMore(String(VERSION_MINOR));
  if (VERSION_PATCH != 0) {
      MorseVoice::announceMore("point");
      MorseVoice::announceMore(String(VERSION_PATCH));
  }
  if (BETA)
      MorseVoice::announceMore("beta");
  if (v > 1000) {
      int c = (v / 50) * 50;
      int whole = c / 1000;
      MorseVoice::announceMore("battery");
      MorseVoice::announceMore(String(whole));
      MorseVoice::announceMore("point");
      MorseVoice::announceMore(String((c - 1000 * whole) / 100));
      MorseVoice::announceMore("volts");
  }
}


static void announceEmptyBattery() {
  MorseVoice::announce("battery empty");
  splashPause(2000);
}
#else
static void announceSplash(uint16_t v) { (void)v; }
static void announceEmptyBattery() {}
#endif



void displayStartUp(uint16_t volt) {
  String s;
  s.reserve(18);
  s = PROJECTNAME;
  s += " ";
  announceSplash(volt);
  #ifdef CONFIG_TFT
  MorseOutput::dispM32Logo();
  splashPause(1800);
  MorseOutput::clearDisplay();
  #else
  MorseOutput::clearDisplay();
  #endif

#ifndef LORA_DISABLED
  s += String(MorsePreferences::loraQRG / 10000);
#endif
  MorseOutput::printOnStatusLine( true, 0, s);
  s = "V" ;
  s += VERSION_MAJOR;
  s += ".";
  s += VERSION_MINOR;

  if (VERSION_PATCH != 0) {
    s += ".";
    s += VERSION_PATCH;
  }

  if (BETA) {
    s += "b ";
    s += shortDate(COMPILEDATE);
  }





  MorseOutput::printOnScroll(0, REGULAR, 0, s, false, true);
  MorseOutput::printOnScroll(1, REGULAR, 0, COPYRIGHT, false, true);

#ifdef CONFIG_AUDIO_A11Y





  if (const char *clipWarning = MorseVoice::clipStoreDisplayText())
      MorseOutput::printOnScroll(2, BOLD, 0, clipWarning, false, true);
#endif



#ifndef SKIP_BATTERY_PROTECT
#ifdef CONFIG_MCP73871
  uint8_t powerpath_state = (digitalRead(CONFIG_MCP_STAT1_PIN)<<2) + ( digitalRead(CONFIG_MCP_STAT2_PIN) << 1) + digitalRead(CONFIG_MCP_PG_PIN);
  if (powerpath_state == 3) {
    announceEmptyBattery();
    MorseOutput::displayEmptyBattery(shutMeDown);
  }

#else
  if (volt > 1000 && volt < 3320) {
    announceEmptyBattery();
    MorseOutput::displayEmptyBattery(shutMeDown);
  }
  else
#endif
#endif
    MorseOutput::displayBatteryStatus(volt);



#define ST(A) #A
#define STR(A) ST(A)

#ifdef HW_NAME
    brd = STR(HW_NAME);
#else
    brd = "Unknown Device";
#endif
#ifdef ORIGINAL_M32
    if (MorsePreferences::boardVersion == 3)
        brd = "M32 1st edition";
    else if (MorsePreferences::boardVersion == 4)
        brd = "M32 2nd edition";
    else
        brd = "unknown M32 board";
#endif



  vsn = String(VERSION_MAJOR) + "." + String(VERSION_MINOR);
  if (VERSION_PATCH != 0)
      vsn += "." + String(VERSION_PATCH);


splashPause(1800);

#ifdef KIP_SPIKE
  M32KipSpike::run();
#endif
}



void loop() {
   int t;

   m32state = active_loop;

#ifdef CONFIG_AUDIO_A11Y





   MorseVoice::tick();
#endif

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

#ifdef CONFIG_AUDIO_A11Y






   if ((leftKey || rightKey || keyerState != IDLE_STATE) && MorseVoice::isSpeaking())
       MorseVoice::stop();
#endif

   switch (morseState) {
      case morseKeyer: if (doPaddleIambic(leftKey, rightKey)) {
                              return;
                          }
                          if (playCW)
                              generateCW();
                          break;

#ifdef CONFIG_M32KIP
      case kipKeyer:
                          if (doPaddleIambic(leftKey, rightKey)) {
#ifdef CONFIG_AUDIO_A11Y



                              if (MorseVoice::busy()) MorseVoice::stop();
#endif
                              return;
                          }
                          if (playCW) {
#ifdef CONFIG_AUDIO_A11Y
                              if (MorseVoice::busy()) MorseVoice::stop();
#endif
                              generateCW();
                          }
                          else
                              MorseKipKeyer::tick();
                          break;
#endif
      case loraTrx:
      case wifiTrx: if (doPaddleIambic(leftKey, rightKey)) {
                              return;
                          }
                          generateCW();
                          break;

#ifdef CONFIG_WINKEYER
      case morseWinKeyer:
      case morseRemoteWinKeyer:
                          if (MorseWinKeyer::wantsMenu()) {
                              if (protocolActive())
                                  MorseJSON::jsonActivate(ACT_EXIT);
                              MorseMenu::menu_();
                              return;
                          }
                          MorseWinKeyer::paddleActivity(leftKey || rightKey, keyerState);
                          MorseWinKeyer::tick();
                          if (doPaddleIambic(leftKey, rightKey)) {
#if defined(REMOTE_WINKEYER) && defined(CONFIG_AUDIO_A11Y)

                              if (morseState == morseRemoteWinKeyer && MorseVoice::busy()) MorseVoice::stop();
#endif
                              return;
                          }
#ifdef REMOTE_WINKEYER


                          if (morseState == morseRemoteWinKeyer && !MorseWinKeyer::hostSending())
                              MorseKipKeyer::tick();
#endif
                          break;
#endif
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
                                  showPausePrompt(true);
                              }
                          } else {
                              checkStopFlag();
                          }
                          if (genIsActive)
                            generateCW();
                          else if (millis() >= pausePromptDeadline)
                            showPausePrompt(false);
                          break;
      case echoTrainer:
                          checkStopFlag();



                          if (!genIsActive && stopSummaryUntil == STOP_SUMMARY_IDLE && (leftKey || rightKey)) {

                              while (checkPaddles() )
                                  ;
                              genIsActive = !genIsActive;

                              cleanStartSettings();
                          }
                          if (!genIsActive && millis() >= pausePromptDeadline)
                            showPausePrompt(false);
                          if (genIsActive)
                          switch (echoTrainerState) {
                            case START_ECHO:
                            case SEND_WORD:
                            case REPEAT_WORD: echoResponse = ""; generateCW();
                                                break;
                            case EVAL_ANSWER:
                            case EVAL_FEEDBACK:
                            case EVAL_SETTLE: echoTrainerEval();
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
#ifdef CONFIG_BLUETOOTH_KEYBOARD

if (morseState == morseKeyer &&
      MorseBluetooth::keyboardMode() != 0 &&
      !MorseBluetooth::isBLErunning) {

    MorseBluetooth::initializeBluetooth();
}
MorseBluetooth::tick();
#ifdef CONFIG_USB_HOSTIO
MorseUsbIO::tick(morseState == morseKeyer);
#endif
#endif



    serialEvent();
    if (goToMenu) {
        MorseJSON::jsonActivate(ACT_EXIT);
        goToMenu = false;
        MorseMenu::menu_();
        return;
    }
    if (MorseOutput::takeRepaintRequest())
        repaintModeScreen();

    Buttons::modeButton.Update();
    Buttons::volButton.Update();
#ifdef CONFIG_WINKEYER
    if (isWinKeyerMode(morseState) && Buttons::volButton.pin == 0)
        Buttons::volButton.clicks = MorseWinKeyer::filterFn(Buttons::volButton.clicks, Buttons::volButton.depressed);
#endif
    if (Buttons::modeButton.clicks || Buttons::volButton.clicks) {
        MorseOutput::resetTOT();
        if (!genIsActive && (morseState == morseGenerator || morseState == echoTrainer))
            pausePromptDeadline = millis() + PAUSE_PROMPT_QUIET_MS;
    }

    boolean previewCharModeAvailable = (morseState == echoTrainer && generatorMode == KOCH_PREVIEW);







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
                  encoderState = previewCharModeAvailable ? previewCharMode : speedSettingMode;
                  MorsePreferences::writeVolume();
                  #ifdef CONFIG_TLV320AIC3100
                  MorseOutput::soundSetVolume(MorsePreferences::sidetoneVolume);
                  #endif
                  updateTopLine();
                }
                else if (encoderState == previewCharMode) {
                  encoderState = speedSettingMode;
                  updateTopLine();
                }
                else {
                  encoderState = volumeSettingMode;
                  updateTopLine();
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
       case -1: if (protocolActive())
                      MorseJSON::jsonActivate(ACT_EXIT);
                  MorseMenu::menu_();
                  return;
       case 1: if (encoderState == memSelMode) {
                    if (ptr != 0) {
                      preparePlay(memList[ptr]);
                      if (protocolActive())
                        MorseJSON::jsonOK();
                    }
                    encoderState = speedSettingMode;
                    updateTopLine();
                 }
                 else if (morseState == morseGenerator || morseState == echoTrainer) {
                  genIsActive = !genIsActive;
                  if (!genIsActive) {
                        keyOut(false, true, 0, 0);
                        showPausePrompt(true);
                  }
                  else {
                    cleanStartSettings();
                  }

#ifdef CONFIG_WINKEYER
              } else if (isWinKeyerMode(morseState)) {
                    MorseWinKeyer::stopSending();
#endif
              } else if (morseState == morseKeyer || morseState == morseTrx || morseState == kipKeyer) {
                    if (protocolActive())
                        MorseJSON::jsonCreate("message", "Select Memory", "");
                    memoryKeyer();
              }
              break;
       case 2:
#ifdef CONFIG_M32KIP



                if (morseState == kipKeyer)
                    MorsePreferences::setCurrentOptions(
                        MorseKipKeyer::rigCfgAvailable() ? MorsePreferences::kipKeyerRigOptions
                                                         : MorsePreferences::kipKeyerOptions,
                        MorseKipKeyer::rigCfgAvailable() ? MorsePreferences::kipKeyerRigOptionsSize
                                                         : MorsePreferences::kipKeyerOptionsSize);



#ifdef REMOTE_WINKEYER
                if (morseState == morseRemoteWinKeyer)
                    MorsePreferences::setCurrentOptions(
                        MorseKipKeyer::rigCfgAvailable() ? MorsePreferences::remoteWinKeyerRigOptions
                                                         : MorsePreferences::remoteWinKeyerOptions,
                        MorseKipKeyer::rigCfgAvailable() ? MorsePreferences::remoteWinKeyerRigOptionsSize
                                                         : MorsePreferences::remoteWinKeyerOptionsSize);
#endif
                if ((morseState == kipKeyer || morseState == morseRemoteWinKeyer) && MorseKipKeyer::rigCfgAvailable())
                    MorsePreferences::startPreferencesAt(posRigDelay);
#endif
#ifdef CONFIG_WINKEYER
                if (isWinKeyerMode(morseState))
                    MorseWinKeyer::beforePreferences();
#endif
                MorsePreferences::setupPreferences(MorsePreferences::menuPtr);
#ifdef CONFIG_WINKEYER
                if (isWinKeyerMode(morseState))
                    MorseWinKeyer::afterPreferences();
#endif
#ifdef CONFIG_BLUETOOTH_KEYBOARD
                MorseMenu::applyBluetoothUse();
#endif
                MorseOutput::clearDisplay();
                updateTopLine();
                MorseOutput::refreshScrollArea(MorseOutput::relPos);



                if (morseState == morseGenerator || morseState == echoTrainer)
                    pauseTrainer();
                else
                    stopFlag = false;


     default: break;
    }


     if ((t = checkEncoder())) {

        MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
        MorseOutput::resetTOT();
        if (!genIsActive && (morseState == morseGenerator || morseState == echoTrainer))
            pausePromptDeadline = millis() + PAUSE_PROMPT_QUIET_MS;
        switch (encoderState) {
          case speedSettingMode:
#ifdef CONFIG_WINKEYER
                                  if (isWinKeyerMode(morseState)) {
                                    MorseWinKeyer::encoderSpeed(t);
                                    break;
                                  }
#endif
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
           case previewCharMode: changePreviewChar(t);
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







}


void updateManualSpeed() {
  if (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY && speedChanged) {
    speedChanged = false;
    displayCWspeed();
  }
}





void showPausePrompt(boolean armReminder) {
    MorseOutput::printOnStatusLine( true, 0, continueMsg4Disp);
    if (protocolActive())
        MorseJSON::jsonCreate("message", continueMsg4Json, "");
# 1701 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
    pausePromptDeadline = armReminder ? millis() + PAUSE_PROMPT_QUIET_MS : PAUSE_PROMPT_IDLE;
}
# 1711 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void pauseTrainer() {
    lastWord = clearText;
    genIsActive = stopFlag = false;
    stopSummaryUntil = STOP_SUMMARY_IDLE;
    keyOut(false, true, 0, 0);
    wordCounter = 1; errCounter = 0;
    showPausePrompt(true);
}

void checkStopFlag() {
    if (stopFlag) {
      lastWord = clearText;
      genIsActive = stopFlag = false;
      keyOut(false, true, 0, 0);
      if (morseState == echoTrainer) {
        MorseOutput::clearStatusLine();
        MorseOutput::printOnStatusLine( true, 0, String(errCounter) + " errs (" + String(wordCounter-2) + " wds)" );
        wordCounter = 1; errCounter = 0;
# 1737 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
        stopSummaryUntil = millis() + STOP_SUMMARY_MS;
        return;
      }
      wordCounter = 1; errCounter = 0;


      showPausePrompt(true);
    }
    else if (stopSummaryUntil != STOP_SUMMARY_IDLE && millis() >= stopSummaryUntil) {
      stopSummaryUntil = STOP_SUMMARY_IDLE;
      showPausePrompt(true);
    }
}


void cleanStartSettings() {
    if (encoderState == scrollMode) {
        encoderState = speedSettingMode;
        MorseOutput::relPos = MorseOutput::maxPos;
    }
    clearText = "";
    CWword = ""; CWwordPos = 0;
    echoPromptSpeed = 0;
    echoTrainerState = START_ECHO;
    generatorState = KEY_UP;
    keyerState = IDLE_STATE;
    ultDitMemory = ultDahMemory = false;

    interWordTimer = 4294967000;
    pausePromptDeadline = PAUSE_PROMPT_IDLE;

    genTimer = millis()-1;
    errCounter = wordCounter = 0;
    stopSummaryUntil = STOP_SUMMARY_IDLE;


    startFirst = true;
    autoStop = nextword;
    keyOut(false, true, 0, 0);
    updateTopLine();
}
# 1786 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void ultimaticSelect(boolean dit, boolean dah) {
    keyerControl &= ~(DIT_L + DAH_L);
    if (ultDitMemory && ultDahMemory)
        keyerControl |= (DIT_LAST_CLOSED ? DIT_L : DAH_L);
    else if (ultDitMemory)
        keyerControl |= DIT_L;
    else if (ultDahMemory)
        keyerControl |= DAH_L;
    else if (dit && dah)
        keyerControl |= (DIT_LAST_CLOSED ? DIT_L : DAH_L);
    else if (dit)
        keyerControl |= DIT_L;
    else if (dah)
        keyerControl |= DAH_L;

}





boolean doPaddleIambic (boolean dit, boolean dah) {
  boolean paddleSwap;
  static long ktimer;
  static long curtistimer;
  static long latencytimer;
  static long corrTime;
  unsigned int pitch;
#if defined(CONFIG_BLUETOOTH_KEYBOARD) || defined(CONFIG_BLE_SERIAL)
    static unsigned long lastHeapCheck = 0;
    if (millis() - lastHeapCheck > 10000) {
        lastHeapCheck = millis();
        uint32_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 20000) {
            DEBUG("LOW HEAP: " + String(freeHeap) + " bytes free");
        }
    }
#endif
  if (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY) {
    keyDecoder.decode();
    updateManualSpeed();
    return false;
  }
  if (MorsePreferences::pliste[posPolarity].value == 0) {
      paddleSwap = dit; dit = dah; dah = paddleSwap;
  }
# 1852 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
  static const long CHATTER_GUARD_MS = 5;
  static boolean prevDit = false, prevDah = false;
  static long ditOpenedAt = 0, dahOpenedAt = 0;
  if (!dit && prevDit) ditOpenedAt = millis();
  if (!dah && prevDah) dahOpenedAt = millis();
  if (dit && !prevDit) {
      DIT_LAST_CLOSED = true;
      boolean bounce = (millis() - ditOpenedAt) < CHATTER_GUARD_MS;
      if (!(bounce && (keyerControl & DIT_LAST))) ultDitMemory = true;
  }
  if (dah && !prevDah) {
      DIT_LAST_CLOSED = false;
      boolean bounce = (millis() - dahOpenedAt) < CHATTER_GUARD_MS;
      if (!(bounce && !(keyerControl & DIT_LAST))) ultDahMemory = true;
  }
  prevDit = dit; prevDah = dah;


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


           if (echoTrainerState == GET_ANSWER && millis() > genTimer + MorsePreferences::pliste[posEchoThinkTime].value * 1000UL) {
            echoTrainerState = EVAL_ANSWER;
         }
         return false;
        }
        break;

    case DIT:

            if ( MorsePreferences::pliste[posACS].value > 0 && (millis() <= acsTimer))
              break;
            clearPaddleLatches();
            ultDitMemory = false;
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
            ultDahMemory = false;
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
                    if (MorsePreferences::pliste[posCurtisMode].value == ULTIMATIC)
                        ultimaticSelect(dit, dah);
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
                                      case ULTIMATIC: if (DIT_LAST_CLOSED)
                                                               setDITstate();
                                                        else
                                                               setDAHstate();
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

                                   if (charCounter == 12 && !isWinKeyerMode(morseState)) {
                                        MorsePreferences::fireCharSeen(false);
                                   }
                                   if (MorsePreferences::pliste[posACS].value > 0)
                                        acsTimer = millis() + (MorsePreferences::pliste[posACS].value + 1) * ditLength;
                                   if (morseState == morseKeyer || morseState == loraTrx || morseState == wifiTrx || morseState == morseTrx
                                       || morseState == kipKeyer || isWinKeyerMode(morseState))

                                      interWordTimer = millis() + ditLength * (MorsePreferences::pliste[posInterWordSpace].value -1);
                                   else if (morseState == echoTrainer)
                                       interWordTimer = millis() + 2*interCharacterSpace + ditLength + interWordSpace/8;
                                   else
                                       interWordTimer = millis() + interWordSpace;



                                   keyerControl = 0;
                                   ultDitMemory = ultDahMemory = false;
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
#ifdef CONFIG_CN3_PADDLE
  if (MorsePreferences::cn3Mechanical) {



    newL = !digitalRead(LEFT);
    newR = !digitalRead(RIGHT);
  } else
#endif
  {
    sensor = readSensors(LEFT, RIGHT, false);
    newL = (sensor >> 1);
    newR = (sensor & 0x01);
  }

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
#ifdef CONFIG_USB_HOSTIO
  if (morseState == morseKeyer) {
    bool swap = MorsePreferences::pliste[posPolarity].value == 0;
    MorseUsbIO::paddles(swap ? rightKey : leftKey, swap ? leftKey : rightKey);
  }
#endif
  return (leftKey || rightKey);
}






void updatePaddleLatch(boolean dit, boolean dah)
{
    if (dit || dah)
      MorseOutput::resetTOT();
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
# 2185 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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



    if (sizeof(touch_value_t) < 4) {
        if (lValue < 32 || rValue < 32)
          return 3;
        else
          return 0;
          }
    else {
        if (lValue > 40000 || rValue > 40000)
          return 3;
        else
          return 0;
    }
  }
#endif
}


void initSensors() {
#ifdef CONFIG_CN3_PADDLE
  if (MorsePreferences::cn3Mechanical) {



    pinMode(LEFT, INPUT_PULLUP);
    pinMode(RIGHT, INPUT_PULLUP);
    return;
  }
#endif
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


String getRandomWord(uint8_t lengthPref) {
    if (kochActive)
        return koch.getRandomWord();
    uint8_t minLen, maxLen;
    MorsePreferences::wordLengthLimits(lengthPref, minLen, maxLen);
#ifdef CONFIG_ENGLISH_OXFORD
    return getEnglishWord(minLen, maxLen);
#else

    int first = minLen > 1 ? 0 : EnglishWords::WORDS_POINTER[maxLen > 7 ? 0 : maxLen];
    int last = minLen > 1 ? EnglishWords::WORDS_POINTER[minLen - 1] : EnglishWords::WORDS_NUMBER_OF_ELEMENTS;
    return EnglishWords::words[random(first, last)];
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
# 2313 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
String getRandomChars(int maxLength, int option) {
    static char buf[8];
    int s = 0, e = 51;

    if (maxLength > 6) {
        maxLength = random(2, maxLength - 3);
    }




    if (MorsePreferences::usePracticeChars) {
        String result; result.reserve(7);
        const String& pool = MorsePreferences::practiceCharSet;
        for (int i = 0; i < maxLength; ++i)
            result += pool.charAt(random(pool.length()));
        return result;
    }




    if (kochActive) {
        if (option == OPT_KOCH_ADAPTIVE)
            return koch.getAdaptiveChar(maxLength);
        else
            return koch.getRandomChar(maxLength);
    }

    switch (option) {
        case OPT_NUM: case OPT_NUMPUNCT: case OPT_NUMPUNCTPRO: s = 26; break;
        case OPT_PUNCT: case OPT_PUNCTPRO: s = 36; break;
        case OPT_PRO: s = 44; break;
        default: s = 0; break;
    }
    switch (option) {
        case OPT_ALPHA: e = 26; break;
        case OPT_ALNUM: case OPT_NUM: e = 36; break;
        case OPT_ALNUMPUNCT: case OPT_NUMPUNCT: case OPT_PUNCT: e = 45; break;
        default: e = 51; break;
    }

    int len = (maxLength > 6) ? 6 : maxLength;
# 2370 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
    uint8_t boostLevel = MorsePreferences::pliste[posRandomBoost].value;
    if (boostLevel && MorsePreferences::practiceCharSet.length()) {
        static const uint8_t boostMultiplier[] = {3, 8};
        uint8_t mult = boostMultiplier[boostLevel - 1];
        const String& pool = MorsePreferences::practiceCharSet;
        uint8_t weight[51];
        int totalWeight = 0;
        for (int i = s; i < e; ++i) {
            weight[i - s] = (pool.indexOf(CWchars[i]) >= 0) ? mult : 1;
            totalWeight += weight[i - s];
        }
        for (int i = 0; i < len; ++i) {
            int roll = random(totalWeight);
            int j = 0;
            while ((roll -= weight[j]) >= 0)
                ++j;
            buf[i] = CWchars[s + j];
        }
        buf[len] = '\0';
        return String(buf);
    }

    for (int i = 0; i < len; ++i)
        buf[i] = CWchars[random(s, e)];
    buf[len] = '\0';

    return String(buf);
}






static void readPrefix(int index, char* buf, uint8_t* cont, uint8_t* weight) {
    memcpy_P(buf, prefixTable[index].prefix, 5);
    *cont = pgm_read_byte(&prefixTable[index].continent);
    *weight = pgm_read_byte(&prefixTable[index].weight);
}


static bool endsWithDigit(const char* s) {
    char last = 0;
    while (*s) { last = *s; s++; }
    return (last >= '0' && last <= '9');
}


static uint8_t getContinentMask(uint8_t prefValue) {
    static const uint8_t masks[] = {
        CONT_ALL, CONT_EU, CONT_NA, CONT_SA, CONT_AF, CONT_AS, CONT_OC
    };
    return (prefValue <= 6) ? masks[prefValue] : CONT_ALL;
}






uint8_t lastGeneratedCallContinent = CONT_ALL;
uint8_t lastGeneratedCallCqZone = 0;

String getRandomCall(int maxLength) {
    static char call[16];
    int pos = 0;
    uint8_t contMask;
    uint8_t contPref = MorsePreferences::pliste[posCallContinent].value;






    bool vkzlOnly = (contPref == 7) && (maxLength != 1);

    if (maxLength != 1)
        contMask = vkzlOnly ? CONT_OC : getContinentMask(contPref);
    else
        contMask = CONT_ALL;
    bool commonOnly = (MorsePreferences::pliste[posCallCommon].value == 1);


    uint8_t minWeight = commonOnly ? 81 : 0;


    if (vkzlOnly) {


        bool isVK = (random(0, 100) < 85);
        int area;
        if (isVK) {

            int r = random(0, 100);
            if (r < 1) area = 0;
            else if (r < 11) area = 1;
            else if (r < 31) area = 2;
            else if (r < 51) area = 3;
            else if (r < 66) area = 4;
            else if (r < 76) area = 5;
            else if (r < 81) area = 6;
            else if (r < 96) area = 7;
            else if (r < 99) area = 8;
            else area = 9;
            call[pos++] = 'v';
            call[pos++] = 'k';
        } else {

            int r = random(0, 100);
            if (r < 35) area = 1;
            else if (r < 70) area = 2;
            else if (r < 90) area = 3;
            else area = 4;
            call[pos++] = 'z';
            call[pos++] = 'l';
        }
        call[pos++] = '0' + area;

        int suffixLen = (random(0, 10) < 3) ? 2 : 3;
        for (int i = 0; i < suffixLen; i++)
            call[pos++] = 'a' + random(0, 26);




        call[pos] = '\0';
        lastGeneratedCallContinent = CONT_OC;
        lastGeneratedCallCqZone = isVK ? 29 : 32;
        return String(call);
    }




    uint32_t totalWeight = 0;
    char pfxBuf[5];
    uint8_t cont, weight;
    int maxPfxLen = (maxLength > 0) ? maxLength : 99;

    for (int i = 0; i < PREFIX_COUNT; i++) {
        readPrefix(i, pfxBuf, &cont, &weight);
        if (!(cont & contMask)) continue;
        if (weight < minWeight) continue;
        if ((int)strlen(pfxBuf) > maxPfxLen) continue;
        totalWeight += weight;
    }


    if (totalWeight == 0) {
        lastGeneratedCallContinent = CONT_ALL;
        lastGeneratedCallCqZone = 0;

        call[0] = 'a' + random(0, 26);
        call[1] = '0' + random(0, 10);
        call[2] = 'a' + random(0, 26);
        call[3] = 'a' + random(0, 26);
        call[4] = 'a' + random(0, 26);
        call[5] = '\0';
        return String(call);
    }


    uint32_t pick = random(0, totalWeight);
    uint32_t cumulative = 0;
    int chosen = 0;

    for (int i = 0; i < PREFIX_COUNT; i++) {
        readPrefix(i, pfxBuf, &cont, &weight);
        if (!(cont & contMask)) continue;
        if (weight < minWeight) continue;
        if ((int)strlen(pfxBuf) > maxPfxLen) continue;
        cumulative += weight;
        if (cumulative > pick) {
            chosen = i;
            break;
        }
    }


    readPrefix(chosen, pfxBuf, &cont, &weight);
    lastGeneratedCallContinent = cont;
    lastGeneratedCallCqZone = pgm_read_byte(&prefixTable[chosen].cqZone);




    for (int i = 0; pfxBuf[i]; i++) {
        char c = pfxBuf[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        call[pos++] = c;
    }







    int pfxLen = strlen(pfxBuf);
    if (!endsWithDigit(pfxBuf) || pfxLen <= 2) {
        call[pos++] = '0' + random(0, 10);
    }


    int maxTotal;
    if (maxLength == 0) maxTotal = 10;
    else if (maxLength > 4) maxTotal = 6;
    else maxTotal = maxLength + 2;


    int suffixSpace = maxTotal - pos;
    if (suffixSpace < 1) suffixSpace = 1;
    if (suffixSpace > 3) suffixSpace = 3;

    int suffLen;
    if (maxLength == 0) {

        suffLen = random(1, 4);
        if (suffLen == 1 && random(0, 3) != 0)
            suffLen = random(2, 4);
    } else {
        suffLen = suffixSpace;
    }


    while (suffLen-- > 0 && pos < 12) {
        call[pos++] = 'a' + random(0, 26);
    }


    if (maxLength == 0 && random(0, 8) == 0) {
        call[pos++] = '/';
        call[pos++] = (random(0, 2) ? 'm' : 'p');
    }

    call[pos] = '\0';
    return String(call);
}
# 2616 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
String generateCWword(const String& symbols) {
    static char buf[160];
    int pos = 0;

    int l = symbols.length();

    for (int i = 0; i < l; ++i) {
        char c = symbols.charAt(i);

        int pointer = indexOfConstStr(CWchars, c);
        if (pointer < 0) continue;

        byte NoE = pool[pointer][1];
        byte bitMask = pool[pointer][0];

        for (int j = 0; j < NoE; ++j) {
            if (pos >= (int)sizeof(buf) - 2) goto done;
            buf[pos++] = (bitMask & B10000000) ? '2' : '1';
            bitMask <<= 1;
        }
        buf[pos++] = '0';
    }
    if (pos > 0) --pos;
done:
    buf[pos] = '\0';

    return String(buf);
}
# 2654 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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

#ifdef CONFIG_AUDIO_A11Y







  if (MorseVoice::isSpeaking() && generatorState == KEY_UP)
      return;
#endif

  switch (generatorState) {
    case KEY_UP:
            if (millis() < genTimer)
                return;

            if (startFirst == true) {
                CWword = ""; CWwordPos = 0;
            }
            l = CWword.length() - CWwordPos;
            if (l<=0) {
                if (clearText.length() > 0) {


                if (MorsePreferences::pliste[posGeneratorDisplay].value == DISPLAY_BY_WORD &&
                    (morseState == loraTrx || morseState == wifiTrx || morseState == morseGenerator || playCW == true))
                  {
                      displayGeneratedMorse(
                          (morseState == morseGenerator && !frameWordForDisplay) ? REGULAR : BOLD,
                          cleanUpProSigns(clearText));

                  }
                }
                fetchNewWord();
                if (CWword.length() - CWwordPos <= 0)
                  return;
                if ((morseState == echoTrainer)) {
                  displayGeneratedMorse(REGULAR, "\n");
                }
            }
            c = CWword[CWwordPos++];
            if (c == '0' || CWwordPos >= (int)CWword.length()) {
                   if (c == '0') {
                      c = CWword[CWwordPos++];
                      if (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value >= 1)
                          cwForTx(0);
                      }
            }




            if (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value == DISP_ONLY)
                genTimer = millis() + 2;
            else if (morseState != loraTrx && morseState != wifiTrx)
                genTimer = millis() + (c == '1' ? ditLength : dahLength);
            else
                genTimer = millis() + (c == '1' ? rxDitLength : rxDahLength);
            if (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value >= 1)
                c == '1' ? cwForTx(1) : cwForTx(2) ;

            if (generatorMode == KOCH_LEARN || generatorMode == KOCH_PREVIEW)
                displayGeneratedMorse(BOLD, c == '1' ? "." : "-");
            silentEcho = (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value == DISP_ONLY);

            if (silentEcho || stopFlag)
              ;
            else {
#ifdef CONFIG_USB_HOSTIO
                if (morseState == morseKeyer && playCW)
                    MorseUsbIO::element(c != '1', c == '1' ? ditLength : dahLength);
#endif
                keyOut(true, (morseState != loraTrx && morseState != wifiTrx), pitch(), MorsePreferences::sidetoneVolume);
            }
            generatorState = KEY_DOWN;

            break;
    case KEY_DOWN:
#ifdef CONFIG_USB_HOSTIO
            MorseUsbIO::service();
#endif
            if (millis() < genTimer)
                return;


           keyOut(false, (morseState != loraTrx && morseState != wifiTrx), 0, 0);
            if (CWwordPos >= (int)CWword.length()) {
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


                                                uint8_t speedMaxIdx = MorsePreferences::pliste[posEchoSpeedMax].value;
                                                uint8_t speedMax = speedMaxIdx * 5;
                                                if (speedMax > 0 && MorsePreferences::wpm > speedMax) {
                                                    echoPromptSpeed = MorsePreferences::wpm;
                                                    MorsePreferences::wpm = speedMax;
                                                    updateTimings();
                                                    displayCWspeed();
                                                }
                                          }
                        default: break;
                    }
                }
                else {
                      genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxInterWordSpace : interWordSpace - KEYOUT_EDGE_MS) ;
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
             else if ((c = CWword[CWwordPos]) == '0') {


                dispGeneratedChar();
                if (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value == DISP_ONLY)
                    genTimer = millis() +1;
                else
                    genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxInterCharacterSpace : wordDoublerICS() - KEYOUT_EDGE_MS);
             }
             else {
                genTimer = millis() + ((morseState == loraTrx || morseState == wifiTrx) ? rxDitLength : ditLength - KEYOUT_EDGE_MS);
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


    if (alternatePitch && morseState == morseGenerator && MorsePreferences::pliste[posEchoToneShift].value != 0)
      p = (MorsePreferences::pliste[posEchoToneShift].value == 1 ? p * 18 / 17 : p * 17 / 18);
    return p;
}
# 2854 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
static boolean newWordForDisplay = false;

void dispGeneratedChar() {
    static char charBuf[6];
    int charBufLen = 0;
    boolean wordStart = newWordForDisplay;
    newWordForDisplay = false;

    if (generatorMode == KOCH_LEARN || generatorMode == KOCH_PREVIEW ||
            (MorsePreferences::pliste[posGeneratorDisplay].value == DISPLAY_BY_CHAR &&
            (morseState == loraTrx || morseState == wifiTrx || morseState == morseGenerator || playCW)) ||
            (morseState == echoTrainer && MorsePreferences::pliste[posEchoDisplay].value != CODE_ONLY))
    {
# 2881 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
        if (wordStart && (morseState == morseGenerator || morseState == loraTrx ||
                          morseState == wifiTrx || playCW)) {
            int nextSpace = clearText.indexOf(' ');
            uint16_t wordLen = (uint16_t) ((nextSpace == -1) ? clearText.length() : nextSpace);
            if (MorseOutput::wordNeedsWrap(wordLen)) {
                MorseOutput::printToScroll(REGULAR, "\n", true, encoderState == scrollMode);
            }
        }

        if (clearText.charAt(0) == (char)0xC3) {
            charBuf[0] = clearText.charAt(0);
            charBuf[1] = clearText.charAt(1);
            charBuf[2] = '\0';
            charBufLen = 2;
            clearText.remove(0, 2);
        }
        else {
            charBuf[0] = clearText.charAt(0);
            charBuf[1] = '\0';
            charBufLen = 1;
            clearText.remove(0, 1);
        }

        if (generatorMode == KOCH_LEARN || generatorMode == KOCH_PREVIEW) {
            displayGeneratedMorse(REGULAR, " ");
        }
        displayGeneratedMorse(
            ((frameWordForDisplay && morseState == morseGenerator) ||
             morseState == loraTrx || morseState == wifiTrx || generatorMode == KOCH_LEARN || generatorMode == KOCH_PREVIEW)
                ? BOLD : REGULAR,
            cleanUpProSigns(charBuf));
        if (generatorMode == KOCH_LEARN || generatorMode == KOCH_PREVIEW) {
            displayGeneratedMorse(REGULAR, " ");
        }
    }

    ++charCounter;
    if (charCounter == 12) {
        MorsePreferences::fireCharSeen(true);
    }
}






void fetchNewWord() {
  int rssi, rxWpm, rv;
  char numBuffer[16];

  frameWordForDisplay = false;


    if (morseState == loraTrx || morseState == wifiTrx) {
       MorseOutput::updateSMeter(0);
       startFirst = false;

       if (cwBuReady()) {
            uint8_t header = decodePacket(&rssi, &rxWpm, &CWword); CWwordPos = 0;
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
            newWordForDisplay = true;

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
        CWword = generateCWword(clearText); CWwordPos = 0;
        displayGeneratedMorse(REGULAR, " ");
        newWordForDisplay = true;
        return;
    }
    else {
    if ((morseState == morseGenerator) ) {
        displayGeneratedMorse(REGULAR, " ");
    }

    if (generatorMode == KOCH_LEARN || generatorMode == KOCH_PREVIEW) {
        startFirst = false;
        echoTrainerState = SEND_WORD;
    }
    if (startFirst == true) {
        clearText = "vvvA";
        frameWordForDisplay = true;
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
                                    if (generatorMode != KOCH_LEARN && generatorMode != KOCH_PREVIEW) {
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
#ifdef CONFIG_PRACTICE_STATS
                            bool statsDiscardWord = false;
#endif
                            if ((MorsePreferences::pliste[posMaxSequence].value != 0) && (generatorMode != KOCH_LEARN) && (generatorMode != KOCH_PREVIEW))
                              if ( morseState == echoTrainer || ((morseState == morseGenerator) && !MorsePreferences::pliste[posAutoStop].value) ) {

                                ++ wordCounter;
                                int limit = 1 + MorsePreferences::pliste[posMaxSequence].value;
                                if (wordCounter == limit) {
                                  clearText = "+";
                                  frameWordForDisplay = true;
                                  echoStop = true;
                                  if (echoTrainerState == REPEAT_WORD)
                                    echoTrainerState = SEND_WORD;
                                }
                                else if (wordCounter == (limit+1)) {
                                    stopFlag = true;
                                    echoStop = false;
#ifdef CONFIG_PRACTICE_STATS
                                    statsDiscardWord = true;
#endif

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
                                      case KOCH_PREVIEW:clearText = koch.getKochChar(MorsePreferences::previewCharIndex);
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
#ifdef CONFIG_PRACTICE_STATS
# 3105 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
                                    if (!statsDiscardWord)
                                        MorsePracticeStats::wordPresented(clearText);
#endif
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
      CWword = generateCWword(clearText); CWwordPos = 0;
      echoTrainerWord = clearText;
      newWordForDisplay = true;
    }
}





void displayDecodedMorse(String symbol, boolean keyed) {
#ifdef CONFIG_WINKEYER
    if (isWinKeyerMode(morseState))
        MorseWinKeyer::paddleChar(symbol);
#endif
#if defined(CONFIG_CW_GAME) || defined(CONFIG_QSO_BOT)





    if (gameMode) {
        String encoded = symbol;
        encodeProSigns(encoded);
        if (encoded.length() >= 1) {
            gameCharBuffer = encoded.charAt(0);
        }
        return;
    }
#endif
#ifdef CONFIG_QSO_BOT





    if (qsoBotMode) {
        String encoded = symbol;
        encodeProSigns(encoded);
        if (encoded.length() >= 1) {
            gameCharBuffer = encoded.charAt(0);
        }

    }
#endif




    if (symbol == "e" && echoResponse.endsWith("eee")) {
        if (morseState != echoTrainer || !echoTrainerWord.startsWith(echoResponse + "e"))
            symbol = "<err>";
    }
    String tmp_str = symbol;
    if (MorsePreferences::pliste[posOutputCase].value) {
        toUpperCaseM32(tmp_str);
    }





#ifdef CONFIG_TFT
    FONT_ATTRIB cwStyle = keyed ? MORSE_REGULAR : MORSE_BOLD;
#else
    FONT_ATTRIB cwStyle = keyed ? REGULAR : BOLD;
#endif
    MorseOutput::printToScroll(cwStyle, tmp_str, true, encoderState == scrollMode);
    SerialOutMorse(tmp_str, keyed ? 0b001 : 0b010);

#ifdef CONFIG_BLUETOOTH_KEYBOARD
    if ((MorseBluetooth::keyboardMode() & 0x6) >= 0x2)
        MorseBluetooth::bluetoothTypeString(tmp_str);
#endif

    if (morseState == echoTrainer) {



        encodeProSigns(symbol);

        if (symbol == "R")
            echoResponse = "";
        else if (symbol != " ")
            echoResponse.concat(symbol);
    }
}
# 3219 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void displayGeneratedMorse(FONT_ATTRIB style, const String& s) {
#ifdef CONFIG_TFT


    if (style == REGULAR) style = MORSE_REGULAR;
    else if (style == BOLD) style = MORSE_BOLD;
#endif
    if (MorsePreferences::pliste[posOutputCase].value) {
        String upper = s;
        toUpperCaseM32(upper);
        MorseOutput::printToScroll(style, upper, true, encoderState == scrollMode);
        SerialOutMorse(upper, 0b100);
#ifdef CONFIG_BLUETOOTH_KEYBOARD
        if ((MorseBluetooth::keyboardMode() & 0x6) >= 0x2)
            MorseBluetooth::bluetoothTypeString(upper);
#endif
    } else {
        MorseOutput::printToScroll(style, s, true, encoderState == scrollMode);
        SerialOutMorse(s, 0b100);
#ifdef CONFIG_BLUETOOTH_KEYBOARD
        if ((MorseBluetooth::keyboardMode() & 0x6) >= 0x2)
            MorseBluetooth::bluetoothTypeString(s);
#endif
    }
}



void SerialOutMorse(const String& s, uint8_t origin) {
  uint8_t bitmap = (MorsePreferences::pliste[posSerialOut].value < 5 ? MorsePreferences::pliste[posSerialOut].value : 7);
  if (origin & bitmap)
      m32out.echo(s);
}





void displayCWspeed() {
  char numBuf[16];
  uint8_t wpm;





  if (encoderState == previewCharMode) {
      displayPreviewChar();
      return;
  }

  wpm = lastWasKey ? keyDecoder.getWpm() : audioDecoder.getWpm();
  if ((morseState == echoTrainer && MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY))
    sprintf(numBuf, "(%2i)", wpm);
  else if (( morseState == morseGenerator || morseState == echoTrainer ))
    sprintf(numBuf, "(%2i)", effWpm);
  else if (morseState == morseTrx )
    sprintf(numBuf, "r%2is", audioDecoder.getWpm());
#ifdef CONFIG_WINKEYER
  else if (isWinKeyerMode(morseState))
    sprintf(numBuf, MorseWinKeyer::isOpen() ? "Host" : "    ");
#endif
  else sprintf(numBuf, "    ");

  MorseOutput::printOnStatusLine(false, 3, numBuf);

  if (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY &&
      (morseState == morseKeyer || morseState == loraTrx || morseState == wifiTrx || morseState == kipKeyer))
        sprintf(numBuf, "%2i", wpm);
  else
    sprintf(numBuf, "%2i", (morseState == morseDecoder ? wpm : MorsePreferences::wpm));
  MorseOutput::printOnStatusLine(encoderState == speedSettingMode ? true : false, 7, numBuf);
  MorseOutput::printOnStatusLine(false, 10, "WpM");
  MorseOutput::refreshDisplay();
}
# 3316 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void displayPreviewChar() {
  if (encoderState != previewCharMode)
      return;
  char numBuf[16];
  String glyph = cleanUpProSigns(koch.getKochChar(MorsePreferences::previewCharIndex));
  sprintf(numBuf, "%2d:%-4s", MorsePreferences::previewCharIndex + 1, glyph.c_str());
  MorseOutput::printOnStatusLine(true, 3, numBuf);
  MorseOutput::printOnStatusLine(false, 10, "Chr");
  MorseOutput::refreshDisplay();
}
# 3334 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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
  else if (morseState != morseDecoder)
    MorseOutput::printOnStatusLine(false, 2, getKeyerModeSymbol() + " ");
  else
    MorseOutput::printOnStatusLine(false, 2, "d  ");
  displayPreviewChar();

  MorseOutput::printOnStatusLine(false, 3, MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY ? "SK" : "CK");

  displayCWspeed();
  if ((morseState == loraTrx ) || (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value == 2))
    MorseOutput::dispLoraLogo();
  else if ((morseState == wifiTrx) || (morseState == morseGenerator && MorsePreferences::pliste[posLoraCwTransmit].value == 1))
      MorseOutput::dispWifiLogo();
#ifdef CONFIG_M32KIP
  else if ((morseState == kipKeyer || morseState == morseRemoteWinKeyer) && MorseKipKeyer::linked())
      MorseOutput::dispWifiLogo();
#endif
#ifdef CONFIG_BLE_SERIAL
  else if (bleProtocol)
      MorseOutput::dispBleLogo(true);
#endif
#ifdef CONFIG_BLUETOOTH_KEYBOARD
  else if (morseState == morseKeyer && MorseBluetooth::hostConnected())
      MorseOutput::dispBleLogo(true);
#endif




  MorseOutput::displayVolume(encoderState != volumeSettingMode, MorsePreferences::sidetoneVolume);
  MorseOutput::refreshDisplay();
}







void repaintModeScreen() {
  MorseOutput::clearDisplay();
  updateTopLine();
  MorseOutput::refreshScrollArea(MorseOutput::relPos);
  if (encoderState == scrollMode)
    MorseOutput::displayScrollBar(true);
}


String getKeyerModeSymbol() {
  const char symbols[] = " ABUNS";
  return String(symbols[MorsePreferences::pliste[posCurtisMode].value]);
}
# 3424 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void echoTrainerEval() {
    switch (echoTrainerState) {

      case EVAL_ANSWER:
            if (echoPromptSpeed > 0) {
                MorsePreferences::wpm = echoPromptSpeed;
                echoPromptSpeed = 0;
                updateTimings();
                displayCWspeed();
            }
            echoEvalCorrect = (echoResponse == echoTrainerWord);
            echoEvalTimer = millis() + interCharacterSpace / 2;
            echoTrainerState = EVAL_FEEDBACK;
            break;

      case EVAL_FEEDBACK:
            if (millis() < echoEvalTimer)
                break;
            if (echoEvalCorrect) {
                displayGeneratedMorse(OK_RESULT, "OK");
                if (MorsePreferences::pliste[posEchoConf].value)
                    MorseOutput::soundSignalOK();
                if (kochActive) {
                    koch.decreaseWordProbability(echoTrainerWord);
#ifdef CONFIG_PRACTICE_STATS
                    MorsePracticeStats::recordWord(echoTrainerWord, echoResponse);
#endif
                }
            }
            else if ((generatorMode != KOCH_LEARN && generatorMode != KOCH_PREVIEW) || echoResponse != "") {
                ++errCounter;
                displayGeneratedMorse(ERR_RESULT, "ERR");
                if (MorsePreferences::pliste[posEchoConf].value)
                    MorseOutput::soundSignalERR();
                if (kochActive) {
                    koch.increaseWordProbability(echoTrainerWord, echoResponse);
#ifdef CONFIG_PRACTICE_STATS
                    MorsePracticeStats::recordWord(echoTrainerWord, echoResponse);
#endif
                }
            }
            echoEvalTimer = millis() + 16*ditLength + interWordSpace/12;
            echoTrainerState = EVAL_SETTLE;
            break;

      case EVAL_SETTLE:
            if (millis() < echoEvalTimer)
                break;
            if (MorsePreferences::pliste[posSpeedAdapt].value)
                changeSpeed(echoEvalCorrect ? 1 : -1);
            echoResponse = "";
            clearPaddleLatches();
            echoTrainerState = echoEvalCorrect ? SEND_WORD : REPEAT_WORD;
            break;

      default:
            break;
    }
}


void updateTimings() {
  ditLength = 1200 / MorsePreferences::wpm;
  dahLength = 3 * ditLength;
  interCharacterSpace = MorsePreferences::pliste[posInterCharSpace].value * ditLength;
  halfICS = (3 + MorsePreferences::pliste[posInterCharSpace].value) * ditLength / 2;
  interWordSpace = _max(MorsePreferences::pliste[posInterWordSpace].value, MorsePreferences::pliste[posInterCharSpace].value+4) * ditLength;
  effWpm = 60000 / (31 * ditLength + 4 * interCharacterSpace + interWordSpace );
}
# 3504 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void changeSpeedValue( int t) {
  MorsePreferences::wpm += t;
  MorsePreferences::wpm = constrain(MorsePreferences::wpm, MorsePreferences::wpmMin, MorsePreferences::wpmMax);
  updateTimings();
  if (protocolActive())
      MorseJSON::jsonControl("speed", MorsePreferences::wpm, MorsePreferences::wpmMin, MorsePreferences::wpmMax, false);
}

void changeSpeed( int t) {
  changeSpeedValue(t);
  charCounter = 0;
  if (m32state != menu_loop) {
      if (MorseOutput::statusLineHasFullLineMessage())
          updateTopLine();
      else
          displayCWspeed();
  }
#ifdef CONFIG_AUDIO_A11Y






  MorseVoice::announceOverriding(String(MorsePreferences::wpm));
  MorseVoice::announceMore("words per minute");
#endif
}


void changeVolumeValue( int t) {
    MorsePreferences::sidetoneVolume += t+1;
    MorsePreferences::sidetoneVolume = constrain(MorsePreferences::sidetoneVolume, 1, 20) -1;

    #ifdef CONFIG_TLV320AIC3100
      MorseOutput::soundSetVolume(MorsePreferences::sidetoneVolume);
    #endif
    if (protocolActive())
      MorseJSON::jsonControl("volume", MorsePreferences::sidetoneVolume, MorsePreferences::volumeMin, MorsePreferences::volumeMax, false);
}

void changeVolume( int t) {
    changeVolumeValue(t);
    if (m32state != menu_loop) {
        if (MorseOutput::statusLineHasFullLineMessage())
            updateTopLine();
        else
            MorseOutput::displayVolume((encoderState == volumeSettingMode ? false : true), MorsePreferences::sidetoneVolume);
    }
}
# 3563 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
#define PREVIEW_CHAR_SETTLE_MS 200

void changePreviewChar( int t) {
    uint8_t count = MorsePreferences::kochMaximum;
    int16_t idx = (int16_t) MorsePreferences::previewCharIndex + t;
    if (idx < 0) idx = 0;
    if (idx >= count) idx = count - 1;
    MorsePreferences::previewCharIndex = (uint8_t) idx;

    keyOut(false, true, 0, 0);
    keyOut(false, false, 0, 0);
    clearText = "";
    CWword = "";
    CWwordPos = 0;
    generatorState = KEY_UP;
    echoTrainerState = SEND_WORD;



    genTimer = millis() + PREVIEW_CHAR_SETTLE_MS;




    if (m32state != menu_loop)
        updateTopLine();
}

void keyTransmitter(boolean noTx) {
#ifdef CONFIG_USB_HOSTIO


   if (morseState == morseKeyer)
      MorseUsbIO::keyed(true, playCW);
#endif
  if (noTx )
      return;
   digitalWrite(keyerPin, HIGH);
#ifdef CONFIG_BLUETOOTH_KEYBOARD
   if ((MorseBluetooth::keyboardMode() & 0x1) == 0x1)
      MorseBluetooth::bluetoothTypeLCTRL(true);
#endif
}


String cleanUpProSigns( const String &input ) {




    static const struct { char code; const char* expansion; uint8_t expLen; } table[] = {
        { 'S', "<as>", 4 },
        { 'A', "<ka>", 4 },
        { 'N', "<kn>", 4 },
        { 'K', "<sk>", 4 },
        { 'E', "<ve>", 4 },
        { 'B', "<bk>", 4 },
        { 'H', "ch", 2 },
        { 'R', "<err>", 5 },
        { 'U', "*", 1 },
    };
    static const int tableSize = sizeof(table) / sizeof(table[0]);



    static char buf[128];
    int out = 0;
    int len = input.length();
    const char* src = input.c_str();

    for (int i = 0; i < len && out < (int)sizeof(buf) - 6; ++i) {
        bool found = false;
        for (int t = 0; t < tableSize; ++t) {
            if (src[i] == table[t].code) {
                memcpy(&buf[out], table[t].expansion, table[t].expLen);
                out += table[t].expLen;
                found = true;
                break;
            }
        }
        if (!found && (uint8_t) src[i] >= 0xC0) {
            buf[out++] = (char) 0xC3;
            buf[out++] = (char) ((uint8_t) src[i] - 0x40);
            found = true;
        }
        if (!found) {
            buf[out++] = src[i];
        }
    }
    buf[out] = '\0';

    return String(buf);
}
# 3669 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
int16_t batteryVoltage() {
#ifndef CONFIG_MCP73871
#ifdef PIN_ADC_CTRL
  #define ADC_Ctrl PIN_ADC_CTRL
  pinMode(ADC_Ctrl,OUTPUT);
#endif
#ifdef PIN_VEXT


      if (MorsePreferences::boardVersion == 3)
         digitalWrite(PIN_VEXT,LOW);

      else if (MorsePreferences::boardVersion == 4)
         digitalWrite(PIN_VEXT, HIGH);
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


#else

#ifdef CONFIG_BATMEAS_PIN
# 3747 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
  fadcInit(1, CONFIG_BATMEAS_PIN);
  int16_t mvolt = 0;
  for (int i = 0; i < 4; ++i) {
      mvolt += analogReadMilliVoltsFast(CONFIG_BATMEAS_PIN-1);

    }


  return ((int) (mvolt * 3.275) / 4);
#else
  return 0;
#endif

#endif

}



double ReadVoltage(byte pin){
  adcAttachPin(pin);
  analogSetClockDiv(128);
  analogSetPinAttenuation(pin,ADC_11db);
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

  if (MorsePreferences::pliste[posTimeOut].value || enforce) {
      timeOut = 300000UL * MorsePreferences::pliste[posTimeOut].value;
    if (protocolActive() && !enforce)
      return;
    if ((millis() - MorseOutput::TOTcounter) > timeOut || enforce == true ) {
          MorseOutput::clearDisplay();
          MorseOutput::printOnScroll(1, INVERSE_BOLD, 0, "Power OFF...");
          MorseOutput::printOnScroll(2, REGULAR, 0, "FN to turn ON");
          if (protocolActive())
                  MorseJSON::jsonCreate("message", "Power off", "");
          MorseOutput::refreshDisplay();
          delay (1500);
          shutMeDown();
      }
  }
}

void shutMeDown() {
  MorsePreferences::writeVolume();
#ifdef CONFIG_PRACTICE_STATS
  MorsePracticeStats::endSegment();
#endif
  MorseOutput::sleep();
  if (protocolActive())
    MorseJSON::jsonError("M32 SLEEP SHUTDOWN BY USER");
#ifdef CONFIG_BLE_SERIAL
  MorseBleSerial::txFlush(300);
#endif

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
#ifdef LORA_RADIOLIB
  radio.sleep();
#endif

WiFi.disconnect(true, false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  MorseOutput::soundSuspend();
#ifdef PIN_VEXT
  digitalWrite(PIN_VEXT, ! VEXT_ON_VALUE);
  delay(100);
#endif





  esp_deep_sleep_start();


}
# 3859 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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
# 3929 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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

    #ifdef CONFIG_CW_GAME
    if (MorseMorsel::morselNetMode && broadcast && len >= 5 &&
        data[0] == 'M' && data[1] == 'S' && data[2] == 'L') {
        MorseMorsel::mslNetOnRecv(mac, data, len);
        return;
    }
    #endif



    #ifdef CONFIG_CW_GAME
    if (MorseGridNet::gridNetMode && broadcast && len >= 5 &&
        data[0] == 'G' && data[1] == 'R' && data[2] == 'D') {
        MorseGridNet::onRecv(mac, data, len);
        return;
    }
    #endif





    #ifdef CONFIG_CW_GAME
    if (pileupMode && broadcast && len >= FTP_MAGIC_LEN &&
        memcmp(data, FTP_MAGIC, FTP_MAGIC_LEN) == 0) {
        MorsePileup::ftpNetOnRecv(mac, data, len);
        return;
    }
    #endif


    #ifdef CONFIG_CW_GAME
    if (pileupMode && len > 2 && broadcast) {

        String elements = "";
        for (int i = 2; i < len; i++) {
            uint8_t c = data[i];
            int startBit = (i == 2) ? 2 : 0;
            for (int j = startBit; j < 4; j++) {
                uint8_t elem = (c >> (2 * (3 - j))) & 0x03;
                if (elem == 3) goto pileup_eow;
                elements += (char)(elem + '0');
            }
        }
        pileup_eow:
        pileupRxText = CWwordToClearText(elements);
        pileupRxReady = true;
        return;
    }
    #endif
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
          case '0': symbol = decodedSymbol(ptr);
                    ptr = 0;
                    result += symbol;
                    break;
      }
  }
  symbol = decodedSymbol(ptr);
  result += symbol;
  return encodeProSigns(result);
}
# 4107 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
const char* decoderSetLetters() {
#ifdef CONFIG_AUDIO_A11Y
  uint8_t set = DEC_CHARS_STANDARD;
#else
  uint8_t set = MorsePreferences::pliste[posDecoderChars].value;
#endif
  switch (set) {
    case DEC_CHARS_ITU: return "\xE4\xF6\xFC" "H" "()" "\xE9";
    case DEC_CHARS_FR_ES_PT: return "\xE4\xF6\xFC" "H" "\xE0\xE9\xE8\xE7\xF1";
    case DEC_CHARS_SV_FI: return "\xE5\xE4\xF6\xFC" "H";
    case DEC_CHARS_DA_NO: return "\xE5\xE6\xF8\xFC" "H";
    default: return "\xE4\xF6\xFC" "H";
  }
}

uint8_t decoderCharSet() {
#ifdef CONFIG_AUDIO_A11Y
  return DEC_CHARS_STANDARD;
#else
  if (gameMode)
    return DEC_CHARS_STANDARD;
#ifdef CONFIG_QSO_BOT
  if (qsoBotMode)
    return DEC_CHARS_STANDARD;
#endif
  switch (morseState) {
    case morseKeyer: case loraTrx: case wifiTrx: case morseTrx: case morseDecoder: case echoTrainer:
    case kipKeyer:
    case kipRig:
      return MorsePreferences::pliste[posDecoderChars].value;
    default:
      return DEC_CHARS_STANDARD;
  }
#endif
}
# 4150 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
static const char* foldLatin1(uint8_t b) {
    uint8_t l = b + 0x40;
    if (l >= 0xC0 && l <= 0xDE && l != 0xD7) l += 0x20;
    switch (l) {
        case 0xE4: return "\xE4"; case 0xF6: return "\xF6"; case 0xFC: return "\xFC";
        case 0xE9: return "\xE9"; case 0xE8: return "\xE8"; case 0xE0: return "\xE0";
        case 0xE7: return "\xE7"; case 0xF1: return "\xF1";
        case 0xE5: return "\xE5"; case 0xE6: return "\xE6"; case 0xF8: return "\xF8";
        case 0xE1: case 0xE2: case 0xE3: return "a";
        case 0xEA: case 0xEB: return "e";
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return "i";
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: return "o";
        case 0xF9: case 0xFA: case 0xFB: return "u";
        case 0xFD: case 0xFF: return "y";
        case 0xDF: return "ss";
        default: return "";
    }
}



String utf8ToCodes(const String& s) {
    String out;
    out.reserve(s.length());
    for (unsigned int i = 0; i < s.length(); ++i) {
        uint8_t c = (uint8_t) s.charAt(i);
        if (c == 0xC3 && i + 1 < s.length())
            out += foldLatin1((uint8_t) s.charAt(++i));
        else if (c < 0x80)
            out += (char) c;
    }
    return out;
}




String codesToUtf8(const String& s) {
    String out;
    out.reserve(s.length() + 4);
    for (unsigned int i = 0; i < s.length(); ++i) {
        uint8_t c = (uint8_t) s.charAt(i);
        if (c >= 0xC0) { out += (char) 0xC3; out += (char) (c - 0x40); }
        else out += (char) c;
    }
    return out;
}

String encodeProSigns( String &input ) {



    static const struct { const char* seq; uint8_t seqLen; char code; } table[] = {
        { "<as>", 4, 'S' },
        { "<ka>", 4, 'A' },
        { "<kn>", 4, 'N' },
        { "<sk>", 4, 'K' },
        { "<ve>", 4, 'E' },
        { "<bk>", 4, 'B' },
        { "<ch>", 4, 'H' },
        { "<err>", 5, 'R' },
        { "*", 1, 'U' },
    };
    static const int tableSize = sizeof(table) / sizeof(table[0]);

    static char buf[64];
    int out = 0;
    int len = input.length();
    const char* src = input.c_str();

    for (int i = 0; i < len && out < (int)sizeof(buf) - 2; ) {
        bool found = false;
        for (int t = 0; t < tableSize; ++t) {
            if (i + table[t].seqLen <= len &&
                memcmp(&src[i], table[t].seq, table[t].seqLen) == 0) {
                buf[out++] = table[t].code;
                i += table[t].seqLen;
                found = true;
                break;
            }
        }
        if (!found && (uint8_t) src[i] == 0xC3 && i + 1 < len) {
            for (const char* f = foldLatin1((uint8_t) src[i + 1]); *f && out < (int)sizeof(buf) - 2; ++f)
                buf[out++] = *f;
            i += 2;
            found = true;
        }
        if (!found) {
            buf[out++] = src[i++];
        }
    }
    buf[out] = '\0';

    input = buf;
    return input;
}
# 4277 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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
# 4336 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
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
  boolean sidetone = true;
#ifdef CONFIG_WINKEYER


  if (morseState == morseWinKeyer) {
      noTx = !(fromHere && MorsePreferences::pliste[posKeyExternalTx].value != 0 && MorseWinKeyer::txWanted());
      sidetone = MorseWinKeyer::sidetoneWanted();
  }
#endif
#ifdef REMOTE_WINKEYER



  boolean kipEdge = true;
  if (morseState == morseRemoteWinKeyer) {
      sidetone = MorseWinKeyer::sidetoneWanted();
      kipEdge = fromHere && MorseWinKeyer::txWanted();
  }
#endif


  if (on) {
      if (fromHere) {
        intPitch = f;
        intTone = true;
        if (sidetone)
          MorseOutput::pwmTone(intPitch, volume, true);
        else
          delay(KEYOUT_EDGE_MS);
      } else {
        extTone = true;
        extPitch = f;
        if (!intTone)
          MorseOutput::pwmTone(extPitch, volume, MorsePreferences::pliste[posExtAudioOnDecode].value);
        }
#ifdef CONFIG_M32KIP
#ifdef REMOTE_WINKEYER
      if (kipEdge)
#endif
      MorseKipKeyer::noteEdge(true);
#endif
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
#ifdef CONFIG_M32KIP
        MorseKipKeyer::noteEdge(false);
#endif
        digitalWrite(keyerPin, LOW);
#ifdef CONFIG_BLUETOOTH_KEYBOARD
        if ((MorseBluetooth::keyboardMode() & 0x1) == 0x1)
          MorseBluetooth::bluetoothTypeLCTRL(false);
#endif
#ifdef CONFIG_USB_HOSTIO
        MorseUsbIO::keyed(false);
#endif
  }
}



void audioLevelAdjust() {
    uint16_t i, maxi, mini;
    uint16_t dataSize = 1216;
    uint16_t testData[dataSize];

     MorseOutput::clearDisplay();

  #ifdef CONFIG_SOUND_I2S
    MorseOutput::printOnScroll(0, BOLD, 0, "Key ON");
    MorseOutput::printOnScroll(1, REGULAR, 0, "End with FN");

    keyOut(true, true, 698, MorsePreferences::sidetoneVolume);
    while (true) {
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks)
            break;
    }
    keyOut(false, true, 698, 0);
  #else

    MorseOutput::printOnScroll(0, BOLD, 0, "Audio In Adj.");
    MorseOutput::printOnScroll(1, REGULAR, 0, "End with FN");

    keyOut(true, true, 698, 0);
    boolean fnPressed = false;
    while (true) {
        Buttons::volButton.Update();
        if (fnPressed || Buttons::volButton.clicks)
            break;



        for (i = 0; i < dataSize ; ++i) {
            testData[i] = analogRead(audioInPin);
            if ((i & 63) == 0) {
                Buttons::volButton.Update();
                if (Buttons::volButton.clicks)
                    fnPressed = true;
            }
        }
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

  #endif
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
      if (protocolActive())
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
    if (protocolActive()) MorseJSON::jsonCreate("message", "Click to Exit", "");
  }
  else {
    String Number = (memNo < 3 ? "R" : "_") + String(memNo) + ": " ;
    String Value = Number + String(MorsePreferences::cwMem[memNo-1]);
    Value = Value.substring(0,18);
    MorseOutput::printOnStatusLine(false, 0, Value);
    if (protocolActive()) MorseJSON::jsonCreate("message", "Memory " + Value, "");
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
# 4623 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
static boolean wordReaderAtEof = false;

String getWord() {
    String result = "";
    result.reserve(250);
    byte c;

    if (wordReaderAtEof) {
        wordReaderAtEof = false;
        return result;
    }


    uint32_t partStart = 0;
    uint32_t partEnd = 0;

    if (MorsePreferences::filePartCount >= 2) {
        uint8_t p = MorsePreferences::filePartSelected;
        partStart = MorsePreferences::fileParts[p].startOffset;
        partEnd = MorsePreferences::fileParts[p].endOffset;
    }

    while (file.available()) {

        if (partEnd > 0 && file.position() >= partEnd) {
            break;
        }

        c = file.read();
        if (!isSpace(c))
            result += (char) c;
        else if (result.length() > 0) {
            ++MorsePreferences::fileWordPointer;
            result = cleanUpText(result);
            if (result.indexOf('C') != -1) {

                result = "";
                while (file.available()) {
                    char c = file.read();
                    if (c == '\n') break;
                }
            }
            else return result;
        }
    }


    wordReaderAtEof = true;

    if (MorsePreferences::filePartCount >= 2) {

        uint8_t p = MorsePreferences::filePartSelected;
        file.seek(MorsePreferences::fileParts[p].startOffset);
        MorsePreferences::fileParts[p].wordPointer = 0;
        MorsePreferences::fileWordPointer = 0;
    } else {

        file.close();
        file = SPIFFS.open("/player.txt");
        MorsePreferences::fileWordPointer = 0;
    }

    while (!file.available())
        ;
    return result;
}



String getCustomChars() {
  String usedChars = "";
  usedChars.reserve(64);
  String w;
  char c;







  uint32_t savedWordPointer = MorsePreferences::fileWordPointer;

  file.close(); file = SPIFFS.open("/player.txt");
  MorsePreferences::fileWordPointer = 0;
  wordReaderAtEof = false;

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
  wordReaderAtEof = false;



  MorsePreferences::fileWordPointer = savedWordPointer;

  return usedChars;
}



String cleanUpText(String text) {
    static char buf[128];
    int out = 0;

    text.toLowerCase();
    text = utf8umlaut(text);

    const char* src = text.c_str();
    int len = text.length();

    const char* kochChars = koch.morserinoKochChars;

    for (int i = 0; i < len && out < (int)sizeof(buf) - 1; ++i) {
        char c = src[i];
        if (strchr(kochChars, c) != NULL || c == 'P' || c == 'T' || c == 'C' ||
            (strchr(CWchars + 51, c) != NULL && c != '\0')) {
            buf[out++] = c;
        }
    }
    buf[out] = '\0';

    return String(buf);
}
# 4768 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void toUpperCaseM32(String &s) {
    s.toUpperCase();
    for (unsigned int i = 0; i + 1 < s.length(); ++i) {
        if ((uint8_t) s.charAt(i) == 0xC3) {
            uint8_t c = (uint8_t) s.charAt(i + 1);
            if (c >= 0xA0 && c <= 0xBE && c != 0xB7)
                s.setCharAt(i + 1, (char) (c - 0x20));
            ++i;
        }
    }
}

String utf8umlaut(const String& s) {


    static const struct { const char* pat; uint8_t patLen; const char* rep; uint8_t repLen; } table[] = {




        { "<err>", 5, "R", 1 },
        { "<ar>", 4, "+", 1 },
        { "<bk>", 4, "B", 1 },
        { "<bt>", 4, "=", 1 },
        { "<as>", 4, "S", 1 },
        { "<ka>", 4, "A", 1 },
        { "<kn>", 4, "N", 1 },
        { "<sk>", 4, "K", 1 },
        { "<ve>", 4, "E", 1 },
        { "<c>", 3, "C", 1 },
        { "<p>", 3, "P", 1 },
        { "<t>", 3, "T", 1 },

        { "\\ar", 3, "+", 1 },
        { "\\bk", 3, "B", 1 },
        { "\\bt", 3, "=", 1 },
        { "\\as", 3, "S", 1 },
        { "\\ka", 3, "A", 1 },
        { "\\kn", 3, "N", 1 },
        { "\\sk", 3, "K", 1 },
        { "\\ve", 3, "E", 1 },
        { "\\c", 2, "C", 1 },
        { "\\p", 2, "P", 1 },
        { "\\t", 2, "T", 1 },
    };
    static const int tableSize = sizeof(table) / sizeof(table[0]);

    static char buf[512];
    int out = 0;
    const int bufMax = sizeof(buf) - 1;



    String n = s;
    n.replace('[', '<');
    n.replace(']', '>');
    const char* src = n.c_str();
    int len = n.length();

    for (int i = 0; i < len && out < bufMax; ) {


        bool matched = false;
        for (int t = 0; t < tableSize; ++t) {
            if (i + table[t].patLen <= len &&
                memcmp(&src[i], table[t].pat, table[t].patLen) == 0) {

                if (out + table[t].repLen <= bufMax) {
                    memcpy(&buf[out], table[t].rep, table[t].repLen);
                    out += table[t].repLen;
                }
                i += table[t].patLen;
                matched = true;
                break;
            }
        }
        if (!matched && (uint8_t) src[i] == 0xC3 && i + 1 < len) {
            for (const char* f = foldLatin1((uint8_t) src[i + 1]); *f && out < bufMax; ++f)
                buf[out++] = *f;
            i += 2;
            matched = true;
        }
        if (!matched) {
            buf[out++] = src[i++];
        }
    }
    buf[out] = '\0';

    return String(buf);
}




void skipWords(uint32_t count) {
  while (count > 0) {
    getWord();
    --count;
  }
}
# 4880 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
static const uint16_t USB_LINE_MAX = 1024;





volatile unsigned long lastForeignUsbRxMs = 0;



void armForeignUsbRxDetect() {
#if !ARDUINO_USB_CDC_ON_BOOT
  Serial.onReceiveError([](hardwareSerial_error_t e) {
    if (e == UART_FRAME_ERROR || e == UART_BREAK_ERROR || e == UART_PARITY_ERROR)
      lastForeignUsbRxMs = millis();
  });
#endif
}
static bool usbDiscardLine = false;

static void serialLines();

void serialEvent() {
#ifdef CONFIG_WINKEYER
      if (MorseWinKeyer::ownsSerial()) {
        if (!MorseWinKeyer::takeTickServiced())
          MorseWinKeyer::serviceInput();
      } else
#endif
      serialLines();
#ifdef CONFIG_BLE_SERIAL
      bleSerialEvent();

#endif
}


static void serialLines() {
      while (Serial.available()) {

        char inChar = (char)Serial.read();
        if ((uint8_t) inChar >= 0x80 || ((uint8_t) inChar < 0x20 && inChar != '\n' && inChar != '\r' && inChar != '\t'))
          lastForeignUsbRxMs = millis();
#ifdef WK_NATIVE_USB



        if (MorseWinKeyer::topMenuArmed && MorseWinKeyer::topMenuByte((uint8_t) inChar)) {
          if (MorseWinKeyer::isOpen()) {
            inputString = "";
            return;
          }
          continue;
        }
#endif
        if (usbDiscardLine) {
          if (inChar == '\n')
            usbDiscardLine = false;
          continue;
        }

        inputString += inChar;
        if (inputString.length() > USB_LINE_MAX) {
          inputString = "";
          usbDiscardLine = true;
          if (m32protocol) {
            M32TargetScope usbOnly(M32Target::UsbOnly);
            MorseJSON::jsonError("LINE TOO LONG");
            m32out.flushReply();
          }
          continue;
        }


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
                  M32TargetScope usbOnly(M32Target::UsbOnly);
                  MorseJSON::jsonDevice(brd,vsn);
                }
              }




          m32out.flushReply();

          inputString = "";
          stringComplete = false;
      }
}

#ifdef CONFIG_BLE_SERIAL
# 5004 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
static const uint32_t BLE_CONSENT_TIMEOUT_MS = 20000;
static const uint32_t BLE_CONSENT_GRACE_MS = 60000;

static boolean bleAtTopMenu = false;
static boolean bleConsentPending = false;
static boolean bleConsentAsking = false;
static boolean bleConsentGranted = false;
static uint32_t bleConsentAt = 0;





static void bleConsentPrompt() {
  bleConsentPending = false;
  bleConsentAsking = true;

  MorseOutput::clearScrollLines();
  MorseOutput::printOnScroll(0, BOLD, 0, "Allow connect?");
  MorseOutput::printOnScroll(1, REGULAR, 0, "FN = yes");
  MorseOutput::printOnScroll(2, REGULAR, 0, "click = no");
  MorseOutput::refreshDisplay();




  MorseVoice::announce("Allow Bluetooth connection? F N for yes, click for no.");

  Buttons::modeButton.clicks = 0;
  Buttons::volButton.clicks = 0;
  boolean allowed = false;
  uint32_t started = millis();

  while (millis() - started < BLE_CONSENT_TIMEOUT_MS) {
    Buttons::modeButton.Update();
    if (Buttons::modeButton.clicks != 0) break;
    Buttons::volButton.Update();
    if (Buttons::volButton.clicks != 0) { allowed = true; break; }







    serialEvent();


    checkShutDown(false);
    delay(20);
  }
  Buttons::modeButton.clicks = 0;
  Buttons::volButton.clicks = 0;
  bleConsentAsking = false;

  M32TargetScope bleOnly(M32Target::BleOnly);
  if (allowed) {
    bleConsentGranted = true;
    bleConsentAt = millis();
    if (MorseBleSerial::linkUp()) {
      bleProtocol = true;
      MorseJSON::jsonDevice(brd, vsn);
    }





    MorseVoice::announce("Connection allowed.");
  }
  else {
    if (MorseBleSerial::linkUp())
      MorseJSON::jsonError("CONNECTION DECLINED");
    MorseVoice::announce("Connection refused.");
  }
}



boolean bleTopMenuService() {
  bleAtTopMenu = true;
  serialEvent();
  bleAtTopMenu = false;
  if (!bleConsentPending)
    return false;
  bleConsentPrompt();
  return true;
}
# 5102 "/home/ian/projects/morserino/upstream-pr/Software/src/Version 6 and newer/m32_v6.ino"
void bleSerialEvent() {
  uint8_t b;
  MorseBleSerial::pump();
  if (MorseBleSerial::takeLineReset()) {
    bleInputString = "";
    bleDiscardLine = false;
  }
  while (MorseBleSerial::readByte(b)) {
    if (bleDiscardLine) {
      if (b == '\n')
        bleDiscardLine = false;
      continue;
    }
    bleInputString += (char) b;
    if (bleInputString.length() > 400) {
      bleInputString = "";
      bleDiscardLine = true;
      M32TargetScope bleOnly(M32Target::BleOnly);
      MorseJSON::jsonError("BLE LINE TOO LONG");
      continue;
    }
    if (b != '\n')
      continue;
    bleInputString.trim();
    if (!bleProtocol) {
      if (bleInputString.equalsIgnoreCase("put device/protocol/on")) {
        M32TargetScope bleOnly(M32Target::BleOnly);
        if (bleConsentAsking) {



        }
        else if (bleConsentGranted && (millis() - bleConsentAt < BLE_CONSENT_GRACE_MS)) {




          bleProtocol = true;
          MorseJSON::jsonDevice(brd, vsn);
        }
        else if (!bleAtTopMenu) {
          MorseJSON::jsonError("DEVICE BUSY");
        }
        else {
          bleConsentPending = true;
          MorseJSON::jsonCreate("message", "CONFIRM ON DEVICE", "");
        }
      }
    }
    else if (bleInputString.equalsIgnoreCase("put device/protocol/off")) {


      M32TargetScope bleOnly(M32Target::BleOnly);
      MorseJSON::jsonCreate("end m32protocol", "Goodbye!", "");
      bleProtocol = false;
    }
    else if (bleInputString.equalsIgnoreCase("put device/protocol/on")) {



      M32TargetScope bleOnly(M32Target::BleOnly);
      MorseJSON::jsonDevice(brd, vsn);
    }
    else if (bleInputString.length() >= 20 &&
             bleInputString.substring(0, 20).equalsIgnoreCase("put device/protocol/")) {


      M32TargetScope bleOnly(M32Target::BleOnly);
      MorseJSON::jsonError("INVALID Value " + bleInputString.substring(20));
    }
    else
      serialDecode(bleInputString);
    bleInputString = "";
    return;
  }





  if (MorseBleSerial::takeRxOverflow()) {
    bleInputString = "";
    bleDiscardLine = false;
    M32TargetScope bleOnly(M32Target::BleOnly);
    MorseJSON::jsonError("BLE RX OVERFLOW");
  }
}
#endif






void serialDecode(String input) {
  String command;
  String firstArg, secondArg, thirdArg;

command.reserve(12);
firstArg.reserve(20);
secondArg.reserve(20);
thirdArg.reserve(20);

  if (!protocolActive())
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
          MorseJSON::jsonControl("volume", MorsePreferences::sidetoneVolume, MorsePreferences::volumeMin, MorsePreferences::volumeMax, true);
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
    else if (type == "configs") {



        if (token == "")
            MorseJSON::jsonParameterList();
        else if (token == "details") {
            boolean numeric = true;
            for (unsigned int k = 0; k < value.length(); ++k)
                if (!isDigit(value.charAt(k))) { numeric = false; break; }
            if (numeric)
                MorseJSON::jsonParameterDetails((uint8_t)value.toInt());
            else
                MorseJSON::jsonError("INVALID PARAMETER");
        }
        else
            MorseJSON::jsonError("INVALID ARGUMENT");
    }
    else if (type == "capabilities")
        MorseJSON::jsonCapabilities();
    else if (type == "resets")
        MorseJSON::jsonResets();
#ifdef CONFIG_M32KIP
    else if (type == "kip")
        MorseJSON::jsonKip();
#endif
#ifdef CONFIG_CW_GAME
    else if (type == "game") {
        if (token == "scores")
            MorseJSON::jsonGameScores();
        else
            MorseJSON::jsonError("INVALID ARGUMENT");
    }
#endif
    else if (type == "snapshots")
        MorseJSON::jsonSnapshots();
    else if (type == "snapshot") {

        int n = token.toInt() - 1;
        if (n >= 0 && n <= 7 && (MorsePreferences::snapShots & (1 << n)))
            MorseJSON::jsonGetSnapshot(n);
        else
            MorseJSON::jsonError("NO SUCH SNAPSHOT");
    }
    else if (type == "player") {
        MorseJSON::jsonGetPlayer();
    }
    else if (type == "customchars") {
        MorseJSON::jsonGetCustomChars();
    }
    else if (type == "practicechars") {
        MorseJSON::jsonGetPracticeChars();
    }
    else if (type == "hardware") {
        MorseJSON::jsonGetHardware();
    }
    else if (type == "battery") {
        MorseJSON::jsonGetBattery();
    }
    else if (type == "file") {
            if (token == "size")
                MorseJSON::jsonFileStats();
            else if (token == "text")
                MorseJSON::jsonFileText();
            else if (token == "first line" || token == "")
                MorseJSON::jsonFileFirstLine();
            else if (token == "list")
                MorseJSON::jsonFileList();
            else if (token == "parts") {

          if (MorsePreferences::filePartCount < 2) {
              MorseJSON::jsonCreate("fileparts", "Single file (not multipart)", "");
          } else {
              StaticJsonDocument<512> doc;
              JsonObject obj = doc.createNestedObject("fileparts");
              obj["count"] = MorsePreferences::filePartCount;
              obj["selected"] = MorsePreferences::filePartSelected + 1;
              JsonArray arr = obj.createNestedArray("parts");
              for (int i = 0; i < MorsePreferences::filePartCount; i++) {
                  arr.add(MorsePreferences::fileParts[i].name);
              }
              MorseJSON::jsonSend(doc);
          }
      } }
    else if (type == "wifi") {
      MorseJSON::jsonGetWifi();
    }
    else if (type == "kochlesson") {
      MorseJSON::jsonGetKoch();
    }
#ifdef CONFIG_PRACTICE_STATS
    else if (type == "stats") {
      if (token == "log" || token == "")
        MorseJSON::jsonStatsLog();
      else
        MorseJSON::jsonError("INVALID ARGUMENT");
    }
#endif
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



        M32TargetScope usbOnly(M32Target::UsbOnly);
        if (value.equalsIgnoreCase("off")) {
            MorseJSON::jsonCreate("end m32protocol", "Goodbye!", "");
            m32protocol = false;
        }
        else if (value.equalsIgnoreCase("on"))
            MorseJSON::jsonDevice(brd,vsn);
        else
            MorseJSON::jsonError("INVALID Value " + value);
      }
      else if (token == "reset" && value == "defaults") {






        Preferences resetPref;
        resetPref.begin("morserino", false);
        resetPref.putUChar("factoryReset", 1);
        resetPref.end();
        MorseJSON::jsonOK();
        Serial.flush();
#ifdef CONFIG_BLE_SERIAL
        MorseBleSerial::txFlush(500);
#endif
        delay(100);
        ESP.restart();
      }
      else MorseJSON::jsonError("INVALID NAME " + token);
    }





    else if (type == "kip") {
      if (token == "psk") {
        if (value.length() < 12)
          MorseJSON::jsonError("PSK TOO SHORT - 12 CHARACTERS MINIMUM");
        else {
          MorsePreferences::writeKipPsk(value);
          MorseJSON::jsonOK();
        }
      }
      else if (token == "port") {

        bool digits = value.length() > 0 && value.length() <= 5;
        for (unsigned i = 0; digits && i < value.length(); i++)
          if (!isDigit(value[i])) digits = false;
        long p = digits ? value.toInt() : -1;
        if (p < 0 || p > 65535 || (p != 0 && p < 1024))
          MorseJSON::jsonError("INVALID PORT - 1024 TO 65535, OR 0 FOR 7374");
        else {
          MorsePreferences::writeKipPort((uint16_t)p);
          MorseJSON::jsonOK();
        }
      }
      else MorseJSON::jsonError("INVALID NAME " + token);
    }

    else if (type == "player") {
      if (token == "call") {
        value = value.substring(0, 8);
        value.toUpperCase();
        Preferences playerPref;
        playerPref.begin("morserino", false);
        playerPref.putString("playerCall", value);
        playerPref.end();
        MorseJSON::jsonOK();
      }
      else if (token == "name") {
        value = value.substring(0, 8);
        value.toUpperCase();
        Preferences playerPref;
        playerPref.begin("morserino", false);
        playerPref.putString("playerName", value);
        playerPref.end();
        MorseJSON::jsonOK();
      }
      else
        MorseJSON::jsonError("INVALID PROPERTY player " + token);
    }

    else if (type == "customchars") {
      if (token == "set") {

        if (value.length() == 0) {
          MorsePreferences::useCustomChars = false;
          MorsePreferences::customCharSet = "";
        } else {
          MorsePreferences::useCustomChars = true;
          MorsePreferences::customCharSet = utf8ToCodes(value);
        }
        koch.setup();
        MorsePreferences::writePreferences("morserino");
        MorseJSON::jsonOK();
      }
      else if (token == "clear") {
        MorsePreferences::useCustomChars = false;
        MorsePreferences::customCharSet = "";
        koch.setup();
        MorsePreferences::writePreferences("morserino");
        MorseJSON::jsonOK();
      }
      else
        MorseJSON::jsonError("INVALID ACTION customchars " + token);
    }

    else if (type == "practicechars") {
      if (token == "set") {



        MorsePreferences::setPracticeChars(utf8ToCodes(value));
        MorseJSON::jsonOK();
      }
      else if (token == "clear") {
        MorsePreferences::setPracticeChars("");
        MorseJSON::jsonOK();
      }
      else
        MorseJSON::jsonError("INVALID ACTION practicechars " + token);
    }

    else if (type == "config") {
      if (setParameter(token, value))
        MorseJSON::jsonError("ERROR setting parameter " + token);
      else
        MorseJSON::jsonOK();
    }
#ifdef CONFIG_CW_GAME






    else if (type == "game") {
      if (token == "scores" && value.equalsIgnoreCase("clear")) {



        if (morseState == morseGame)
          MorseJSON::jsonError("GAME IN PROGRESS");
        else {
          MorsePreferences::clearGameScores();
          MorseJSON::jsonOK();
        }
      }
      else
        MorseJSON::jsonError("INVALID ACTION game/" + token);
    }
#endif

    else if (type == "snapshot") {
      if (token == "store") {
        int i = value.toInt() -1;
        if (i >= 0 && i <= 7 ) {
            if (MorsePreferences::doWriteSnapshot(i, MorsePreferences::menuPtr))
                MorseJSON::jsonOK();
            else
                MorseJSON::jsonError("SNAPSHOT STORE FAILED - NVS FULL?");
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
#ifdef CONFIG_BLE_SERIAL







                if (token == "recall" &&
                    MorsePreferences::pliste[posBluetoothOut].value != BLT_USE_SERIAL_PROT)
                    MorseBleSerial::stopWithNotice("BLE serial off", 300);
#endif
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

        File f = SPIFFS.open("/player.txt", "w");
        f.println(value);
        f.close();
        MorsePreferences::fileWordPointer = 0;
        MorsePreferences::writeWordPointer();

        MorseJSON::jsonOK();
      }
      else if (token == "append") {
        File f = SPIFFS.open("/player.txt", "a");
        f.println(value);
        f.close();

        MorseJSON::jsonOK();
      }
      else if (token == "begin") {

        if (uploadActive) {
          uploadFile.close();
        }
        String filename = value;
        if (!filename.startsWith("/"))
            filename = "/" + filename;
        uploadFile = SPIFFS.open(filename, "w");
        if (!uploadFile) {
          MorseJSON::jsonError("Cannot open file: " + filename);
          uploadActive = false;
        } else {
          uploadActive = true;
          uploadBytesWritten = 0;
          MorseJSON::jsonOK();
        }
      }
      else if (token == "data") {

        if (!uploadActive) {
          MorseJSON::jsonError("No upload in progress");
        } else {

          size_t inputLen = value.length();
          size_t outputLen = 0;

          mbedtls_base64_decode(NULL, 0, &outputLen,
                                (const unsigned char*)value.c_str(), inputLen);

          unsigned char decoded[256];
          int ret = mbedtls_base64_decode(decoded, sizeof(decoded), &outputLen,
                                (const unsigned char*)value.c_str(), inputLen);
          if (ret == 0 && outputLen > 0) {
            uploadFile.write(decoded, outputLen);
            uploadBytesWritten += outputLen;
            MorseJSON::jsonOK();
          } else {
            MorseJSON::jsonError("Base64 decode error");
          }
        }
      }
      else if (token == "end") {

        if (uploadActive) {
          String name = String(uploadFile.path());
          uploadFile.close();


          if (name == "/player.txt" || name == "player.txt") {
            for (int i = 0; i < MAX_FILE_PARTS; i++)
              MorsePreferences::fileParts[i].wordPointer = 0;
          }
          MorsePreferences::scanFileParts();
          MorsePreferences::writeFilePartData();
          uploadActive = false;
          if (name == "/player.txt" || name == "player.txt") {
            MorsePreferences::fileWordPointer = 0;
            MorsePreferences::writeWordPointer();
          }

          MorseJSON::jsonUploadComplete(name, uploadBytesWritten);
        } else {
          MorseJSON::jsonError("No upload in progress");
        }
      }
      else if (token == "delete") {

        String filename = value;
        if (!filename.startsWith("/"))
            filename = "/" + filename;
        if (SPIFFS.exists(filename)) {
          SPIFFS.remove(filename);
          MorseJSON::jsonOK();
        } else {
          MorseJSON::jsonError("File not found: " + filename);
        }
      }
      else if (token == "part") {
        uint8_t nr = value.toInt();
        if (nr >= 1 && nr <= MorsePreferences::filePartCount) {
            MorsePreferences::filePartSelected = nr - 1;
            MorsePreferences::writeFilePartData();
            remoteFilePartSelect = nr - 1;
            MorseJSON::jsonFilePart(
                String(MorsePreferences::fileParts[nr - 1].name),
                nr - 1,
                MorsePreferences::filePartCount);
        } else if (MorsePreferences::filePartCount == 0) {
            MorseJSON::jsonError("File is not multipart");
        } else {
            MorseJSON::jsonError("Part number out of range (1-" +
                String(MorsePreferences::filePartCount) + ")");
        }
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
      else value = "";

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




        uint8_t nr = 0;
        if (value != "") {
          nr = (char) value.toInt();
          if (!(nr > 0 && nr < menuN && MorseMenu::isRemotelyExecutable(nr))) {
            MorseJSON::jsonError("NOT EXECUTABLE - Menu No " + value);
            return;
          }
        }
        if (m32state != menu_loop) {
          MorseJSON::jsonError("NOT AT MENU - stop the running mode first");
          return;
        }
        goToMenu = false;
        if (nr) {
          MorsePreferences::newMenuPtr = nr;
          executeMenu = true;
          if (token == "start now")
            executeNow = true;
        }
        else {
          executeMenu = true;
          if (token == "start now" && MorseMenu::isRemotelyExecutable(MorsePreferences::menuPtr))
            executeNow = true;
        }
        MorseJSON::jsonOK();
      }
      else if (token == "set") {
        uint8_t nr = (char) value.toInt();
        if (nr > 0 && nr < menuN) {
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
#ifdef CONFIG_PRACTICE_STATS




    else if (type == "time") {
      if (token == "set") {
        time_t epoch = (time_t) value.toInt();
        if (epoch > 0) {
          MorsePracticeStats::setWallClock(epoch);
          MorseJSON::jsonOK();
        }
        else
          MorseJSON::jsonError("INVALID EPOCH " + value);
      }
      else
        MorseJSON::jsonError("INVALID ACTION time " + token);
    }

    else if (type == "stats") {
      if (token == "clear") {
        MorsePracticeStats::clearLog();
        MorseJSON::jsonOK();
      }
      else if (token == "enabled") {
        MorsePracticeStats::setEnabled(value == "1" || value == "true");
        MorseJSON::jsonOK();
      }
      else
        MorseJSON::jsonError("INVALID ACTION stats " + token);
    }
#endif

    else if (type == "kochlesson") {
        uint8_t nr = (char) token.toInt();
        if (nr >= MorsePreferences::kochMinimum && nr <= MorsePreferences::kochMaximum) {
          MorsePreferences::kochFilter = nr;
          koch.setup();


          MorsePreferences::writePreferences("morserino");
          MorseJSON::jsonOK();
        }
        else
          MorseJSON::jsonError("INVALID KOCH LESSON " + token);
    }

    else if (type == "cw") {
      if (token == "play" || token == "repeat" || token == "recall") {
        if (m32state != menu_loop && (morseState == morseKeyer || morseState == morseTrx || morseState == kipKeyer)) {
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
            MorsePreferences::handleKochSequence(true);
      else if (i == posCarouselStart)
            MorsePreferences::handleCarouselChange();
#ifdef CONFIG_SOUND_I2S
      else if (i == posSidetoneShape)
            MorseOutput::setSidetoneEnvelope(MorsePreferences::pliste[i].value);
#endif
      found = true;
      break;
    }


    if (found) {
      MorsePreferences::writePreferences("morserino");
      return false;
    }
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
  MorsePreferences::wlanChoice = i;
  MorsePreferences::writePreferences("morserino");
  MorseJSON::jsonOK();
}


void stopPlayCw() {
            keyOut(false,true,0,0);
            playCW = false;
            repeatPlayCW = false;
            playCWBuffer = clearText = CWword = ""; CWwordPos = 0;
            playCWMaxIndex = 0;
            startFirst = true;
            MorseJSON::jsonOK();
}