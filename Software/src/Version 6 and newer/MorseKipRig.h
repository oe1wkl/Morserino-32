#ifndef MORSEKIPRIG_H_
#define MORSEKIPRIG_H_
/******************************************************************************************************************************
 *  M32KIP Rig unit — reproduces a remote operator's keying on this Morserino's transmitter output.
 *
 *  Listens on UDP 7374, authenticates with the pre-shared key, and rebuilds the key line from the timestamps the Keyer unit
 *  sends, delayed by a constant playout delay so that jitter is absorbed rather than heard. The reconstruction itself is
 *  MorseKipRig's only because it owns the hardware: all of the protocol lives in M32Kip.*, which is host-tested.
 *
 *  Two properties this file exists to guarantee, both measured rather than assumed (devdocs/m32kip/PHASE0_RESULTS.md):
 *    - edges are written from a hardware-timer ISR on core 1, not from loop(), which stalls for milliseconds under WiFi
 *      load and tens of milliseconds on a display redraw;
 *    - nothing else may touch the key line while this mode owns it. `kipRig` appears in none of keyOut()'s Key-Ext-Tx
 *      cases, so keyTransmitter() cannot fire behind the ISR's back.
 *
 *  Safety is spec §8 and is enforced here, not by the far end: a mark longer than the limit, a lost link, a BYE, a local
 *  paddle touch or the encoder long-press all put the key line up immediately.
 *****************************************************************************************************************************/

#include "morsedefs.h"

namespace MorseKipRig {
    /// Runs the Rig unit until the operator leaves it. Self-contained, like the QSO Bot and the games: it owns the
    /// display and the buttons for its duration and returns to the caller, which returns to the menu.
    void run();
}
#endif /* MORSEKIPRIG_H_ */
