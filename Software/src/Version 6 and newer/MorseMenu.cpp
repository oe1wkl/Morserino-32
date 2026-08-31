/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
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

#include "MorseMenu.h"
#include "MorseOutput.h"
#include "MorseDecoder.h"
#include "MorseJSON.h"
#include "M32ProtocolOut.h"    // protocolActive(): emission gate across transports
#ifdef CONFIG_BLE_SERIAL
#include "MorseBleSerial.h"    // BLE Serial lifecycle: top-menu restart backstop, WiFi suspension
#endif
#include "MorseVoice.h"

#ifdef CONFIG_PRACTICE_STATS
#include "MorsePracticeStats.h"
#endif
static void suspendBleSerialForWifi();   // defined below setupWifi(); no-op without CONFIG_BLE_SERIAL

#ifdef CONFIG_TFT
#include "MorseGameMode.h"

// Defined in m32_v6.ino at global scope. True for the FIRST menu_()
// invocation after a memory-clearing reboot triggered by sprite-allocation
// failure. We use it to suppress the "QUICK START" overlay so the
// post-reboot resume into the game looks silent.
extern bool memoryReboot;
#endif

#ifdef CONFIG_CW_GAME
  #include "MorseGame.h"
  #include "MorsePileup.h"
  #include "MorseRadioCave.h"
  #include "MorseMorsel.h"
  #include "MorseTrailblazer.h"
  #include "MorseFoxHunt.h"
  #include "MorseMemoryChain.h"
#endif

#ifdef CONFIG_QSO_BOT
  #include "MorseQsoBot.h"
#endif

#ifdef LORA_RADIOLIB
#include <RadioLib.h>
extern RADIO radio;
#endif

#ifdef CONFIG_BLUETOOTH_KEYBOARD
#include "MorseBluetooth.h"
#endif

#ifdef CONFIG_MCP73871
//extern volatile bool powerpath_event;
extern uint16_t volt;
extern int16_t batteryVoltage();
#endif

#ifdef CONFIG_CW_GAME
#include "MorseGame.h"
#endif

using namespace MorseMenu;

//////// variables and constants for the modes menu

//boolean MorseMenu::inMenuLoop = false;


const char* const menuText[menuN]  = {
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
#ifndef LORA_DISABLED
    "LoRa Trx",
#endif
    "WiFi Trx",
    "iCW/Ext Trx",
#ifdef CONFIG_QSO_BOT
    "QSO Bot",
        "SOTA/POTA", "Standard", "Contest",
#endif

  "CW Decoder",
#ifdef CONFIG_CW_GAME
"Games",
    "Morse Invaders", "Fight Pileup", "Radio Cave", "Morsel",
#endif

  "WiFi Functions",
    "Disp MAC Addr",
    "Config WiFi",
    "Check WiFi",
#ifndef CONFIG_AUDIO_A11Y                    // not in the Accessibility Edition (see morsedefs.h)
    "Upload File",
    "Update Firmw",
#endif
    "Wifi Select",
#ifdef CONFIG_PRACTICE_STATS
    "Practice Stats",
#endif

  "Go To Sleep"
#ifdef CONFIG_CW_GAME
  , "Trailblazer"
  , "Fox Hunt"
  , "Memory Chain"
#endif
  , "Practice Set"    // _genPractice  - sibling of CW Generator's Random/CW Abbrevs/.../File Player
  , "Practice Set"    // _echoPractice - sibling of Echo Trainer's Random/CW Abbrevs/.../File Player
  , "Preview Char"    // _kochPreview  - sibling of Koch Trainer's Select Lesson/Learn New Chr
  } ;

enum navi {naviLevel, naviLeft, naviRight, naviUp, naviDown };

// The WiFi ring's left-hand neighbour of "Wifi Select": normally "Update Firmw", but the
// Accessibility Edition drops that entry and "Upload File" with it (see morsedefs.h), so
// the ring closes over "Check WiFi" instead.
#ifdef CONFIG_AUDIO_A11Y
  #define _wifi_beforeSelect _wifi_check
#else
  #define _wifi_beforeSelect _wifi_update
#endif

bool EspNowIsActive = false;

const uint8_t menuNav [menuN] [5] = {                   // { level, left, right, up, down}
  { 0,0,0,0,0},                                         // 0 = dummy
  {0,_goToSleep,_gen,_dummy,0},                         // 1 keyer  -e
  {0,_keyer,_echo,_dummy,_genRand},                     // 2 generator
  {1,_genPractice,_genAbb,_gen,0},                      // 3 gen random -e (left <- Practice Set)
  {1,_genRand,_genWords,_gen,0},                        // 4 gen Abb  -e
  {1,_genAbb,_genCalls,_gen,0},                         // 5 gen words  -e
  {1,_genWords,_genMixed,_gen,0},                       // 6 gen calls  -e
  {1,_genCalls,_genPlayer,_gen,0},                      // 7 gen mixed  -e
  {1,_genMixed,_genPractice,_gen,0},                    // 8 gen player  -e (right -> Practice Set)
  {0,_gen,_koch,_dummy,_echoRand},                      // 9 echo tr
  {1,_echoPractice,_echoAbb,_echo,0},                    // 10 echo random  -e (left <- Practice Set)
  {1,_echoRand,_echoWords,_echo,0},                     // 11 echo abb  -e
  {1,_echoAbb,_echoCalls,_echo,0},                      // 12 echo words  -e
  {1,_echoWords,_echoMixed,_echo,0},                    // 13 echo calls  -e
  {1,_echoCalls,_echoPlayer,_echo,0},                   // 14 echo mixed  -e
  {1,_echoMixed,_echoPractice,_echo,0},                  // 15 echo player  -e (right -> Practice Set)
  {0,_echo,_trx,_dummy,_kochSel},                       // 16 koch
  {1,_kochEcho,_kochLearn,_koch,0},                     // 17 koch select  -e!!?
  {1,_kochSel,_kochPreview,_koch,0},                    // 18 koch learn new (right -> Preview Char, appended below)
  {1,_kochLearn,_kochEcho,_koch,_kochGenRand},          // 19 koch gen (left <- Preview Char, appended below)
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
#ifdef LORA_DISABLED
  #ifdef CONFIG_QSO_BOT
    {0,_koch,_decode,_dummy,_trxWifi},                  // _trx
    {1,_qsoBot,_trxIcw,_trx,0},                         // _trxWifi  (left wraps via _qsoBot)
    {1,_trxWifi,_qsoBot,_trx,0},                        // _trxIcw   (right wraps to _qsoBot)
    {1,_trxIcw,_trxWifi,_trx,_qsoSotaPota},             // _qsoBot   (level 1; descends to _qsoSotaPota)
    {2,_qsoContest,_qsoStandard,_qsoBot,0},             // _qsoSotaPota
    {2,_qsoSotaPota,_qsoContest,_qsoBot,0},             // _qsoStandard
    {2,_qsoStandard,_qsoSotaPota,_qsoBot,0},            // _qsoContest
  #else
    {0,_koch,_decode,_dummy,_trxWifi},                  // _trx (no LoRa, first child is WiFi)
    {1,_trxIcw,_trxIcw,_trx,0},                         // _trxWifi  (2-item wrap)
    {1,_trxWifi,_trxWifi,_trx,0},                       // _trxIcw   (2-item wrap)
  #endif
#else
  #ifdef CONFIG_QSO_BOT
    {0,_koch,_decode,_dummy,_trxLora},                  // _trx
    {1,_qsoBot,_trxWifi,_trx,0},                        // _trxLora  (left wraps via _qsoBot)
    {1,_trxLora,_trxIcw,_trx,0},                        // _trxWifi
    {1,_trxWifi,_qsoBot,_trx,0},                        // _trxIcw   (right wraps to _qsoBot)
    {1,_trxIcw,_trxLora,_trx,_qsoSotaPota},             // _qsoBot   (level 1; descends to _qsoSotaPota)
    {2,_qsoContest,_qsoStandard,_qsoBot,0},             // _qsoSotaPota
    {2,_qsoSotaPota,_qsoContest,_qsoBot,0},             // _qsoStandard
    {2,_qsoStandard,_qsoSotaPota,_qsoBot,0},            // _qsoContest
  #else
    {0,_koch,_decode,_dummy,_trxLora},                  // _trx (has LoRa)
    {1,_trxIcw,_trxWifi,_trx,0},                        // _trxLora
    {1,_trxLora,_trxIcw,_trx,0},                        // _trxWifi
    {1,_trxWifi,_trxLora,_trx,0},                       // _trxIcw
  #endif
#endif
#ifdef CONFIG_CW_GAME
  {0,_trx,_games,_dummy,0},                               // decoder  -e
  {0,_decode,_wifi,_dummy,_morseInvaders},                 // games
  {1,_memoryChain,_fightPileup,_games,0},                    // morse invaders  -e (left wraps via _memoryChain)
  {1,_morseInvaders,_radioCave,_games,0},                    // fight pileup  -e
  {1,_fightPileup,_morsel,_games,0},                         // radio cave  -e
  {1,_radioCave,_trailblazer,_games,0},                     // morsel  -e (right -> Trailblazer)
  {0,_games,_goToSleep,_dummy,_wifi_mac},                   // WiFi
#else
  {0,_trx,_wifi,_dummy,0},                                // decoder  -e
  {0,_decode,_goToSleep,_dummy,_wifi_mac},                 // WiFi
#endif
#ifdef CONFIG_PRACTICE_STATS
   {1,_wifi_stats,_wifi_config,_wifi,0},                 // 36 Disp Mac  -e!! (left now wraps via Practice Stats)
#else
   {1,_wifi_select,_wifi_config,_wifi,0},                // 36 Disp Mac  -e!!
#endif
  {1,_wifi_mac,_wifi_check,_wifi,0},                    // 37 Config Wifi    --NE!
#ifdef CONFIG_AUDIO_A11Y
  {1,_wifi_config,_wifi_select,_wifi,0},                // 38 Check WiFi  -e!! (right skips the two dropped entries)
#else
  {1,_wifi_config,_wifi_upload,_wifi,0},                // 38 Check WiFi  -e!!
  {1,_wifi_check,_wifi_update,_wifi,0},                 // 39 Upload File    --NE!
  {1,_wifi_upload,_wifi_select,_wifi,0},                // 40 Update Firmware  --NE!
#endif
#ifdef CONFIG_PRACTICE_STATS
  {1,_wifi_beforeSelect,_wifi_stats,_wifi,0},           // 41 Select network  --NE!! (next now wraps via Practice Stats)
  {1,_wifi_select,_wifi_mac,_wifi,0},                   // Practice Stats  (inserted after Select network, wraps to Disp Mac)
  {0,_wifi,_keyer,_dummy,0},                            // 42 goto sleep  -e
#else
  {1,_wifi_beforeSelect,_wifi_mac,_wifi,0},             // 41 Select network  --NE!!
  {0,_wifi,_keyer,_dummy,0},                            // 42 goto sleep  -e
#endif
#ifdef CONFIG_CW_GAME
  {1,_morsel,_foxHunt,_games,0},                        // Trailblazer  -e (in the games ring after Morsel)
  {1,_trailblazer,_memoryChain,_games,0},               // Fox Hunt  -e (right -> Memory Chain)
  {1,_foxHunt,_morseInvaders,_games,0},                 // Memory Chain  -e (right wraps to Morse Invaders)
#endif
  {1,_genPlayer,_genRand,_gen,0},                       // Practice Set (CW Generator) - appended at enum's end;
                                                         //   spliced into the gen ring via the _genPlayer/_genRand edits above
  {1,_echoPlayer,_echoRand,_echo,0},                    // Practice Set (Echo Trainer) - same splice, echo ring
  {1,_kochLearn,_kochGen,_koch,0}                       // Preview Char (Koch Trainer) - appended at enum's end for the
                                                         //   same reason (menuPtr persisted raw); spliced into the koch
                                                         //   ring via the _kochLearn/_kochGen edits above
};

//String MorseMenu::cmdPath;   // used to create string for json

#ifdef CONFIG_AUDIO_A11Y
// a11y: what was said last - the menu entry, and the parent it hung off (0xFF = nothing yet).
// menuDisplay() uses the pair to decide whether the announcement needs to name the whole path.
// Forgetting them forces the next announcement to name it - what you want after arriving from
// somewhere else (power-on, returning from a mode, leaving the preferences menu).
static uint8_t a11yMenuParent = 0xFF, a11yLastEntry = 0xFF;
static void a11yForgetMenuContext() { a11yMenuParent = a11yLastEntry = 0xFF; }
#endif

////// The MENU


void MorseMenu::menu_() {
   // Memory-clearing reboot is now reactive (see MorseGameMode::allocate)
   // rather than preemptive on WiFi Trx exit. The heap stays fragmented
   // after WiFi Trx, but that only matters if the user immediately starts
   // a game — in which case sprite allocation will fail and we reboot
   // then, resuming directly into the requested game.
   MorsePreferences::newMenuPtr = MorsePreferences::menuPtr;
   uint8_t disp = 0;
#ifdef CONFIG_BLE_SERIAL
   boolean bleSessionShown = bleProtocol;   // what the indicator currently claims; seeded from
                                            // reality so entering the menu paints it just once
#endif
   int t, command;
   m32state = menu_loop;
#ifdef CONFIG_AUDIO_A11Y
   a11yForgetMenuContext();     // arriving from a mode (or from power-on): announce the full path
#endif
   // Persist a volume changed in the mode we just left. The in-mode save only fires on
   // the vol-button toggle back to speed mode; every other exit (long-press to menu,
   // decoder mode, games, QSO bot) would otherwise lose the change on reboot.
   MorsePreferences::writeVolume();                     // no-op if unchanged

#ifdef LORA_RADIOLIB
    radio.standby();
#endif
    if (EspNowIsActive) {
      quickEspNow.stop();
      EspNowIsActive = false;
    }
    else {
      WiFi.disconnect(true, false);
    }
    //DEBUG("All WiFi Disconnected");
    delay(50);
    WiFi.mode(WIFI_OFF);
    //DEBUG("WiFi Mode OFF");
#ifdef CONFIG_BLUETOOTH_KEYBOARD
    if (MorseBluetooth::isBLErunning) {
      MorseOutput::clearDisplay();
      MorseOutput::printOnScroll(1, INVERSE_BOLD, 0,  "Stop BT Kbd");
      //MorseOutput::printOnScroll(2, REGULAR, 0, "Needs Reboot");
      if (protocolActive())
              MorseJSON::jsonCreate("message", "Stop BT Kbd", "");
      MorseOutput::refreshDisplay();
      delay (1400);
      MorseBluetooth::stopBluetooth();

      //DEBUG("Bluetooth Stopped");
      //ESP.restart(); // not needed anymore
    }
#endif
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

#ifdef CONFIG_TLV320AIC3100
    MorseOutput::soundSetVolume(MorsePreferences::sidetoneVolume);
#endif
    while (true) {                          // we wait for a click (= selection) or to get some serial input
#ifdef CONFIG_BLE_SERIAL
        // serialEvent(), plus the ONE place a BLE client may ask to be let in
        // (devdocs/ble-serial/ACCESS_CONTROL.md D2 — everywhere else refuses).
        // Returns true when the consent prompt took the screen, so repaint.
        if (bleTopMenuService())
            MorseMenu::menuDisplay(disp);
#else
        serialEvent();
#endif
#ifdef CONFIG_BLE_SERIAL
        // THE top-menu backstop (the only one): every path to the top menu
        // ends in this wait loop — menu_() falls straight through to here,
        // and _wifi_* functions / the selector changed (locally or via PUT
        // config) return here without re-entering menu_(). Cheap when
        // running (one flag test); a failed init latches and won't retry.
        if (MorsePreferences::pliste[posBluetoothOut].value == BLT_USE_SERIAL_PROT && !MorseBleSerial::isRunning)
          MorseBleSerial::init();
#endif
#ifdef CONFIG_AUDIO_A11Y
        MorseVoice::tick();                 // drive async menu announcements (non-blocking)
#endif


        if (disp != MorsePreferences::newMenuPtr) {
          disp = MorsePreferences::newMenuPtr;
          MorseMenu::menuDisplay(disp);
        }
#ifdef CONFIG_BLE_SERIAL
        // Repaint when a session comes or goes: menuDisplay() otherwise runs
        // only when the highlighted entry changes, and a session can open
        // (grace window) or drop while the menu sits perfectly still. Placed
        // AFTER the sync above so `disp` is never the stale 0 it starts as.
        if (bleProtocol != bleSessionShown) {
            bleSessionShown = bleProtocol;
            MorseMenu::menuDisplay(disp);
        }
#endif
        if (quickStart) {
            quickStart = false;
            command = 1;
            delay(250);
#ifdef CONFIG_TFT
            // After a memory-clearing reboot, auto-resume into the saved
            // menu item silently — the user just clicked it a second ago,
            // they don't need a "QUICK START" splash announcing the same
            // action. memoryReboot is one-shot: clear it after consuming.
            const bool announceQuickStart = !memoryReboot;
            memoryReboot = false;
#else
            const bool announceQuickStart = true;
#endif
            if (announceQuickStart) {
              if (protocolActive())
                MorseJSON::jsonCreate("message", "Quick Start", "");
              MorseOutput::printOnScroll(2, REGULAR, 1, "QUICK START");
              MorseOutput::refreshDisplay();
              delay(600);
              MorseOutput::clearDisplay();
            }
        }
        else if (executeMenu) {
            executeMenu = false;
            command = 1;
        }
        else {
            Buttons::modeButton.Update();
            command = Buttons::modeButton.clicks;
        }

#ifdef CONFIG_AUDIO_A11Y
        // Any button press means the user is done listening: cut whatever is still being
        // said, so what they do next is spoken straight away. This is what keeps the boot
        // splash (~10 s of speech) from standing between them and the device. A no-op when
        // nothing is playing, and the menu's own announcements are far too short to need it.
        if (command)
            MorseVoice::stop();
#endif
        switch (command) {                                          // actions based on encoder button
          case 2: // the top menu always offers ALL preferences. A cancelled mode start
                  // (menuExec() returning false after it already set its mode-specific
                  // options) would otherwise leave a stale subset in currentOptions.
                  MorsePreferences::setCurrentOptions(MorsePreferences::allOptions, MorsePreferences::allOptionsSize);
                  if (MorsePreferences::setupPreferences(MorsePreferences::newMenuPtr))                       // all available options when called from top menu
                    MorsePreferences::newMenuPtr = MorsePreferences::menuPtr;
#ifdef CONFIG_AUDIO_A11Y
                  a11yForgetMenuContext();      // back from the preferences menu: name the path again
#endif
                  MorseMenu::menuDisplay(MorsePreferences::newMenuPtr);
                  m32state = menu_loop;
                  break;
          case 1: // check if we have a submenu or if we execute the selection
                  if (menuNav[MorsePreferences::newMenuPtr][naviDown] == 0) {
                      MorsePreferences::menuPtr = MorsePreferences::newMenuPtr;
                      disp = 0;
                      // Remember last executed for Quick Start. The remembered set - the
                      // training modes and the classic games - all sit before _wifi in
                      // menuNo, so "< _wifi" captures them and automatically excludes every
                      // WiFi function, shutdown, and grid game (all >= _wifi), including any
                      // future append-only WiFi/game leaf (e.g. _wifi_stats) with no edit
                      // needed here. The leaves that must be remembered yet live *after* _wifi
                      // are the ones appended at the enum's end (menuPtr is persisted raw, so
                      // inserting mid-enum instead would shift every later index - see the
                      // Koch Trainer submenu wiring above), so they are named explicitly here:
                      // the two Practice Set pickers (found via on-device testing: Quick Start
                      // into "CW Generator: Practice Set" otherwise fell back to "Random") and
                      // Koch Trainer's Preview Char.
                      if (MorsePreferences::menuPtr < _wifi
                          || MorsePreferences::menuPtr == _genPractice
                          || MorsePreferences::menuPtr == _echoPractice
                          || MorsePreferences::menuPtr == _kochPreview)
                          MorsePreferences::writeLastExecuted(MorsePreferences::newMenuPtr);
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
#ifdef CONFIG_AUDIO_A11Y
          MorseVoice::stop();                    // navigating: drop leftover splash speech (see above)
#endif
          MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);         /// click
          MorsePreferences::newMenuPtr =  menuNav [MorsePreferences::newMenuPtr][(t == -1) ? naviLeft : naviRight];
          MorseMenu::menuDisplay(MorsePreferences::newMenuPtr);
          m32state = menu_loop;       
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

#ifdef CONFIG_MCP73871
       MorseOutput::checkPowerpathState();
       MorseOutput::updateBatteryDisplay();
#endif

checkShutDown(false);                  // check for time out
  } // end while - we leave as soon as the button has been pressed
  // MorseMenu::inMenuLoop = false;
} // end menu_()


void MorseMenu::menuDisplay(uint8_t ptr) {
  //DEBUG("Level: " + (String) menuNav [ptr][naviLevel] + " " + menuText[ptr]);
  uint8_t oneUp = menuNav[ptr][naviUp];
  uint8_t twoUp = menuNav[oneUp][naviUp];
  uint8_t oneDown = menuNav[ptr][naviDown];


  MorseOutput::clearStatusLine();
  MorseOutput::printOnStatusLine( true, 0,  "Select Mode:      ");
#ifdef CONFIG_BLE_SERIAL
  // A session opened over the air is the one thing about this device the
  // operator cannot otherwise see, so say so wherever they are looking. In a
  // running mode the same glyph rides in the top bar (updateTopLine()).
  if (bleProtocol)
      MorseOutput::dispBleLogo();
  else
      MorseOutput::clearBleLogo();     // never leave a finished session on screen
#endif

  // delete previous content
  MorseOutput::clearScrollLines();

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
#ifdef CONFIG_AUDIO_A11Y
  // a11y: speak the highlighted entry - preceded by the branch it hangs off whenever the
  // listener cannot already know that branch. A sighted user reads it off the two lines above
  // the highlighted entry; a blind one otherwise hears a bare "Call Signs" at power-on, with
  // no way to tell CW Generator from Echo Trainer. The path is named when the branch changed
  // AND we did not just descend into it from its own parent entry (that one was announced a
  // click ago), so: full path after switching on, after coming back from a mode or from the
  // preferences, and when stepping back up a level; terse "Call Signs" while scrolling
  // through a level, and terse "Random" right after clicking into "CW Generator".
  {
    const uint8_t level = menuNav[ptr][naviLevel];
    if (level > 0 && oneUp != a11yMenuParent && oneUp != a11yLastEntry) {
        MorseVoice::announce(menuText[level == 2 ? twoUp : oneUp]);
        if (level == 2)
            MorseVoice::announceMore(menuText[oneUp]);
        MorseVoice::announceMore(menuText[ptr]);
    }
    else
        MorseVoice::announce(menuText[ptr]);       // at level 0 the entry already IS the path
    a11yMenuParent = oneUp;
    a11yLastEntry  = ptr;
  }
#endif
  if (protocolActive()) {
      //cmdPath = MorseMenu::getMenuPath(ptr);
      MorseJSON::jsonMenu( MorseMenu::getMenuPath(ptr), (unsigned int) ptr, (m32state == menu_loop ? false : true), MorseMenu::isRemotelyExecutable(ptr));
  }

}

String MorseMenu::getMenuPath(uint8_t ptr) {
    uint8_t oneUp = menuNav[ptr][naviUp];
    uint8_t twoUp = menuNav[oneUp][naviUp];
    uint8_t oneDown = menuNav[ptr][naviDown];
    String cmdPath; cmdPath.reserve(80);

    switch (menuNav [ptr][naviLevel]) {
      case 2: cmdPath = String(menuText[twoUp]) + "/" + menuText[oneUp] + "/" + menuText[ptr];
              break;
      case 1: cmdPath = String(menuText[oneUp]) + "/" + menuText[ptr] + (oneDown ? "/.." : "");
              break;
      case 0: cmdPath = String(menuText[ptr]) + (oneDown ? "/.." : "");
              break;
  }
  return cmdPath;
}


boolean MorseMenu::menuExec() {       // return true if we should  leave menu after execution, false if we should stay in menu
#ifdef CONFIG_AUDIO_A11Y
  MorseVoice::stop();                 // silence any pending/playing announcement before the mode starts
#endif

  uint32_t wcount = 0;
//  String peer;
//  const char* peerHost;
  String s;

  if (protocolActive() && (MorsePreferences::menuPtr != _kochSel))
      MorseJSON::jsonActivate(ACT_ON);

  m32state = active_loop;

#ifdef CONFIG_PRACTICE_STATS
  MorsePracticeStats::endSegment();   // leaving whatever mode was active — flush it
#endif
  kochActive = false;
  MorsePreferences::usePracticeChars = false;
  keyerState = IDLE_STATE;

#ifdef CONFIG_MCP73871
    MorseOutput::clearBatteryIcon();
#endif

  switch (MorsePreferences::menuPtr) {
    case  _keyer:  /// keyer
                #ifdef CONFIG_BLUETOOTH_KEYBOARD
                  if (MorseBluetooth::keyboardMode() != 0) {
                    // Initialize Bluetooth System (keyboardMode() is 0 when the
                    // Bluetooth Use selector assigns BLE to the serial protocol)
                    MorseBluetooth::initializeBluetooth();
                  }
                #endif
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
     case _genPractice:  // "Practice Set" - plain RANDOMS generation, sourced from the device-picked
                          // practiceCharSet instead of CWchars; unrelated to Koch Trainer/kochActive.
                if (MorsePreferences::practiceCharSet.length() == 0) {
                    showStartDisplay("No Practice Set", "Pick chars in", "Settings menu", 900);
                    return false;
                }
                generatorMode = RANDOMS;
                MorsePreferences::usePracticeChars = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::generatorOptions, MorsePreferences::generatorOptionsSize);
                goto startGenerator;
     case _genPlayer:
                generatorMode = (GEN_TYPE) (MorsePreferences::menuPtr - 3);
                MorsePreferences::setCurrentOptions(MorsePreferences::playerOptions, MorsePreferences::playerOptionsSize);
                file = SPIFFS.open("/player.txt");
                MorsePreferences::scanFileParts();       // ADD: always scan on start
              
                // Multipart: let user select a chapter
                if (MorsePreferences::filePartCount >= 2) {
                    int8_t partChoice = MorseMenu::selectFilePart();
                    if (partChoice < 0) {
                        file.close();
                        return false;   // user cancelled
                    }
                    // Seek to start of selected part
                    file.seek(MorsePreferences::fileParts[partChoice].startOffset);
                    // Skip to saved word pointer for this part
                    wcount = MorsePreferences::fileParts[partChoice].wordPointer;
                    if (wcount > 0) wcount--;
                    MorsePreferences::fileWordPointer = 0;
                    skipWords(wcount);
                } else {
                    // Single file mode (as before)
                    wcount = MorsePreferences::fileWordPointer > 1 ? MorsePreferences::fileWordPointer - 1 : 0;
                    MorsePreferences::fileWordPointer = 0;
                    skipWords(wcount);
                }
     startGenerator:
                startFirst = true;
                firstTime = true;
                morseState = morseGenerator;
                showStartDisplay("Generator     ", "Start / Stop  ", "press Paddle  ", 1250);
                if (MorsePreferences::pliste[posLoraCwTransmit].value == 1)
                  {
                    if (MorsePreferences::useEspNow)
                      MorseMenu::setupESPNow();
                    else
                      if (!setupWifi())
                        return false;
                  }
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
            case  _echoPractice:  // "Practice Set" - see the CW Generator case for the rationale
                if (MorsePreferences::practiceCharSet.length() == 0) {
                    showStartDisplay("No Practice Set", "Pick chars in", "Settings menu", 900);
                    return false;
                }
                generatorMode = RANDOMS;
                MorsePreferences::usePracticeChars = true;
                MorsePreferences::setCurrentOptions(MorsePreferences::echoTrainerOptions, MorsePreferences::echoTrainerOptionsSize);
                goto startEcho;
            case  _echoPlayer:
                generatorMode = (GEN_TYPE) (MorsePreferences::menuPtr - 10);
                MorsePreferences::setCurrentOptions(MorsePreferences::echoPlayerOptions, MorsePreferences::echoPlayerOptionsSize);
                file = SPIFFS.open("/player.txt");
                MorsePreferences::scanFileParts();       // ADD: always scan on start
             
                if (MorsePreferences::filePartCount >= 2) {
                    int8_t partChoice = MorseMenu::selectFilePart();
                    if (partChoice < 0) {
                        file.close();
                        return false;
                    }
                    file.seek(MorsePreferences::fileParts[partChoice].startOffset);
                    wcount = MorsePreferences::fileParts[partChoice].wordPointer;
                    if (wcount > 0) wcount--;
                    MorsePreferences::fileWordPointer = 0;
                    skipWords(wcount);
                } else {
                    wcount = MorsePreferences::fileWordPointer > 1 ? MorsePreferences::fileWordPointer - 1 : 0;
                    MorsePreferences::fileWordPointer = 0;
                    skipWords(wcount);
                }
       startEcho:
                startFirst = true;
                morseState = echoTrainer;
                echoStop = false;
                showStartDisplay(
                    generatorMode == KOCH_LEARN   ? "New Character:" :
                    generatorMode == KOCH_PREVIEW ? "Preview Char:"  : "Echo Trainer:",
                    "Start:       ", "Press paddle ", 1250);
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
      case  _kochPreview: {  // Koch Preview Char - pick any character of the course (in lesson
                              // order) and repeat it, same playback as Koch Learn New.
                              // setCurrentOptions() goes first: the picker's own preferences
                              // double-click (see selectKochPreviewChar()) must not open whatever
                              // option set the previously active mode left behind.
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                int8_t previewIdx = MorseMenu::selectKochPreviewChar();
                if (previewIdx < 0) {
                    executeNow = false;
                    m32state = menu_loop;
                    return false;
                }
                MorsePreferences::previewCharIndex = (uint8_t) previewIdx;
                generatorMode = KOCH_PREVIEW;
                executeNow = false;
                goto startEcho;
      }
      case  _kochGenRand: // RANDOMS
                generatorMode = RANDOMS;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "listen");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochGenAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "listen");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochGenWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "listen");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochGenMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "listen");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochGenOptions, MorsePreferences::kochGenOptionsSize);
                goto startGenerator;
      case  _kochEchoRand: // Koch Echo Random
                generatorMode = RANDOMS;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "send");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "send");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "send");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "send");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);
                goto startEcho;
      case  _kochEchoAdaptive: // Koch Echo Adaptive - 6
                generatorMode = KOCH_ADAPTIVE;
                kochActive = true;
#ifdef CONFIG_PRACTICE_STATS
                MorsePracticeStats::beginSegment(MorsePreferences::kochFilter, "send");
#endif
                MorsePreferences::setCurrentOptions(MorsePreferences::kochEchoOptions, MorsePreferences::kochEchoOptionsSize);

                // re-run the setup, this will reset the character probabilities
                koch.setup();
                goto startEcho;
#ifndef LORA_DISABLED
      case  _trxLora: // LoRa Transceiver
                generatorMode = RANDOMS;  // to reset potential KOCH_LEARN
                MorsePreferences::setCurrentOptions(MorsePreferences::loraTrxOptions, MorsePreferences::loraTrxOptionsSize);
                morseState = loraTrx;
                showStartDisplay("", "Start LoRa Trx", "", 500);
                clearPaddleLatches();
                clearText = "";
#ifdef LORA_RADIOLIB
                radio.startReceive();
#endif
                executeNow = false;
                return true;
                break;
#endif
      case  _trxWifi: // Wifi Transceiver
                generatorMode = RANDOMS;  // to reset potential KOCH_LEARN
                MorsePreferences::setCurrentOptions(MorsePreferences::wifiTrxOptions, MorsePreferences::wifiTrxOptionsSize);
                morseState = wifiTrx;
                if (MorsePreferences::useEspNow) {
                    showStartDisplay("", "Start  Wifi Trx", "EspNow", 1500);
                    MorseMenu::setupESPNow();
                }
                else {
                    MorseOutput::clearDisplay();
                    MorseOutput::printOnScroll(0, REGULAR, 0, "Connecting...");
                    if (protocolActive())
                      MorseJSON::jsonCreate("message", "Connecting...", "");

                    if (!setupWifi())
                      return false;
                    //DEBUG("Peer IP: " + peerIP.toString());
                    s = peerIP.toString();
                    showStartDisplay("", "Start Wifi Trx", s  == "255.255.255.255" ?"IP Broadcast" : s, 1500);

                    MorseWiFi::audp.listen(MORSERINOPORT); // listen on port 7373
                    MorseWiFi::audp.onPacket(onWifiReceive);
                }
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
                if (protocolActive())
                  MorseJSON::jsonCreate("message", "Start CW Transceiver", "");
                clearPaddleLatches();
                goto setupDecoder;
#ifdef CONFIG_QSO_BOT
      // QSO Bot — simulated CW QSO partner. Uses the same safety story as
      // the games (morseQsoBot is local-sidetone-only; never appears in
      // the LoRa/WiFi/external-TX gates in m32_v6.ino, so RF stays off).
      case _qsoSotaPota:
      case _qsoStandard:
      case _qsoContest:
                MorsePreferences::setCurrentOptions(MorsePreferences::qsoBotOptions,
                                                    MorsePreferences::qsoBotOptionsSize);
                morseState = morseQsoBot;
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                MorseQsoBot::run((menuNo) MorsePreferences::menuPtr);
                m32state = menu_loop;
                return false;
#endif
#ifdef CONFIG_CW_GAME
      // Games manage their own CW and must not inherit a transmit mode
      // (loraTrx/wifiTrx) from a previous menu session, or the shared keyer
      // (doPaddleIambic) would try to send keyed characters over
      // LoRa/WiFi/ESP-NOW — which, after that subsystem was torn down on
      // menu entry, dereferences freed state and crashes. The dedicated
      // morseGame state gives local-sidetone-only keying: the keyer's
      // transmit gates (== loraTrx/wifiTrx) and keyOut()'s external-TX
      // gates (== morseKeyer/morseTrx/morseGenerator) all exclude it, so a
      // game never transmits nor keys a real rig. Reset by the next menu mode.
      case _morseInvaders:
                morseState = morseGame;
                MorseGame::run();
                m32state = menu_loop;
                // Clear both buttons: a long-press exit leaves clicks == -1, which
                // would trigger an immediate exit in the next game's lobby guard.
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                return false;
      case _fightPileup:
                morseState = morseGame;
                MorsePileup::run();
                m32state = menu_loop;
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                return false;
      case _radioCave:
                morseState = morseGame;
                MorseRadioCave::run();
                m32state = menu_loop;
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                return false;
      case _morsel:
                morseState = morseGame;
                MorseMorsel::run();
                m32state = menu_loop;
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                return false;
      case _trailblazer:
                morseState = morseGame;
                MorseTrailblazer::run();
                m32state = menu_loop;
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                return false;
      case _foxHunt:
                morseState = morseGame;
                MorseFoxHunt::run();
                m32state = menu_loop;
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                return false;
      case _memoryChain:
                morseState = morseGame;
                MorseMemoryChain::run();
                m32state = menu_loop;
                Buttons::modeButton.clicks = 0;
                Buttons::volButton.clicks  = 0;
                return false;
#endif
      case  _decode: /// decoder
                MorsePreferences::setCurrentOptions(MorsePreferences::decoderOptions, MorsePreferences::decoderOptionsSize);
                morseState = morseDecoder;
                  /// here we will do the init for decoder mode
                encoderState = volumeSettingMode;
                MorseOutput::clearDisplay();
                MorseOutput::printOnScroll(1, REGULAR, 0, "Start Decoder" );
                if (protocolActive())
                  MorseJSON::jsonCreate("message", "Start Decoder", "");
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
#ifndef CONFIG_AUDIO_A11Y
      case _wifi_upload:
      case _wifi_update:
#endif
#ifdef CONFIG_PRACTICE_STATS
      case _wifi_stats:
#endif
                  // uniform for ALL _wifi_* functions (even display-only _wifi_mac):
                  // one rule, no per-function special case to drift
                  suspendBleSerialForWifi();
                  MorseWiFi::menuExec((uint8_t) MorsePreferences::menuPtr);
                  break;
      case _wifi_select:
                  suspendBleSerialForWifi();
                  MorseWiFi::menuNetSelect();
                  break;
      case  _goToSleep: /// deep sleep
                checkShutDown(true);
      default:  break;
  }
  m32state = menu_loop;
  return false;
}   /// end menuExec()

#ifdef CONFIG_BLE_SERIAL
// whenever any code path brings up the WiFi radio, BLE Serial is suspended
// for the remainder of the session (PLAN D8; the wait-loop backstop restarts
// it at the top menu). The radio primitives in MorseWiFi carry the same call
// as defense in depth — these menu-level hooks are what the manuals promise
// (uniform for every _wifi_* function, including display-only ones).
static void suspendBleSerialForWifi() { MorseBleSerial::suspendForWifi(); }
#else
static inline void suspendBleSerialForWifi() {}
#endif

boolean MorseMenu::setupWifi() {
  String peer;
  peer.reserve(24);
  const char* peerHost;

  suspendBleSerialForWifi();

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

void MorseMenu::wifiWarmup() {
  // Force ESP-IDF WiFi+ESP-NOW lazy allocations to happen now. WiFi.mode(STA)
  // allocates ~51 KB of which ~36 KB is freed by WiFi.mode(OFF); the
  // remaining ~16 KB stays allocated for the device's lifetime. Doing this
  // cycle at boot makes that 16 KB land predictably here rather than in
  // the middle of game-allocated memory later.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  quickEspNow.begin(MorsePreferences::pliste[posLoraChannel].value ? ESPNOW_CH_ALT : ESPNOW_CH);
  delay(50);
  quickEspNow.stop();
  WiFi.mode(WIFI_OFF);
}

void MorseMenu::setupESPNow() {
  // init wifi for espnow
      suspendBleSerialForWifi();
      WiFi.mode(WIFI_STA);
      WiFi.disconnect (false, true);
      EspNowIsActive = true;
      quickEspNow.onDataRcvd (onEspnowRecv);
      quickEspNow.begin (MorsePreferences::pliste[posLoraChannel].value ? ESPNOW_CH_ALT : ESPNOW_CH); // If you don't use an AP channel needs to be specified
      delay(100);
      //DEBUG("setupESPNow has been performed.");
  }
  
void MorseMenu::cleanupScreen() {
    MorseOutput::clearDisplay();
    updateTopLine();
    MorseOutput::clearScroll();      // clear the buffer}
}

void MorseMenu::showStartDisplay(const String& l0, const String& l1, const String& l2, int pause) {
    MorseOutput::clearDisplay();
    if (l0.length())
        MorseOutput::printOnScroll(0, REGULAR, 0, l0);
    if (l1.length())
        MorseOutput::printOnScroll(1, REGULAR, 0, l1);
    if (l2.length())
        MorseOutput::printOnScroll(2, REGULAR, 0, l2);
    if (protocolActive())
        MorseJSON::jsonCreate("message", l0  + l1 + l2, "");
    delay(pause);
    cleanupScreen();
}

boolean MorseMenu::isRemotelyExecutable(uint8_t ptr) {
  if (menuNav[ptr][naviDown] == 0) {
    switch (ptr) {
      case _kochPreview:      // its picker (selectKochPreviewChar()) has no remote-selection path
                              // (unlike selectFilePart()'s remoteFilePartSelect) - a protocol client
                              // executing it would strand the device in a picker only someone
                              // standing at it can dismiss
      case _wifi_config:      // these WiFi functions cannot be executed remotely
#ifndef CONFIG_AUDIO_A11Y
      case _wifi_upload:
      case _wifi_update:
#endif
      case _wifi_select:
#ifdef CONFIG_PRACTICE_STATS
      case _wifi_stats:
#endif
#ifdef CONFIG_CW_GAME
      case _games:            // nor the games that depend on visual clues
      case _morseInvaders:
      case _fightPileup:
      case _radioCave:
      case _morsel:
      case _trailblazer:
      case _foxHunt:
      case _memoryChain:
#endif
      return false;
    }
    return true;
  }
  else
    return false;
}
// This function is called from the file player mode when a multipart file is selected. 
// When the file player is selected and the file is multipart, show a
// scrollable list of part names. The user selects with encoder + click.
//
// Returns the selected part index (0-based), or -1 if cancelled.
// This follows the same pattern as the existing menu navigation.
int8_t MorseMenu::selectFilePart() {
    if (MorsePreferences::filePartCount < 2)
        return 0;
 
    int8_t selected = MorsePreferences::filePartSelected;
    int8_t count = MorsePreferences::filePartCount;
 
    MorseOutput::clearDisplay();
    MorseOutput::printOnStatusLine(true, 0, "Select Chapter:   ");
 
    int8_t lastDisplayed = -1;
    remoteFilePartSelect = -1;   // reset remote selection flag
 
    while (true) {
        // Check for remote selection via serial protocol
        if (remoteFilePartSelect >= 0) {
            selected = remoteFilePartSelect;
            remoteFilePartSelect = -1;
            MorsePreferences::filePartSelected = selected;
            MorsePreferences::writeFilePartData();
            return selected;
        }
 
        // Display current selection and report to serial client
        if (selected != lastDisplayed) {
            lastDisplayed = selected;
            MorseOutput::clearScrollLines();
 
            if (selected > 0)
                MorseOutput::printOnScroll(0, REGULAR, 0,
                    String(MorsePreferences::fileParts[selected - 1].name));
 
            MorseOutput::printOnScroll(1, BOLD, 0,
                String(MorsePreferences::fileParts[selected].name));
 
            if (selected < count - 1)
                MorseOutput::printOnScroll(2, REGULAR, 0,
                    String(MorsePreferences::fileParts[selected + 1].name));
 
            // Report to serial client (like menuDisplay does)
            if (protocolActive()) {
                MorseJSON::jsonFilePart(
                    String(MorsePreferences::fileParts[selected].name),
                    selected,
                    count);
            }
        }
 
        // Check encoder
        int t = checkEncoder();
        if (t) {
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            selected += t;
            if (selected < 0) selected = 0;
            if (selected >= count) selected = count - 1;
        }
 
        // Check buttons
        Buttons::modeButton.Update();
        switch (Buttons::modeButton.clicks) {
            case 1:
                MorsePreferences::filePartSelected = selected;
                MorsePreferences::writeFilePartData();
                if (protocolActive())
                    MorseJSON::jsonActivate(ACT_SET);
                return selected;
            case -1:
                if (protocolActive())
                    MorseJSON::jsonActivate(ACT_CANCELLED);
                return -1;
        }
 
        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == -1) {
            if (protocolActive())
                MorseJSON::jsonActivate(ACT_CANCELLED);
            return -1;
        }

        checkShutDown(false);
        serialEvent();
    }
}

// Koch Trainer "Preview Char": let the user pick any character of the active Koch
// sequence (built-in or Custom Koch set - Koch::getKochChar() handles both), not just
// the already-unlocked ones - the full course, so you can preview ahead too. The list
// is shown in lesson order (position 0 = first learned), which doubles as the order
// these characters get unlocked in. Same encoder/click/long-press pattern as
// selectFilePart() above - deliberately reused rather than inventing a new interaction
// (see UX_CONVENTIONS.md: no mode may redefine a global gesture); the visible window
// scales with NoOfVisibleLines (hard rule: never hardcode the line count) instead of
// the fixed 3 lines selectFilePart() uses, since the Pocket has room for more.
//
// Returns the picked 0-based index into the active sequence, or -1 if cancelled.
int8_t MorseMenu::selectKochPreviewChar() {
    uint8_t count = MorsePreferences::kochMaximum;      // # of characters in the whole course

    if (count < 2) {                                    // nothing to pick between
        MorsePreferences::previewCharIndex = 0;
        return 0;
    }

    int8_t selected = MorsePreferences::previewCharIndex;
    if (selected < 0 || selected >= count) {
        selected = MorsePreferences::kochFilter - 1;      // default: the current lesson's char
        if (selected < 0) selected = 0;                   // kochFilter == 0 edge: don't hand back -1 (= "cancelled")
    }

    MorseOutput::clearDisplay();
    MorseOutput::printOnStatusLine(true, 0, "Preview Char:    ");

    int8_t lastDisplayed = -1;
    boolean a11ySaidTotal = false;      // see the announcement below
    uint8_t rows = (NoOfVisibleLines < count) ? NoOfVisibleLines : count;

    while (true) {
        if (selected != lastDisplayed) {
            lastDisplayed = selected;
            MorseOutput::clearScrollLines();

            // Window of `rows` consecutive lessons, current one kept roughly centered
            // (clamped at both ends of the full [0, count) range).
            int16_t windowStart = (int16_t) selected - rows / 2;
            int16_t maxStart = (int16_t) count - rows;
            if (windowStart > maxStart) windowStart = maxStart;
            if (windowStart < 0) windowStart = 0;

            String curLine, curRawChar;
            for (uint8_t row = 0; row < rows; ++row) {
                uint8_t idx = (uint8_t) windowStart + row;
                String rawChar = koch.getKochChar(idx);
                // Display gets the cleaned-up glyph (hard rule 4): CWchars reuses plain
                // letters as single-char prosign codes (a/n/k/s/e/b), so a raw prosign
                // position would draw as a bare uppercase letter indistinguishable from
                // the plain-letter lesson elsewhere in the course. Same technique as
                // practiceGlyph() in MorsePreferences.cpp.
                // NB cleanUpProSigns() takes a String& and writes the expansion back into
                // its argument, so it has to work on a copy - cleaning rawChar in place would
                // leave the announcement below with "<ka>" where it needs the raw "K".
                String cleanChar = rawChar;
                cleanUpProSigns(cleanChar);
                String line = String(idx + 1) + ": " + cleanChar;
                // BOLD alone is too close to REGULAR to pick the highlighted row out at a
                // glance - markedly so on the Pocket's TFT. So the selected row also carries
                // the cursor idiom the preference pickers already use (MorsePreferences.cpp):
                // an INVERSE_BOLD ">" in column 0, with every row's text shifted to column 1
                // so the list stays aligned whether or not it is the selected one.
                MorseOutput::printOnScroll(row, idx == selected ? BOLD : REGULAR, 1, line);
                if (idx == selected)
                    MorseOutput::printOnScroll(row, INVERSE_BOLD, 0, ">");
                if (idx == selected) { curLine = line; curRawChar = rawChar; }
            }

            if (protocolActive())
                MorseJSON::jsonCreate("message", "Preview character: " + curLine, "");

            // Accessibility Edition: "21 of 51, Yankee" - same shape as posKochFilter's
            // announcement (MorsePreferences.cpp). announceMoreChar() wants the RAW
            // character (pre-cleanUpProSigns) so prosign clips match; announce()/
            // announceMore() are no-ops without CONFIG_AUDIO_A11Y, so this call is
            // unconditional.
            // The total is spoken on entry only: it does not change while the encoder turns,
            // and repeating it on all 51 detents makes the list far slower to scan by ear
            // (the same reasoning announceValue() records for posKochFilter).
            MorseVoice::announce(String(selected + 1));
            if (!a11ySaidTotal) {
                MorseVoice::announceMore("of");
                MorseVoice::announceMore(String(count));
                a11ySaidTotal = true;
            }
            MorseVoice::announceMoreChar(curRawChar);
        }

        int t = checkEncoder();
        if (t) {
            MorseOutput::pwmClick(MorsePreferences::sidetoneVolume);
            selected += t;
            if (selected < 0) selected = 0;
            if (selected >= count) selected = count - 1;
        }

        Buttons::modeButton.Update();
        switch (Buttons::modeButton.clicks) {
            case 1:
                MorsePreferences::previewCharIndex = selected;
                if (protocolActive())
                    MorseJSON::jsonActivate(ACT_SET);
                return selected;
            case -1:
                if (protocolActive())
                    MorseJSON::jsonActivate(ACT_CANCELLED);
                return -1;
            case 2:   // double click: preferences menu, same as everywhere else (m32_v6.ino loop())
                MorsePreferences::setupPreferences(MorsePreferences::menuPtr);
                MorseOutput::clearDisplay();
                MorseOutput::printOnStatusLine(true, 0, "Preview Char:    ");
                // Koch Sequence / Custom Chars may have just changed in there, which
                // changes kochMaximum - re-read and re-clamp instead of trusting the
                // bounds read at entry.
                count = MorsePreferences::kochMaximum;
                if (count < 2) {
                    MorsePreferences::previewCharIndex = 0;
                    return 0;
                }
                if (selected >= count) selected = count - 1;
                rows = (NoOfVisibleLines < count) ? NoOfVisibleLines : count;
                lastDisplayed = -1;                  // force the list to redraw below
                break;
        }

        Buttons::volButton.Update();
        switch (Buttons::volButton.clicks) {
            case -1:
                if (protocolActive())
                    MorseJSON::jsonActivate(ACT_CANCELLED);
                return -1;
            case 2:   // double click: step display brightness, same as everywhere else
                MorseOutput::decreaseBrightness();
                break;
        }

#ifdef CONFIG_AUDIO_A11Y
        MorseVoice::tick();                 // this loop blocks outside loop(), and tick() is what
                                            // actually starts and advances the clips - the same
                                            // reason setupPreferences() drives it
#endif
        checkShutDown(false);
        serialEvent();
    }
}