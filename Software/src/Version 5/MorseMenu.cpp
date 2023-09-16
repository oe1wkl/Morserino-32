/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
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

#include "MorseMenu.h"
#include "MorseOutput.h"
#include "MorseDecoder.h"

using namespace MorseMenu;

//////// variables and constants for the modus menu

//boolean MorseMenu::inMenuLoop = false;


const String menuText [menuN] = {
  "",
  "CW Keyer", // 1
  
  "CW Generator", // 2
    "Random",
    "CW Abbrevs",
    "English Words",
    "Call Signs",
    "Mixed",
    "File Player",

  "Echo Trainer", // 9
    "Random",
    "CW Abbrevs",
    "English Words",
    "Call Signs",
    "Mixed",
    "File Player",
  
  "Koch Trainer", // 16
    "Select Lesson",
    "Learn New Chr",
    "CW Generator",   // 19
      "Random",   // 20
      "CW Abbrevs",
      "English Words",
      "Mixed",
    "Echo Trainer",   // 24
      "Random",
      "CW Abbrevs",
      "English Words",
      "Mixed",
      "Adapt. Rand.",
  
  "Transceiver",    // 30
    "LoRa Trx",
    "WiFi Trx",
    "iCW/Ext Trx",
  
  "CW Decoder",     // 34

  "WiFi Functions", // 35
    "Disp MAC Addr",
    "Config WiFi",
    "Check WiFi",
    "Upload File",
    "Update Firmw", //40
    "Wifi Select", //41
  
  
  "Go To Sleep" } ; // 42

enum navi {naviLevel, naviLeft, naviRight, naviUp, naviDown };
       

const uint8_t menuNav [menuN] [5] = {                   // { level, left, right, up, down}
  { 0,0,0,0,0},                                         // 0 = dummy                
  {0,_goToSleep,_gen,_dummy,0},                         // 1 keyer  -e
  {0,_keyer,_echo,_dummy,_genRand},                     // 2 generator
  {1,_genPlayer,_genAbb,_gen,0},                        // 3 gen random -e
  {1,_genRand,_genWords,_gen,0},                        // 4 gen Abb  -e
  {1,_genAbb,_genCalls,_gen,0},                         // 5 gen words  -e
  {1,_genWords,_genMixed,_gen,0},                       // 6 gen calls  -e
  {1,_genCalls,_genPlayer,_gen,0},                      // 7 gen mixed  -e
  {1,_genMixed,_genRand,_gen,0},                        // 8 gen player  -e
  {0,_gen,_koch,_dummy,_echoRand},                      // 9 echo tr
  {1,_echoPlayer,_echoAbb,_echo,0},                     // 10 echo random  -e
  {1,_echoRand,_echoWords,_echo,0},                     // 11 echo abb  -e
  {1,_echoAbb,_echoCalls,_echo,0},                      // 12 echo words  -e
  {1,_echoWords,_echoMixed,_echo,0},                    // 13 echo calls  -e
  {1,_echoCalls,_echoPlayer,_echo,0},                   // 14 echo mixed  -e
  {1,_echoMixed,_echoRand,_echo,0},                     // 15 echo player  -e
  {0,_echo,_trx,_dummy,_kochSel},                       // 16 koch
  {1,_kochEcho,_kochLearn,_koch,0},                     // 17 koch select  -e!!?
  {1,_kochSel,_kochGen,_koch,0},                        // 18 koch learn new
  {1,_kochLearn,_kochEcho,_koch,_kochGenRand},          // 19 koch gen
  {2,_kochGenMixed,_kochGenAbb,_kochGen,0},             // 20 koch gen random  -e
  {2,_kochGenRand,_kochGenWords,_kochGen,0},            // 21 koch gen abb  -e
  {2,_kochGenAbb,_kochGenMixed,_kochGen,0},             // 22 koch gen words  -e
  {2,_kochGenWords,_kochGenRand,_kochGen,0},            // 23 koch gen mixed  -e
  {1,_kochGen,_kochSel,_koch,_kochEchoRand},            // 24 koch echo
  {2,_kochEchoAdaptive,_kochEchoAbb,_kochEcho,0},       // 25 koch echo random  -e
  {2,_kochEchoRand,_kochEchoWords,_kochEcho,0},         // 26 koch echo abb  -e
  {2,_kochEchoAbb,_kochEchoMixed,_kochEcho,0},          // 27 koch echo words  -e
  {2,_kochEchoWords,_kochEchoAdaptive,_kochEcho,0},     // 28 koch echo mixed  -e
  {2,_kochEchoMixed,_kochEchoRand,_kochEcho,0},         // 29 koch echo adaptive  -e
  {0,_koch,_decode,_dummy,_trxLora},                    // 30 transceiver
  {1,_trxIcw,_trxWifi,_trx,0},                          // 31 lora  -e
  {1,_trxLora,_trxIcw,_trx,0},                          // 32 wifi  -e
  {1,_trxWifi,_trxLora,_trx,0},                         // 33 icw  -e
  {0,_trx,_wifi,_dummy,0},                              // 34 decoder  -e
  {0,_decode,_goToSleep,_dummy,_wifi_mac},              // 35 WiFi
  {1,_wifi_select,_wifi_config,_wifi,0},                // 36 Disp Mac  -e!!
  {1,_wifi_mac,_wifi_check,_wifi,0},                    // 37 Config Wifi    --NE!
  {1,_wifi_config,_wifi_upload,_wifi,0},                // 38 Check WiFi  -e!!
  {1,_wifi_check,_wifi_update,_wifi,0},                 // 39 Upload File    --NE!
  {1,_wifi_upload,_wifi_select,_wifi,0},                // 40 Update Firmware  --NE!
  {1,_wifi_update,_wifi_mac,_wifi,0},                   // 41 Select network  --NE!!
  {0,_wifi,_keyer,_dummy,0}                             // 42 goto sleep  -e
};

//String MorseMenu::cmdPath;   // used to create string for json

////// The MENU


void MorseMenu::menu_() {
   MorsePreferences::newMenuPtr = MorsePreferences::menuPtr;
   uint8_t disp = 0;
   int t, command;
   m32state = menu_loop;
      
    LoRa.idle();
    WiFi.disconnect(true, false);
    genIsActive = false;
    cleanStartSettings();
    MorseOutput::clearScroll();                  // clear the buffer

    keyOut(false, true, 0, 0);
    keyOut(false, false, 0, 0);
    encoderState = speedSettingMode;             // always start with this encoderstate (decoder will change it anyway)
    MorsePreferences::setCurrentOptions(MorsePreferences::allOptions, MorsePreferences::allOptionsSize);
    MorsePreferences::writeWordPointer();
    file.close();                               // just in case it is still open....
    MorseOutput::clearDisplay();
    
    while (true) {                          // we wait for a click (= selection) or to get some serial input
        serialEvent();

        if (disp != MorsePreferences::newMenuPtr) {
          disp = MorsePreferences::newMenuPtr;
          MorseMenu::menuDisplay(disp);
        }
        if (quickStart) {
            quickStart = false;
            command = 1;
            delay(250);
            if (m32protocol)
              jsonCreate("message", "Quick Start", "");
            MorseOutput::printOnScroll(2, REGULAR, 1, "QUICK START");
            Heltec.display -> display();
            delay(600);
            MorseOutput::clearDisplay();
        }
        else if (executeMenu) {
            executeMenu = false;
            command = 1;
        }
        else {           
            Buttons::modeButton.Update();
            command = Buttons::modeButton.clicks;
        }

        switch (command) {                                          // actions based on encoder button
          case 2: if (MorsePreferences::setupPreferences(MorsePreferences::newMenuPtr))                       // all available options when called from top menu
                    MorsePreferences::newMenuPtr = MorsePreferences::menuPtr;
                  MorseMenu::menuDisplay(MorsePreferences::newMenuPtr);
                  m32state = menu_loop;
                  break;
          case 1: // check if we have a submenu or if we execute the selection
                  //DEBUG("newMP: " + String(MorsePreferences::newMenuPtr) + " navi: " + String(menuNav[MorsePreferences::newMenuPtr][naviDown]));
                  if (menuNav[MorsePreferences::newMenuPtr][naviDown] == 0) {
                      MorsePreferences::menuPtr = MorsePreferences::newMenuPtr;
                      disp = 0;
                      if (MorsePreferences::menuPtr < _wifi) {                        // remember last executed, unless it is a wifi function or shutdown
                        MorsePreferences::writeLastExecuted(MorsePreferences::newMenuPtr);
                      }
                      if (MorseMenu::menuExec())
                        return;
                  } else {
                      MorsePreferences::newMenuPtr = menuNav[MorsePreferences::newMenuPtr][naviDown];
                  }
                  break;
          case -1:  // we need to go one level up, if possible
                  if (menuNav[MorsePreferences::newMenuPtr][naviUp] != 0) 
                      MorsePreferences::newMenuPtr = menuNav[MorsePreferences::newMenuPtr][naviUp];
          default: break;
        }

       if ((t=checkEncoder())) {
          MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);         /// click
          MorsePreferences::newMenuPtr =  menuNav [MorsePreferences::newMenuPtr][(t == -1) ? naviLeft : naviRight];
       }

       Buttons::volButton.Update();
    
       switch (Buttons::volButton.clicks) {
          case -1:  audioLevelAdjust();                         /// for adjusting line-in audio level (at the same time keying tx and sending oudio on line-out
                    MorseOutput::clearDisplay();
                    MorseMenu::menuDisplay(disp);
                    break;
          case 2:   MorseOutput::decreaseBrightness();
                    MorseMenu::menuDisplay(disp);
                    break;
       }
       checkShutDown(false);                  // check for time out   
  } // end while - we leave as soon as the button has been pressed
  // MorseMenu::inMenuLoop = false;
} // end menu_() 


void MorseMenu::menuDisplay(uint8_t ptr) {
  //DEBUG("Level: " + (String) menuNav [ptr][naviLevel] + " " + menuText[ptr]);
  uint8_t oneUp = menuNav[ptr][naviUp];
  uint8_t twoUp = menuNav[oneUp][naviUp];
  uint8_t oneDown = menuNav[ptr][naviDown];


  MorseOutput::printOnStatusLine( true, 0,  "Select Modus:     ");
  
  // delete previous content
  MorseOutput::clearThreeLines();
  
  /// level 0: top line, possibly ".." on line 1
  /// level 1: higher level on 0, item on 1, possibly ".." on 2
  /// level 2: higher level on 1, highest level on 0, item on 2
  switch (menuNav [ptr][naviLevel]) {
    case 2: //cmdPath = menuText[twoUp] + "/" + menuText[oneUp] + "/" + menuText[ptr];
            MorseOutput::printOnScroll(2, BOLD, 0, menuText[ptr]);
            MorseOutput::printOnScroll(1, REGULAR, 0, menuText[oneUp]);
            MorseOutput::printOnScroll(0, REGULAR, 0, menuText[twoUp]);
            break;
    case 1: //cmdPath = menuText[oneUp] + "/" + menuText[ptr] + (oneDown ? ("/..") : "");
            if (oneDown)
                MorseOutput::printOnScroll(2, REGULAR, 0, String(".."));
            MorseOutput::printOnScroll(1, BOLD, 0, menuText[ptr]);
            MorseOutput::printOnScroll(0, REGULAR, 0, menuText[oneUp]);
            break;
    case 0: //cmdPath = menuText[ptr] + (oneDown ? ("/..") : "");
            if (oneDown)
            MorseOutput::printOnScroll(1, REGULAR, 0, String(".."));
            MorseOutput::printOnScroll(0, BOLD, 0, menuText[ptr]);
            break;
  }
  if (m32protocol) {
      //cmdPath = MorseMenu::getMenuPath(ptr);
      jsonMenu( MorseMenu::getMenuPath(ptr), (unsigned int) ptr, (m32state == menu_loop ? false : true), MorseMenu::isRemotelyExecutable(ptr));
  }
  
}

String MorseMenu::getMenuPath(uint8_t ptr) {
    uint8_t oneUp = menuNav[ptr][naviUp];
    uint8_t twoUp = menuNav[oneUp][naviUp];
    uint8_t oneDown = menuNav[ptr][naviDown];
    String cmdPath; cmdPath.reserve(80);

    switch (menuNav [ptr][naviLevel]) {
      case 2: cmdPath = menuText[twoUp] + "/" + menuText[oneUp] + "/" + menuText[ptr];
              break;
      case 1: cmdPath = menuText[oneUp] + "/" + menuText[ptr] + (oneDown ? ("/..") : "");
              break;
      case 0: cmdPath = menuText[ptr] + (oneDown ? ("/..") : "");
              break;
  }
  return cmdPath;
}


boolean MorseMenu::menuExec() {                                          // return true if we should  leave menu after execution, true if we should stay in menu

  uint32_t wcount = 0;
//  String peer;
//  const char* peerHost;
  String s;

  if (m32protocol && (MorsePreferences::menuPtr != _kochSel)) 
      jsonActivate(ACT_ON); 
      
  m32state = active_loop;

  kochActive = false;
  keyerState = IDLE_STATE;
  
  switch (MorsePreferences::menuPtr) {
    case  _keyer:  /// keyer
                MorsePreferences::setCurrentOptions(MorsePreferences::keyerOptions, MorsePreferences::keyerOptionsSize);
                morseState = morseKeyer;
                showStartDisplay("CW Keyer Start", "", "", 400);
                clearPaddleLatches();
                if(MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY)
                  keyDecoder.setup();
                executeNow = false;
                return true;
                break;
     case _genRand:
     case _genAbb:
     case _genWords:
     case _genCalls:
     case _genMixed:      /// generator
                generatorMode = (GEN_TYPE) (MorsePreferences::menuPtr - 3);                   /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                MorsePreferences::setCurrentOptions(MorsePreferences::generatorOptions, MorsePreferences::generatorOptionsSize);
                goto startGenerator;
     case _genPlayer:  
                generatorMode = (GEN_TYPE) (MorsePreferences::menuPtr - 3);                   /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                MorsePreferences::setCurrentOptions(MorsePreferences::playerOptions, MorsePreferences::playerOptionsSize);
                file = SPIFFS.open("/player.txt");                            // open file
                //skip p_fileWordPointer words, as they have been played before
                wcount = MorsePreferences::fileWordPointer > 1 ? MorsePreferences::fileWordPointer-1  : 0;
                MorsePreferences::fileWordPointer = 0;
                skipWords(wcount);
     startGenerator:
                startFirst = true;
                firstTime = true;
                morseState = morseGenerator;
                showStartDisplay("Generator     ", "Start / Stop  ", "press Paddle  ", 1250);
                if (MorsePreferences::pliste[posLoraCwTransmit].value == 2)
                  if (!setupWifi())
                    return false;
                return true;
                break;
      case  _echoRand:
      case  _echoAbb:
      case  _echoWords:
      case  _echoCalls:
      case  _echoMixed:
                MorsePreferences::setCurrentOptions(MorsePreferences::echoTrainerOptions, MorsePreferences::echoTrainerOptionsSize);

                generatorMode = (GEN_TYPE) (MorsePreferences::menuPtr - 10);                /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                goto startEcho;
      case  _echoPlayer:    /// echo trainer
                generatorMode = (GEN_TYPE) (MorsePreferences::menuPtr - 10);                /// 0 = RANDOMS ... 4 = MIXED, 5 = PLAYER
                MorsePreferences::setCurrentOptions(MorsePreferences::echoPlayerOptions, MorsePreferences::echoPlayerOptionsSize);
                file = SPIFFS.open("/player.txt");                            // open file
                //skip p_fileWordPointer words, as they have been played before
                wcount = MorsePreferences::fileWordPointer > 1 ? MorsePreferences::fileWordPointer-1  : 0;
                MorsePreferences::fileWordPointer = 0;
                skipWords(wcount);
       startEcho:
                startFirst = true;
                morseState = echoTrainer;
                echoStop = false;
                showStartDisplay(generatorMode == KOCH_LEARN ? "New Character:" : "Echo Trainer:", "Start:       ", "Press paddle ", 1250);
                if(MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY)
                  keyDecoder.setup();
                executeNow = false;
                return true;
                break;
      case  _kochSel: // Koch Select 
                MorsePreferences::displayKeyerPreferencesMenu(posKochFilter);
                MorsePreferences::adjustKeyerPreference(posKochFilter);
                MorsePreferences::writePreferences("morserino");
                executeNow = false;
                m32state = menu_loop;
                return false;
                break;
      case  _kochLearn:   // Koch Learn New .  /// just a new generatormode....
                generatorMode = KOCH_LEARN;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                executeNow = false;
                goto startEcho;
      case  _kochGenRand: // RANDOMS 
                generatorMode = RANDOMS;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochGenAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochGenWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochGenMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochEchoRand: // Koch Echo Random
                generatorMode = RANDOMS;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoAdaptive: // Koch Echo Adaptive - 6
                generatorMode = KOCH_ADAPTIVE;
                kochActive = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);

                // re-run the setup, this will reset the character probabilities
                koch.setup();
                goto startEcho;
      case  _trxLora: // LoRa Transceiver
                generatorMode = RANDOMS;  // to reset potential KOCH_LEARN
                MorsePreferences::setCurrentOptions(MorsePreferences::loraTrxOptions, MorsePreferences::loraTrxOptionsSize);
                morseState = loraTrx;
                showStartDisplay("", "Start LoRa Trx", "", 500);
                clearPaddleLatches();
                clearText = "";
                LoRa.receive();
                executeNow = false;
                return true;
                break;
      case  _trxWifi: // Wifi Transceiver
                generatorMode = RANDOMS;  // to reset potential KOCH_LEARN
                MorsePreferences::setCurrentOptions(MorsePreferences::wifiTrxOptions, MorsePreferences::wifiTrxOptionsSize);
                morseState = wifiTrx;
                MorseOutput::clearDisplay();
                MorseOutput::printOnScroll(0, REGULAR, 0, "Connecting...");
                if (m32protocol)
                  jsonCreate("message", "Connecting...", "");

                if (!setupWifi())
                  return false;
                //DEBUG("Peer IP: " + peerIP.toString());
                s = peerIP.toString();
                showStartDisplay("", "Start Wifi Trx", s  == "255.255.255.255" ?"IP Broadcast" : s, 1500);

                MorseWiFi::audp.listen(MORSERINOPORT); // listen on port 7373
                MorseWiFi::audp.onPacket(onWifiReceive);
                clearPaddleLatches();
                //keyTx = false;
                clearText = "";
                executeNow = false;
                return true;
                break;
      case  _trxIcw: /// icw/ext TRX
                MorsePreferences::setCurrentOptions(MorsePreferences::extTrxOptions, MorsePreferences::extTrxOptionsSize);
                morseState = morseTrx;
                MorseOutput::clearDisplay();
                MorseOutput::printOnScroll(1, REGULAR, 0, "Start CW Trx" );
                if (m32protocol)
                  jsonCreate("message", "Start CW Transceiver", "");
                clearPaddleLatches();
                goto setupDecoder;

      case  _decode: /// decoder
                MorsePreferences::setCurrentOptions(MorsePreferences::decoderOptions, MorsePreferences::decoderOptionsSize);
                morseState = morseDecoder;
                  /// here we will do the init for decoder mode
                encoderState = volumeSettingMode;
                MorseOutput::clearDisplay();
                MorseOutput::printOnScroll(1, REGULAR, 0, "Start Decoder" );
                if (m32protocol)
                  jsonCreate("message", "Start Decoder", "");
      setupDecoder:
                speedChanged = true;
                delay(650);
                cleanupScreen();
                MorseOutput::drawInputStatus(false);

                displayCWspeed();
                MorseOutput::displayVolume(encoderState == speedSettingMode, MorsePreferences::sidetoneVolume);                                     // sidetone volume
                keyDecoder.setup();
                audioDecoder.setup();
                executeNow = false;
                return true;
                break;
      case  _wifi_mac:
      case  _wifi_config:
      case _wifi_check:
      case _wifi_upload:
      case _wifi_update:
                  MorseWiFi::menuExec((uint8_t) MorsePreferences::menuPtr);
                  break;
      case _wifi_select:
                  MorseWiFi::menuNetSelect();
                  break;
      case  _goToSleep: /// deep sleep
                checkShutDown(true);
      default:  break;
  }
  m32state = menu_loop;
  return false;
}   /// end menuExec()

boolean MorseMenu::setupWifi() {
  String peer;
  peer.reserve(24);
  const char* peerHost;

//// if not true WiFi has not been configured or is not available, hence return false!
  if (! MorseWiFi::wifiConnect()) {
    delay(1000);    // wait a bit
    return(false);
  }

  if (MorsePreferences::wlanTRXPeer.length() == 0) 
      peer = "255.255.255.255";     // send to local broadcast IP if not set
  else
      peer = MorsePreferences::wlanTRXPeer;
  peerHost = peer.c_str();
  if (!peerIP.fromString(peerHost)) {    // try to interpret the peer as an ip address...
    //DEBUG("hostname: " + String(peerHost));
      int err = WiFi.hostByName(peerHost, peerIP); // ...and resolve peer into ip address if that fails
    //DEBUG("errcode: " + String(err));
      if (err != 1)                       // if that fails too, use broadcast
        peerIP.fromString("255.255.255.255");
  }
  return true;
}


void MorseMenu::cleanupScreen() {
    MorseOutput::clearDisplay();
    updateTopLine();
    MorseOutput::clearScroll();      // clear the buffer}
}

void MorseMenu::showStartDisplay(String l0, String l1, String l2, int pause) {
    MorseOutput::clearDisplay();
    if (l0.length())
        MorseOutput::printOnScroll(0, REGULAR, 0, l0);
    if (l1.length())
        MorseOutput::printOnScroll(1, REGULAR, 0, l1);
    if (l2.length())
        MorseOutput::printOnScroll(2, REGULAR, 0, l2);
    if (m32protocol)
        jsonCreate("message", l0  + l1 + l2, "");
    delay(pause);
    cleanupScreen();
}

boolean MorseMenu::isRemotelyExecutable(uint8_t ptr) {
  if (menuNav[ptr][naviDown] == 0) {
    switch (ptr) {
      case 37:                                        // these WIFi related functions cannot be executed remotely, but must be done directly on the device
      case 39:
      case 40:
      case 41:
        return false;
    }
    return true;
  }
  else
    return false;
}
