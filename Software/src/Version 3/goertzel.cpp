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

 //// Checking audio signal with Goertzel filter - based on code by Hjalmar Skovholm Hansen OZ1JHM
 ////                                             - see http://skovholm.com/cwdecoder

///////////////////////////////////////////////////////////
// The sampling frq will be 106.000 on ESp32             //
// because we need the tone in the center of the bins    //
// I set the tone to 698 Hz                              //
// then n the number of samples which give the bandwidth //
// can be (106000 / tone) * 1 or 2 or 3 or 4 etc         //
// init is 106000/698 = 152 *4 = 608 samples             //
// 152 will give you a bandwidth around 700 hz           //
// 304 will give you a bandwidth around 350 hz           //
// 608 will give you a bandwidth around 175 hz           //
///////////////////////////////////////////////////////////

#include "goertzel.h"

using namespace Goertzel;

  float coeff;
  float sine;
  float cosine;
  const float sampling_freq = 106000.0;
  const float target_freq = 698.0; /// adjust for your needs see above
  int goertzel_n = 152;   //// you can use:         152, 304, 456 or 608 - thats the max buffer reserved in checktone()
                          ///// resulting bandwidth: 700, 350, 233 or 175 Hz, respectively
  float bw;
  float Q1 = 0;
  float Q2 = 0;
  uint32_t magnitudelimit;                                   // magnitudelimit_low = ( p_goertzelBandwidth? 80000 : 30000);
  uint32_t magnitudelimit_low;                               // magnitudelimit = magnitudelimit_low;

//const float target_freq = 698.0; /// adjust for your needs see above
//const int goertzel_n = 304; //// you can use:         152, 304, 456 or 608 - thats the max buffer reserved in checktone()//


void Goertzel::setup() {                 /// pre-compute some values (sine, cosine, coeff, magnitudelimit) that are compute-imntensive and won't change anyway
  goertzel_n = ( MorsePreferences::goertzelBandwidth == 0 ? 152 : 608);                 // update Goertzel parameters depending on chosen bandwidth
  magnitudelimit_low = ( MorsePreferences::goertzelBandwidth? 160000 : 40000);          // values found by experimenting
  magnitudelimit = magnitudelimit_low;
  bw = (sampling_freq / goertzel_n ); //348
  int  k;
  float omega;
  k = (int) (0.5 + ((goertzel_n * target_freq) / sampling_freq)); // 2
  omega = (2.0 * PI * k) / goertzel_n ;                           //0,041314579
  sine = sin(omega);                                              // 0,007210753
  cosine = cos(omega);                                            // 0,999999739
  coeff = 2.0 * cosine;                                           // 1,999999479
  }

boolean Goertzel::checkInput() {                  // check the audio input for a signal
    float magnitude ;
    uint16_t testData[1216];         /// buffer for freq analysis - max. 608 samples; you could increase this (and n) to a max of 1216, for sample time 10 ms, and bw 88 Hz
 

    for (int index = 0; index < goertzel_n ; index++)
            testData[index] = analogRead(audioInPin);
    //DEBUG("Read and stored analog values!");
    for (int index = 0; index < goertzel_n ; index++) {
      float Q0;
      Q0 = coeff * Q1 - Q2 + (float) testData[index];
      Q2 = Q1;
      Q1 = Q0;
    }
    //DEBUG("Calculated Q1 and Q2!");

    float magnitudeSquared = (Q1 * Q1) + (Q2 * Q2) - (Q1 * Q2 * coeff); // we do only need the real part //
    magnitude = sqrt(magnitudeSquared);
    Q2 = 0;
    Q1 = 0;
   //DEBUG("Magnitude: " + String(magnitude) + " Limit: " + String(magnitudelimit));   //// here you can measure magnitude for setup..
    ///////////////////////////////////////////////////////////
    // here we will try to set the magnitude limit automatic //
    ///////////////////////////////////////////////////////////
  
    if (magnitude > magnitudelimit_low) {
      magnitudelimit = (magnitudelimit + ((magnitude - magnitudelimit) / 6)); /// moving average filter
    }
  
    if (magnitudelimit < magnitudelimit_low)
      magnitudelimit = magnitudelimit_low;
    
    ////////////////////////////////////
    // now we check for the magnitude //
    // and return true or false       //
    ////////////////////////////////////
  
    if (magnitude > magnitudelimit * 0.6) // just to have some space up
      return true;
    else
      return false;
}
