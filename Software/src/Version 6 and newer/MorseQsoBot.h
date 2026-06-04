// MorseQsoBot.h — simulated CW QSO partner for keyer practice.
//
// v1 sub-modes (selected from the menu): SOTA, POTA, Standard, Contest.
// The bot transmits in CW via MorseCwEngine and listens to the user's
// keyed replies, driven by a per-QSO-type sequence descriptor. Built on
// both classic and Pocket. The display is the shared OLED scroll
// buffer (printToScroll), so the same code reaches both targets.
//
// PR-3 ships SOTA (activator + chaser roles). POTA / Standard / Contest
// land in PR-4.

#ifndef MORSEQSOBOT_H_
#define MORSEQSOBOT_H_

#include "morsedefs.h"
#include <Arduino.h>

#ifdef CONFIG_QSO_BOT

namespace MorseQsoBot {

// ---- Sequence descriptor types ------------------------------------------

// A QsoStep is one entry in a per-QSO-type sequence: either the bot
// transmits a templated string, or the bot waits for a slot-matching
// token from the user.
enum QsoStepKind : uint8_t {
    STEP_END        = 0,    // terminator: marks end of descriptor
    STEP_BOT_TX,            // bot transmits `tmpl` (with placeholders expanded)
    STEP_EXPECT,            // bot waits for a token matching `slot`; on
                            // failure: AGN recovery (bounded retries)
    STEP_EXPECT_OPT,        // optional acknowledgement; auto-advance on timeout
    STEP_WAIT_USER_CQ,      // dynamic opening: listen ~5 s. If the user calls
                            // CQ (starts keying), capture their call and fall
                            // through to the next step (the "user initiated"
                            // branch). If silence, jump to step `arg` (the
                            // "bot calls CQ" branch).
    STEP_PAUSE_MS,          // small inter-step pause (PR-5 jitter target)
};

// Typed slots the matcher knows how to recognise. PR-3 uses CALLSIGN /
// RST / REF / PROSIGN (the SOTA subset). PR-4 adds NAME, QTH, SERIAL,
// ZONE, RIG, ANT, WX, AGE.
enum QsoSlotKind : uint8_t {
    SLOT_NONE = 0,
    SLOT_CALLSIGN,          // 3..7 alphanumeric, contains a digit; /SUFFIX OK
    SLOT_RST,               // three digits (first 3..5), or literal "5NN"
    SLOT_REF,               // SOTA summit ref or POTA park ref, literal match
    SLOT_PROSIGN,           // BK / AR / KN / SK / K / R / 73 / 72 / TU / ...
};

struct QsoStep {
    QsoStepKind kind;
    const char* tmpl;       // BOT_TX: text with [PLACEHOLDERS]; EXPECT*:
                            // expected-token literal (for PROSIGN matching)
    QsoSlotKind slot;       // for EXPECT*: which slot to match against
    uint16_t    arg;        // PAUSE_MS: ms; EXPECT: per-step retry budget
                            // (0 = use default of 2; CALLSIGN-first should
                            // pass 4)
};

struct QsoDescriptor {
    const char*       name;         // shown in the transcript header
    const QsoStep*    steps;
    uint8_t           stepCount;
    uint16_t          features;     // reserved for PR-4 (SERIAL_INC,
                                    // CUT_NUMBERS_OK, REF_OPTIONAL, ...)
    uint8_t           defaultWpm;   // recommended bot WPM (currently the
                                    // engine uses live keyer timings; this
                                    // is documentation for now)
};

// Public entry: dispatched from MorseMenu's menuExec for any of
// _qsoSotaPota / _qsoStandard / _qsoContest. Blocks until the
// QSO completes or the user exits with a button press.
void run(menuNo mode);

}  // namespace MorseQsoBot

#endif  // CONFIG_QSO_BOT
#endif  // MORSEQSOBOT_H_
