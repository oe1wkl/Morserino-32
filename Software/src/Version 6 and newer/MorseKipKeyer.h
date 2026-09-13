#ifndef MORSEKIPKEYER_H_
#define MORSEKIPKEYER_H_
/******************************************************************************************************************************
 *  M32KIP Keyer unit — the operator's end. Runs the ordinary keyer and sidetone and sends the *timing* of the key line to a
 *  remote Rig unit, which reproduces it on its transmitter.
 *
 *  The keying logic stays here on purpose (spec §4.2): sidetone and key feel are local and completely unaffected by the
 *  network. Nothing but the edge timestamps goes on the wire, and the local transmitter output is never keyed - `kipKeyer`
 *  appears in no Key-Ext-Tx case in keyOut(), so "Key Ext Tx" cannot make this mode key a rig at the operator's end.
 *
 *  Two rules this file exists to obey, both measured in Phase 0 and confirmed in Phase 2 (devdocs/m32kip/):
 *    - **never send from the keying path.** A socket write from the loop stalls it 2-8 ms, and the Phase 2 bench run showed
 *      what that does: edges leave in late bursts and the far end raises its playout delay to the ceiling to cover them.
 *      Edges go into a queue; a separate task owns the socket.
 *    - **never redraw while an edge can arrive.** A redraw costs 37 ms on the OLED and 46 ms on the TFT, which is longer
 *      than a dit at any speed worth using. The display waits for a gap.
 *****************************************************************************************************************************/

#include "morsedefs.h"

namespace MorseKipKeyer {
    /// Runs the Keyer unit until the operator leaves it. Self-contained, like the Rig unit and the QSO Bot.
    void run();

    /// Records one key-line transition, called from keyOut() at the exact point the local TX line would be toggled -
    /// which is what spec §10.1 asks for source 0, and which catches the straight key too, since the decoder keys
    /// through the same function. A no-op outside this mode, and it never blocks.
    void noteEdge(bool down);
}
#endif /* MORSEKIPKEYER_H_ */
