// MorseQsoBot.h — simulated CW QSO partner for keyer practice.
//
// v1 sub-modes (selected from the menu): SOTA, POTA, Standard, Contest.
// The bot transmits in CW via MorseCwEngine and listens to the user's
// keyed replies. Built on both classic and Pocket. The display is the
// shared OLED scroll buffer; no game canvas is used (so the same code
// reaches both targets).
//
// This header is the public surface from MorseMenu's dispatch. The
// implementation in MorseQsoBot.cpp owns the state machine and matcher.

#ifndef MORSEQSOBOT_H_
#define MORSEQSOBOT_H_

#include "morsedefs.h"

#ifdef CONFIG_QSO_BOT

namespace MorseQsoBot {

// Enter a QSO of the type indicated by `mode` (one of _qsoSota, _qsoPota,
// _qsoStandard, _qsoContest). Blocks for the duration of the QSO; returns
// when the user exits via a long-press or the QSO terminates naturally.
void run(menuNo mode);

}  // namespace MorseQsoBot

#endif  // CONFIG_QSO_BOT
#endif  // MORSEQSOBOT_H_
