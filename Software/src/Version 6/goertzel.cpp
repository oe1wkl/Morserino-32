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

 //// Checking audio signal with Goertzel filter - based on code by Hjalmar Skovholm Hansen OZ1JHM
 ////                                             - see http://skovholm.com/cwdecoder
 //// see also: https://www.embedded.com/the-goertzel-algorithm

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
//                                                       //
// Changes as of version 7 (was wrong in V 6):           //
//  - the sampling frequency is now 11905 Hz,            //
//    and the tone is at 698 Hz                          //
//  - the number of samples is now 17, 34, 51, or 68,    //
// which gives you a bandwidth of 700, 350, 233          //
// or 175 Hz, respectively                               //
//
// and for i2s:
// sampling = 44.100
// tone = 698 Hz
// n1 = 44.100/698 = 63; other values; 126,
///////////////////////////////////////////////////////////

#include "goertzel.h"

#ifdef CONFIG_DECODER_I2S
#include "I2S_Sidetone.hpp"
extern I2S_Sidetone sidetone;
#endif


using namespace Goertzel;

  float coeff;
  float sine;
  float cosine;
#ifdef CONFIG_DECODER_I2S
  const float sampling_freq = 44100.0;
  int16_t testData[8192]; // buffer for freq analysis - for i2s this needs to be a signed int and big enough for largest possible frame
  int goertzel_n = 512; // TODO: test different values with i2s
#else
  //const float sampling_freq = 106000.0;
  const float sampling_freq = 11905.0;
  uint16_t testData[1216];         /// buffer for freq analysis - max. 608 samples; you could increase this (and n) to a max of 1216, for sample time 10 ms, and bw 88 Hz
  int goertzel_n = 68;   //// you can use upto 608 608 - thats the max buffer reserved in checktone()
#endif
  float bw;
  float Q1 = 0;
  float Q2 = 0;
  uint32_t magnitudelimit;                                   // magnitudelimit_low = ( p_goertzelBandwidth? 80000 : 30000);
  uint32_t magnitudelimit_low;                               // magnitudelimit = magnitudelimit_low;

  const float target_freq = 698.0; /// adjust for your needs see above


void Goertzel::setup() {                 /// pre-compute some values (sine, cosine, coeff, magnitudelimit) that are compute-imntensive and won't change anyway
  // update Goertzel parameters depending on chosen bandwidth
  //const float target_freq = 698.0; /// adjust for your needs see above
  //const int goertzel_n = 304; //// you can use:         152, 304, 456 or 608 - thats the max buffer reserved in checktone() 
  #ifdef CONFIG_DECODER_I2S
      goertzel_n = ( MorsePreferences::pliste[posGoertzelBandwidth].value == 0 ? 63 : 252);  
      magnitudelimit_low = ( MorsePreferences::pliste[posGoertzelBandwidth].value? 80000 : 20000);          // values found by experimenting
  #else
      goertzel_n = ( MorsePreferences::pliste[posGoertzelBandwidth].value == 0 ? 17 : 68);  
      magnitudelimit_low = ( MorsePreferences::pliste[posGoertzelBandwidth].value? 20000 : 5000);          // values found by experimenting
 
  #endif
  magnitudelimit = magnitudelimit_low;
  bw = (sampling_freq / goertzel_n ); //348
  int  k;
  float omega;
  k = (int) (0.5 + ((goertzel_n * target_freq) / sampling_freq)); // 1,5
  omega = (2.0 * PI * k) / goertzel_n ;                           //0,0620425552
  sine = sin(omega);                                              // 0,062002759
  cosine = cos(omega);                                            // 0,998075978
  coeff = 2.0 * cosine;                                           // 1,996151956
  }

boolean Goertzel::checkInput() {                  // check the audio input for a signal
    float magnitude ;

#ifdef CONFIG_DECODER_I2S
    int num_bytes=sidetone.available();
    //DEBUG("Goertzel: num_bytes = " + String(num_bytes));
    if (num_bytes < goertzel_n*4) {
      //DEBUG("Goertzel: Not enough data available, returning false");
      return false; // not enough data to process
    }
    //DEBUG("Goertzel: Enough data available, proceeding with processing");
 
    int bytes_read = sidetone.readBytes((uint8_t *)testData, goertzel_n*4);
    for (int index = 0; index < 2*goertzel_n ; index+=2) {  // every 2nd value, so ignoring one channel
#else
unsigned long zeit;
    zeit = micros();
    //DEBUG("Start: " + String(zeit));
    for (int index = 0; index < goertzel_n ; index++)
            testData[index] = analogRead(audioInPin);
    //zeit = micros() - zeit;
    //DEBUG("Elapsed: " + String(zeit));
    //DEBUG("Time per read: " + String(zeit / goertzel_n) + " us");
    for (int index = 0; index < goertzel_n ; index++) {
#endif
      //DEBUG("testData[" + String(index) + "] = " + String(testData[index]));
      float Q0;

      Q0 = coeff * Q1 - Q2 + (float) testData[index]; 
      Q2 = Q1;
      Q1 = Q0;
    }
    // DEBUG("Calculated Q1 and Q2! "  + String(Q1) + " " + String(Q2));

    float magnitudeSquared = (Q1 * Q1) + (Q2 * Q2) - (Q1 * Q2 * coeff); // we do only need the real part //
    magnitude = sqrt(magnitudeSquared);
    Q2 = 0;
    Q1 = 0;
    // DEBUG("Magnitude: " + String(magnitude) + " Limit: " + String(magnitudelimit));   //// here you can measure magnitude for setup..
    
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
