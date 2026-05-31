// MorseQsoBot.cpp — v1 stub.
//
// PR-2 scope: confirm the bot is reachable from the menu on both
// platforms, the option array swaps in, the safety state engages, and a
// button press returns to the menu cleanly. The real state machine,
// matcher, and descriptors land in PR-3+.

#include "MorseQsoBot.h"

#ifdef CONFIG_QSO_BOT

#include "MorseOutput.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "ClickButton.h"

extern boolean checkPaddles();

namespace {

const char* subTypeLabel(menuNo mode) {
    switch (mode) {
        case _qsoSota:     return "SOTA";
        case _qsoPota:     return "POTA";
        case _qsoStandard: return "Standard";
        case _qsoContest:  return "Contest";
        default:           return "?";
    }
}

}  // namespace

namespace MorseQsoBot {

void run(menuNo mode) {
    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, "QSO Bot v1 stub");
    MorseOutput::printOnScroll(1, REGULAR,      0, subTypeLabel(mode));
    MorseOutput::printOnScroll(2, REGULAR,      0, "press a button");
    MorseOutput::refreshDisplay();

    // Drain stale click counts so we don't immediately exit from a click
    // left over from the menu navigation that brought us here.
    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks  = 0;

    for (;;) {
        Buttons::modeButton.Update();
        Buttons::volButton.Update();
        if (Buttons::modeButton.clicks != 0 ||
            Buttons::volButton.clicks  != 0) {
            Buttons::modeButton.clicks = 0;
            Buttons::volButton.clicks  = 0;
            return;
        }
        // Touch the paddle scan so the keyer's idle-shutdown timer doesn't
        // fire while we're sitting in the stub. The real bot will be doing
        // far more on each tick.
        checkPaddles();
        delay(20);
    }
}

}  // namespace MorseQsoBot

#endif  // CONFIG_QSO_BOT
