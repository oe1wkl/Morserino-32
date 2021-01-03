#ifndef MORSEMENU_H_
#define MORSEMENU_H_

#include <Arduino.h>
#include "morsedefs.h"
#include "MorsePreferences.h"
#include "MorseWiFi.h"
#include "MorseDecoder.h"


extern boolean active, echoStop, firstTime, startFirst;
extern boolean kochActive;
extern boolean filteredState, filteredStateBefore, speedChanged;
extern boolean quickStart;
extern unsigned long ditAvg, dahAvg;
extern String clearText;
extern File file;
extern IPAddress peerIP;
extern morserinoMode morseState;
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

namespace MorseMenu
{
void menu_();
boolean menuExec();
boolean setupWifi();
void menuDisplay(uint8_t ptr);
void cleanupScreen();
void showStartDisplay(String, String, String, int);
}

#endif /* MORSEMENU_H_ */
