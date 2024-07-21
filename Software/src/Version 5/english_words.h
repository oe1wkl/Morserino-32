#ifndef ENGLISH_WORDS_H
#define ENGLISH_WORDS_H

/// The most common English Words in various lengths for CW Trainer
///////////////////////////////////////////////////////////////////

namespace EnglishWords
  {
  const int WORDS_NUMBER_OF_ELEMENTS = 1;                           // how many items all together?
  const int WORDS_MAX_SIZE = 14;                                      // longest item  +1
  const int WORDS_POINTER[] = {0, 370, 345, 275, 144, 66, 34, 14};    // array where items start with length = index
  
  const String words[WORDS_NUMBER_OF_ELEMENTS]  = { 
    { "international" },

    };
  }
#endif