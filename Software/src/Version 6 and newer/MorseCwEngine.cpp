// MorseCwEngine.cpp — see MorseCwEngine.h for the rationale.

#include "MorseCwEngine.h"

#if defined(CONFIG_QSO_BOT) || defined(CONFIG_CW_GAME)

#include "morsedefs.h"
#include "MorseOutput.h"
#include "MorsePreferences.h"

// generateCWword lives in m32_v6.ino (translates symbols -> '1'/'2'/'0' string).
extern String generateCWword(const String& symbols);

// Keyer globals from m32_v6.ino.
extern KEYERSTATES   keyerState;
extern boolean       leftKey, rightKey;
extern unsigned int  ditLength;
extern unsigned int  dahLength;
extern unsigned int  interCharacterSpace;
extern unsigned int  interWordSpace;

namespace MorseCwEngine {

namespace {

// Element stream encoding (from generateCWword + our inter-word ' ' markers):
//   '1' dit, '2' dah, '0' inter-character gap, ' ' inter-word gap.
// Sized to fit a typical QSO Bot transmission, e.g.
// "cq sota cq sota de [BOTCALL] [BOTCALL] K" — about 35 source chars,
// ~280 elements. RC's longest known clue ("rc0 rc0 de ir7 ir7 k") and
// Pileup callsigns both fit comfortably.
constexpr int kMaxElements = 384;

char           elements[kMaxElements + 1];
int            elementLen   = 0;
int            elementPos   = 0;
bool           playing      = false;
bool           toneOn       = false;
bool           paused       = false;
unsigned long  nextEventMs  = 0;
PlayOpts       opts         = {};
uint8_t        playCount    = 0;

// Pre-computed when opts.wpm > 0; ignored when opts.wpm == 0 (we read globals).
unsigned int   fixedDitMs   = 0;

inline unsigned int ditMs()     { return opts.wpm > 0 ? fixedDitMs     : ditLength; }
inline unsigned int dahMs()     { return opts.wpm > 0 ? fixedDitMs * 3 : dahLength; }
inline unsigned int charGapMs() { return opts.wpm > 0 ? fixedDitMs * 2 : interCharacterSpace; }
inline unsigned int wordGapMs() { return opts.wpm > 0 ? fixedDitMs * 6 : interWordSpace; }

// Build the element stream from text. Words are space-separated; within a
// word, characters are joined by generateCWword's own '0' inter-element
// markers. Between words we insert a literal ' ' marker.
void buildStream(const String& text) {
    elementLen  = 0;
    elements[0] = '\0';

    const int textLen = text.length();
    int start = 0;
    while (start < textLen) {
        const int sp = text.indexOf(' ', start);
        const String word = (sp < 0) ? text.substring(start)
                                     : text.substring(start, sp);
        // Case is preserved deliberately: generateCWword uses uppercase
        // letters as prosign markers (e.g. K = <sk>). Callers control case.
        const String elems = generateCWword(word);
        for (int i = 0; i < (int)elems.length() && elementLen < kMaxElements; i++) {
            elements[elementLen++] = elems[i];
        }
        if (sp < 0) break;
        if (elementLen < kMaxElements) elements[elementLen++] = ' ';
        start = sp + 1;
    }
    elements[elementLen] = '\0';
}

inline bool shouldMute() {
    if (keyerState != IDLE_STATE) return true;
    if (opts.extraMute && opts.extraMute()) return true;
    return false;
}

}  // namespace

void playStart(const String& text, const PlayOpts& newOpts) {
    opts = newOpts;
    fixedDitMs = (opts.wpm > 0) ? (1200u / opts.wpm) : 0;
    buildStream(text);
    elementPos  = 0;
    playing     = true;
    toneOn      = false;
    paused      = false;
    nextEventMs = millis();
    playCount   = 0;
}

void playStop() {
    if (toneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        toneOn = false;
    }
    playing = false;
}

bool playTick() {
    if (!playing) return false;

    if (shouldMute()) {
        if (toneOn) {
            MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
            toneOn = false;
        }
        paused      = true;
        nextEventMs = millis() + 100;       // recheck shortly
        return true;
    }

    // Resume from pause — optional settling gap (Pileup uses 2 dits;
    // RC uses 0 = fall through immediately). The fall-through branch is
    // identical to the original RC code path.
    if (paused) {
        paused = false;
        if (opts.resumeGapDits > 0) {
            nextEventMs = millis() + opts.resumeGapDits * ditMs();
            return true;
        }
    }

    if ((long)(millis() - nextEventMs) < 0) return true;   // waiting

    // End-of-tone: silence and schedule inter-element gap.
    if (toneOn) {
        MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
        toneOn = false;
        // Inter-element gap. Was `- 7`, which came out ~6 ms short of one dit:
        // pwmTone()/pwmNoTone() each block ~6 ms doing their volume ramp
        // ("soften the click"), and that ramp time was being counted against
        // the gap instead of added to it. The classic iambic keyer compensates
        // for exactly this (m32_v6.ino KEY_START/INTER_ELEMENT: its
        // inter-element is `millis() + ditLength - 1`, i.e. 6 ms longer than
        // this was), so match it — `- 1` adds the missing ~6 ms. This also
        // corrects the character/word gaps, which are built on top of this one
        // (the '0'/' ' cases below add to the gap this line already scheduled).
        // See devdocs/cw-timing-audit/FINDINGS.md. Shared engine — also affects
        // Fight the Pileup and Radio Cave (both strictly more spec-correct now).
        nextEventMs = millis() + ditMs() - 1;
        return true;
    }

    // End of stream.
    if (elementPos >= elementLen) {
        // The last source character has no trailing '0' marker — fire
        // the char callback here so callers get one tick per char,
        // including the final one.
        if (opts.onCharComplete) opts.onCharComplete();
        if (opts.loop) {
            playCount++;
            elementPos  = 0;
            nextEventMs = millis() + wordGapMs() + ditMs() * 3;   // inter-loop gap
            return true;
        }
        playStop();
        return false;
    }

    const char c = elements[elementPos++];
    switch (c) {
        case '1':                                       // dit
            MorseOutput::pwmTone(opts.pitchHz,
                                 MorsePreferences::sidetoneVolume, false);
            toneOn      = true;
            nextEventMs = millis() + ditMs() - 7;
            break;
        case '2':                                       // dah
            MorseOutput::pwmTone(opts.pitchHz,
                                 MorsePreferences::sidetoneVolume, false);
            toneOn      = true;
            nextEventMs = millis() + dahMs() - 7;
            break;
        case '0':                                       // inter-character gap
            nextEventMs = millis() + charGapMs();
            if (opts.onCharComplete) opts.onCharComplete();
            break;
        case ' ':                                       // inter-word gap
            nextEventMs = millis() + wordGapMs();
            if (opts.onCharComplete) opts.onCharComplete();
            break;
        default:
            break;                                      // unknown — skip
    }
    return true;
}

bool    isPlaying()              { return playing;  }
bool    isToneOn()               { return toneOn;   }
uint8_t getPlayCount()           { return playCount; }
void    setPlayCount(uint8_t c)  { playCount = c;   }

}  // namespace MorseCwEngine

#endif // CONFIG_QSO_BOT || CONFIG_CW_GAME
