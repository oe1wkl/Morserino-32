#ifdef  CONFIG_ENGLISH_OXFORD

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

#include "english_words.h"
#include <Arduino.h>
#include <stdlib.h>

const int totalWords = sizeof(words) / sizeof(words[0]);

const char* getEnglishWord(int maxWordLength=100) {
    // Calculate the total weight for words within the length limit
    int totalWeight = 0;
    for (int i = 0; i < totalWords; ++i) {
        if (strlen(words[i].word) <= maxWordLength) {
            totalWeight += words[i].weight;
        }
    }

    // Generate a random number between 0 and totalWeight
    int randomValue = random(totalWeight);

    // Select the word based on the random value
    int cumulativeWeight = 0;
    for (int i = 0; i < totalWords; ++i) {
        if (strlen(words[i].word) <= maxWordLength) {
            cumulativeWeight += words[i].weight;
            if (randomValue < cumulativeWeight) {
                return words[i].word;
            }
        }
    }

    return nullptr; // Should not reach here
}
#endif // CONFIG_ENGLISH_OXFORD
