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
#include "morsedefs.h"
#include "goertzel.h"
#include "MorseDecoder.h"
#include "MorseOutput.h"

extern unsigned long genTimer;      /// needed for echo trainer 
extern morserinoMode morseState;
extern int leftPin;

//////// methods for class Decoder /////////

Decoder::Decoder(boolean key) {
   fromKey = key;                // this instance should be audio or key?
   setup();
   MorseTable myTable;
}

void Decoder::setup() {
  /// set up variables for  Morse Decoder
  if (!fromKey)
      Goertzel::setup();
  filteredState = filteredStateBefore = realstate = realstatebefore = false;
  lastStartTime = 0;
  decoderState = LOW_;
  ditAvg = 60;
  dahAvg = 180;
  d_wpm = 15;
  nbtime = 1;
}


#define straightPin leftPin


boolean Decoder::checkInput() {         /// check if we have a tone signal at A6 with Gortzel's algorithm or from a key (fromKey == true), and apply some noise blanking as well
                                        /// we return true when we detected a change in state, false otherwise!
  
///// check straight key first before you check audio in.... (unless we are in transceiver mode)
///// straight key is connected to external paddle connector (tip), i.e. the same as the left pin (dit normally)

if (fromKey) {
// we also check the paddles - you can use them like a cootie key / sideswiper
//    if (MorsePreferences::keyermode == STRAIGHTKEY)          // we only check the "tip" of the external straight key jack
//        realstate =  !digitalRead(straightPin);
//    else
        realstate =  ((!digitalRead(straightPin)) || leftKey || rightKey) ; // we also check the paddles (also the capacitive ones)
}
else 
    realstate = Goertzel::checkInput();


  //////////////////////////////////////////////////////////////////
  // here we clean up the state with a noise blanker (debouncing) //
  //////////////////////////////////////////////////////////////////

  if (realstate != realstatebefore) {
    lastStartTime = millis();
}
  
  if ((millis() - lastStartTime) > nbtime) {
    if (realstate != filteredState) {
      filteredState = realstate;
    }
  }
  realstatebefore = realstate;
 
 if (filteredState == filteredStateBefore) {
  //DEBUG("checkInput returns false");
  return false; 
 }// no change detected in filteredState
 else {
    filteredStateBefore = filteredState;
    //DEBUG("checkInput returns true");
    return true;                                // change detected in filteredState
 }
}   /// end checkInput()


void Decoder::decode() {             
  float lacktime;
  int wpm;

    switch(decoderState) {
      case INTERELEMENT_: if (checkInput()) {
                              ON_();
                              decoderState = HIGH_;
                          } else {
                              lowDuration = millis() - startTimeLow;                        // we record the length of the pause
                              lacktime = 2.2;                                               ///  when high speeds we have to have a little more pause before new letter 
                              if (d_wpm > 35) lacktime = 2.4;
                                else if (d_wpm > 30) lacktime = 2.3;
                              if (lowDuration > (lacktime * ditAvg)) {
                                displayMorse(myTable.retrieveSymbol(), !fromKey);       //displayMorse(!fromKey);    /////////////////////////!!! decode the morse character and display it
                                wpm = (d_wpm + (int) (7200 / (dahAvg + 3*ditAvg))) / 2;     //// recalculate speed in wpm
                                if (d_wpm != wpm) {
                                  d_wpm = wpm;
                                  //DEBUG("d_wpm1: " + String(d_wpm));
                                  lastWasKey = fromKey;
                                  speedChanged = true;
                                }
                                if (morseState == loraTrx || morseState == wifiTrx)
                                      cwForTx(0);
                                decoderState = INTERCHAR_;
                              }
                          }
                          break;
      case INTERCHAR_:    if (checkInput()) {
                              ON_();
                              decoderState = HIGH_;
                          } else {
                              lowDuration = millis() - startTimeLow;             // we record the length of the pause
                              lacktime = 5;                                   ///  when high speeds we have to have a little more pause before new word
                              if (d_wpm > 35) lacktime = 6;
                                else if (d_wpm > 30) lacktime = 5.5;
                                else if (morseState == echoTrainer) lacktime = MorsePreferences::interWordSpace + 1;
                              if (lowDuration > (lacktime * ditAvg)) {
                                   displayMorse(myTable.retrieveSymbol(), !fromKey);            ////////end of word////////// !!!!!   
                                   decoderState = LOW_;
                                   if (morseState == echoTrainer && echoTrainerState == COMPLETE_ANSWER)   {       // change the state of the trainer at end of word in case of echo trainer
                                       echoTrainerState = EVAL_ANSWER;
                                   }
                                   if (morseState == loraTrx || morseState == wifiTrx)    {                    // when in Trx mode
                                       cwForTx(3);
                                       if (morseState == loraTrx)
                                            sendWithLora();                        // finalise the string and send it to LoRA
                                       else
                                            sendWithWifi();
                                   }
                              }
                          }
                          break;
      case LOW_:          if (checkInput()) {
                              ON_();
                              decoderState = HIGH_;
                              if (morseState == echoTrainer)   {      // change the state of the trainer at end of word
                                  echoTrainerState = COMPLETE_ANSWER;
                              }
                          }
                          else {
                            if (echoTrainerState == GET_ANSWER && millis() > genTimer) {
                              echoTrainerState = EVAL_ANSWER;
                            }
                          }
                          break;
      case HIGH_:         if (checkInput()) {
                              OFF_();
                              decoderState = INTERELEMENT_;
                          }
                          break;
    } 
}


void Decoder::ON_() {                         /// what we do when we just detected a rising flank, from low to high
  unsigned long timeNow = millis();
  lowDuration = timeNow - startTimeLow;             // we record the length of the pause
  startTimeHigh = timeNow;                          // prime the timer for the high state
  
  unsigned int pitch = MorseOutput::notes[MorsePreferences::sidetoneFreq];
        if ((morseState == echoTrainer || morseState == loraTrx || morseState == wifiTrx) && MorsePreferences::echoToneShift != 0) {
           pitch = (MorsePreferences::echoToneShift == 1 ? pitch * 18 / 17 : pitch * 17 / 18);        /// one half tone higher or lower, as set in parameters in echo trainer mode
        }
  keyOut(true, fromKey, pitch, MorsePreferences::sidetoneVolume);   
  MorseOutput::drawInputStatus(true);
  
  if (lowDuration < ditAvg * 2.4)                    // if we had an inter-element pause,
    recalculateDit(lowDuration);                    // use it to adjust speed
}

void Decoder::OFF_() {                                 /// what we do when we just detected a falling flank, from high to low
  unsigned long timeNow = millis();
  unsigned int threshold = (int) ( ditAvg * sqrt( dahAvg / ditAvg));

  highDuration = timeNow - startTimeHigh;
  startTimeLow = timeNow;

  if (highDuration > (ditAvg * 0.5) && highDuration < (dahAvg * 2.5)) {    /// filter out VERY short and VERY long highs
      if (highDuration < threshold) { /// we got a dit -
            myTable.recordDit();
            //treeptr = CWtree[treeptr].dit;
            recalculateDit(highDuration);
            if (morseState == loraTrx || morseState == wifiTrx)
                cwForTx(1);
      }
      else  {        /// we got a dah
            myTable.recordDah();
            //treeptr = CWtree[treeptr].dah;   
            recalculateDah(highDuration);
            if (morseState == loraTrx || morseState == wifiTrx)
                cwForTx(2);                 
      }
  }
  keyOut(false, fromKey, 0, 0);
  MorseOutput::drawInputStatus(false);
}


void Decoder::recalculateDit(unsigned long duration) {       /// recalculate the average dit length
  ditAvg = (4*ditAvg + duration) / 5;
  nbtime = constrain(ditAvg/5, 7, 20);
}

void Decoder::recalculateDah(unsigned long duration) {       /// recalculate the average dah length
  if (duration > 2* dahAvg)   {                       /// very rapid decrease in speed!
      dahAvg = (dahAvg + 2* duration) / 3;            /// we adjust faster, ditAvg as well!
      ditAvg = ditAvg/2 + dahAvg/6;
  }
  else { 
      dahAvg = (3* ditAvg + dahAvg + duration) / 3;
  }
}

uint8_t Decoder::getWpm() {
  return d_wpm;
}


//////// methods for class MorseTable /////////

MorseTable::MorseTable()
{
  treeptr = 0;
}

void MorseTable::recordDit() {      /// walk down the dit branch
  treeptr = CWtree[treeptr].dit;
}

void MorseTable::recordDah() {      /// walk down the dah branch
  treeptr = CWtree[treeptr].dah;
}

void MorseTable::resetTable() {     /// go back to the root
  treeptr = 0;
}

String MorseTable::retrieveSymbol() { /// deliver the character, and go backk to the root
  String symbol;
  symbol.reserve(6);
  if (treeptr == 0 )  
    return String(" ");     /// we display a blank
  symbol = CWtree[treeptr].symb;
  treeptr = 0;
  return symbol;
}
