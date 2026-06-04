// MorseCwEngine.h — shared non-blocking CW player for game/bot modes.
//
// Replaces the two parallel cwPlayer implementations previously inlined
// in MorseRadioCave.cpp and MorsePileup.cpp. Behaviour-preserving for
// both callers via PlayOpts:
//
//   - RC: fixed pitch, fixed per-call WPM, no loop, no resume-gap.
//   - Pileup: caller-supplied pitch, live keyer timings, loop with
//     inter-loop gap, settling resume-gap after the mute predicate
//     releases.
//
// The engine ALWAYS mutes when keyerState != IDLE_STATE; the caller's
// optional extraMute predicate is consulted IN ADDITION (logical OR) to
// cover caller-specific conditions (RC: leftKey || rightKey; Pileup:
// inputPos > 0 || ftpSoundPlaying).

#pragma once

#if defined(CONFIG_QSO_BOT) || defined(CONFIG_CW_GAME)

#include <Arduino.h>

namespace MorseCwEngine {

typedef bool (*MutePredicate)();
typedef void (*CharCallback)();   // notification only — caller maintains
                                  // its own pointer into the source text

struct PlayOpts {
    uint16_t      pitchHz;        // playback pitch (Hz), required
    uint8_t       wpm;            // 0 = use globals (ditLength/dahLength/
                                  //     interCharacterSpace, Farnsworth-aware)
                                  // >0 = fixed timing derived from this WPM
    bool          loop;           // true = restart at end with inter-loop gap
    uint8_t       resumeGapDits;  // 0 = no settling pause when mute clears;
                                  // N = N×ditMs() pause (Pileup uses 2)
    MutePredicate extraMute;      // nullptr OK; OR'd with keyerState != IDLE
    CharCallback  onCharComplete; // nullptr OK; fires at each '0'
                                  // (inter-char gap), each ' ' (inter-word
                                  // gap), and once at end-of-stream.
                                  // Caller advances its own char pointer.
};

void  playStart(const String& text, const PlayOpts& opts);
void  playStop();
bool  playTick();          // returns true while playing (for callers that care)
bool  isPlaying();
bool  isToneOn();          // true when audio is currently keyed out

uint8_t getPlayCount();           // # of completed playthroughs in loop mode
void    setPlayCount(uint8_t c);  // preserve/restore across a restart

} // namespace MorseCwEngine

#endif // CONFIG_QSO_BOT || CONFIG_CW_GAME
