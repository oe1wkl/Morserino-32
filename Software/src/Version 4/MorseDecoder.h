#ifndef MORSEDECODER_H_
#define MORSEDECODER_H_

#include <Arduino.h>
#include "morsedefs.h"
#include "goertzel.h"

#define USE_KEY true
#define USE_AUDIO false

extern boolean leftKey, rightKey;

extern echoStates echoTrainerState;
//extern const struct linklist CWtree[];
extern byte treeptr;
extern boolean lastWasKey;
extern boolean speedChanged;

extern void displayMorse(String, boolean);
extern void cwForTx (int);
extern void sendWithLora();
extern void sendWithWifi();
 
void keyOut(boolean,  boolean, int, int);

///////////////////////////////////////////// the tree for decoding CW //////////////////////////////////////

struct linklist {
     const char* symb;
     const uint8_t dit;
     const uint8_t dah;
};


const struct linklist CWtree[67]  = {
  {"",1,2},            // 0
  {"e", 3,4},         // 1
  {"t",5,6},          // 2
//
  {"i", 7, 8},        // 3
  {"a", 9,10},        // 4
  {"n", 11,12},       // 5
  {"m", 13,14},       // 6
//
  {"s", 15,16},       // 7
  {"u", 17,18},       // 8
  {"r", 19,20},       // 9
  {"w", 21,22},       //10
  {"d", 23,24},       //11
  {"k", 25, 26},      //12
  {"g", 27, 28},      //13
  {"o", 29,30},       //14
//---------------------------------------------
  {"h", 31,32},       // 15
  {"v", 33, 34},      // 16
  {"f", 63, 63},      // 17
  {"ü", 35, 36},      // 18 german ue
  {"l", 37, 38},      // 19
  {"ä", 39, 63},      // 20 german ae
  {"p", 63, 40},      // 21
  {"j", 63, 41},      // 22
  {"b", 42, 43},      // 23
  {"x", 44, 63},      // 24
  {"c", 63, 45},      // 25
  {"y", 46, 63},      // 26
  {"z", 47, 48},      // 27
  {"q", 63, 63},      // 28
  {"ö", 49, 63},      // 29 german oe
  {"<ch>", 50, 51},        // 30 !!! german "ch" 
//---------------------------------------------
  {"5", 64, 63},      // 31
  {"4", 63, 63},      // 32
  {"<ve>", 63, 52},      // 33  or <sn>, sometimes "*"
  {"3", 63,63},       // 34
  {"*", 53,63,},      // 35 * used for all unidentifiable characters ¬
  {"2", 63, 63},      // 36 
  {"<as>", 63,63},         // 37 !! <as>
  {"*", 54, 63},      // 38
  {"+", 63, 55},      // 39
  {"*", 56, 63},      // 40
  {"1", 57, 63},      // 41
  {"6", 63, 58},      // 42
  {"=", 63, 63},      // 43
  {"/", 63, 63},      // 44
  {"<ka>", 59, 60},        // 45 !! <ka>
  {"<kn>", 63, 63},        // 46 !! <kn>
  {"7", 63, 63},      // 47
  {"*", 63, 61},      // 48
  {"8", 62, 63},      // 49
  {"9", 63, 63},      // 50
  {"0", 63, 63},      // 51
//
  {"<sk>", 63, 63},        // 52 !! <sk>
  {"?", 63, 63},      // 53
  {"\"", 63, 63},      // 54
  {".", 63, 63},      // 55
  {"@", 63, 63},      // 56
  {"\'",63, 63},      // 57
  {"-", 63, 63},      // 58
  {";", 63, 63},      // 59
  {"!", 63, 63},      // 60
  {",", 63, 63},      // 61
  {":", 63, 63},      // 62
//
  {"*", 63, 63},       // 63 Default for all unidentified characters
  {"*", 65, 63},       // 64
  {"*", 66, 63},       // 65
  {"<err>", 66, 63}      // 66 !! Error - backspace
};

//// we define two classes: MorseTable (a pointer to the linked list defined above, and mthods to manipulate the pointer)
////                   and  Decoder (which decodes signals from a key or from audio - derived from a Goertzel filter) 

class MorseTable {

private:
  byte treeptr;

public:
  MorseTable();
  void recordDit();
  void recordDah();  
  void resetTable();  
  String retrieveSymbol();
};


class Decoder {

private:
  boolean fromKey;                        /// if true, decode  from straight key, if false, from audio through Goertzel
  boolean filteredState;
  boolean filteredStateBefore;
  DECODER_STATES decoderState;

  // Noise Blanker time (currently fixed)     
  int nbtime;  /// ms noise blanker
  
  unsigned long startTimeHigh;
  unsigned long highDuration;
  unsigned long lastStartTime;
  boolean realstate;
  boolean realstatebefore;
  long startTimeLow;
  long lowDuration;  
  unsigned long ditAvg, dahAvg;     /// average values of dit and dah lengths to decode as dit or dah and to adapt to speed change
  uint8_t d_wpm;                    /// wpm as decoded

  boolean checkInput();
  void ON_();
  void OFF_();
  void recalculateDit(unsigned long );
  void recalculateDah(unsigned long );

  //byte treeptr;                          // pointer used to navigate within the linked list representing the dichotomic tree
  MorseTable myTable;
  
public:
  Decoder(boolean);
  void setup();
  void decode();
  uint8_t getWpm();

}; /// end of class Decoder

#endif /* MORSEDECODER_H_ */
