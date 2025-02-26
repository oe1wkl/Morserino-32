#ifdef  CONFIG_ENGLISH_OXFORD
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
