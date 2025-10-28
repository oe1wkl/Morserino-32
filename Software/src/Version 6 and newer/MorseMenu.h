#ifndef MORSEMENU_H_
#define MORSEMENU_H_

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
#include "morsedefs.h"
#include "MorsePreferences.h"
#include "MorseWiFi.h"
#include "MorseDecoder.h"

extern boolean m32protocol;
extern boolean genIsActive, echoStop, firstTime, startFirst;
extern boolean kochActive;
extern boolean filteredState, filteredStateBefore, speedChanged;
extern boolean quickStart;
extern unsigned long ditAvg, dahAvg;
extern String clearText;
extern File file;
extern IPAddress peerIP;
extern morserinoMode morseState;
extern loops m32state;
extern boolean executeMenu;
extern boolean executeNow;
extern encoderMode encoderState;
extern DECODER_STATES decoderState;
extern GEN_TYPE generatorMode;
extern KEYERSTATES keyerState;
extern Decoder keyDecoder;
extern Decoder audioDecoder;


extern void keyOut(boolean,  boolean, int, int);
extern void cleanStartSettings();
extern void checkShutDown(boolean );
extern void updateTopLine();
extern void clearPaddleLatches();
extern void displayCWspeed();
extern void skipWords(uint32_t );
extern void audioLevelAdjust();
extern void onWifiReceive(AsyncUDPPacket);
extern void onEspnowRecv(const uint8_t*, const uint8_t*, uint8_t, signed int, boolean);
extern void serialEvent();
extern void jsonCreate(String,String,String);
extern void jsonMenu(String,unsigned int,bool,bool);
extern void jsonActivate(actMessage);

namespace MorseMenu
{
void menu_();
boolean menuExec();
boolean setupWifi();
void setupESPNow();
String getMenuPath(uint8_t);
void menuDisplay(uint8_t);
void cleanupScreen();
void showStartDisplay(String, String, String, int);
boolean isRemotelyExecutable(uint8_t);
}

#endif /* #ifndef MORSEMENU_H_ */