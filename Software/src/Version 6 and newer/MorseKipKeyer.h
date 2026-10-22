#ifndef MORSEKIPKEYER_H_
#define MORSEKIPKEYER_H_
/******************************************************************************************************************************
 *  M32KIP Keyer unit — the operator's end. Sends the *timing* of the key line to a remote Rig unit, which reproduces it.
 *
 *  **It is the CW Keyer, plus a network link.** Remote Keyer runs inside the global loop exactly as WiFi Trx does, so
 *  everything an operator knows from the CW Keyer works unchanged: the keyed text scrolls on the display, the black-knob
 *  click recalls keyer memories, the encoder owns speed and volume, the double click opens preferences, the long press
 *  leaves. None of that is re-implemented here, and none of it may be - UX conventions make the classic modes the
 *  standard, and a first version that ran its own loop lost both the keyed text and the memories.
 *
 *  What this module adds is only the link: the handshake, a send task that owns the socket, and the edge hook. Two rules
 *  measured in Phase 0 and confirmed on hardware still hold, and are why the pieces sit where they do:
 *    - **never send from the keying path.** keyOut() only timestamps and queues; a task on core 0 transmits.
 *    - **never redraw while an edge can arrive.** tick() touches the display only when the link state *changes*, and the
 *      global loop only calls it in a gap - not while the paddles are keying and not while a memory is playing.
 *
 *  The local transmitter output is never keyed: `kipKeyer` appears in no Key-Ext-Tx case in keyOut().
 *****************************************************************************************************************************/

#include "morsedefs.h"

namespace MorseKipKeyer {
    /// Called from menuExec() on entry. Checks the configuration, brings WiFi up, handshakes with the rig and starts the
    /// send task. Returns false (with a message on screen) if any of that fails, so the device stays in the menu.
    ///
    /// Remote WinKeyer (devdocs/winkeyer/DESIGN.md §9.1 R6) passes two more things: `serve`, called throughout every
    /// wait in here, so the WinKeyer host is answered while WiFi and the rig take their seconds to come up; and
    /// `persist`, for a host session that is already open and cannot be refused. With `persist` begin() always returns
    /// true: a rig that does not answer is called again once a second (D14), and a missing configuration or a failed
    /// WiFi connect leaves the mode running without a link, its error on screen. It also skips the start screen, which
    /// the caller draws itself.
    bool begin(void (*serve)() = nullptr, bool persist = false);

    /// The rig's address as resolved at begin(), for a start screen.
    String peerName();

    /// Stops the send task, says BYE and releases the socket. Idempotent - called from menu_() on every return to the
    /// menu, before WiFi is switched off, because a BYE sent after that could not leave the device at all.
    void end();

    /// Called by the global loop, only in a gap. Tracks the link and repaints the top line when it changes.
    void tick();

    /// True while the rig is answering. Drives the WiFi logo in the top bar, the slot WiFi Trx uses.
    bool linked();

    /// Records one key-line transition, called from keyOut() at the exact point the local TX line would be toggled.
    /// That catches the iambic keyer, a recalled memory and the straight key alike, since all three key through there.
    /// A no-op outside this mode, and it never blocks.
    void noteEdge(bool down);

    // ---- Remote WinKeyer (devdocs/winkeyer/DESIGN.md §9.1) ----

    /// R3a: a logger's Tune or Key Buffered holds the key for seconds, which the rig would cut at its keyer limit
    /// (Rig Limit Kyr, 3 s). While such a mark is on, and for a second after it (the rig plays the key-up out one
    /// playout delay later, and judges the mark by the source in force then), the edges go out as source 1, so the
    /// straight-key limit applies - what spec §8 "Tuning" prescribes. Call with true just before the key goes down.
    void setLongMark(bool on);

    /// The rig's current playout delay, from its last STATS (150 ms until one has arrived). R5 holds the WinKeyer's
    /// BUSY this long after the host's last key-up, so a logger switching PTT over CAT does not clip the rig's tail.
    uint16_t playoutMs();

    /// The key-down limit the rig applies to the current source, from its last STATS (0 until one has arrived).
    uint16_t rigMaxKeydownMs();

    // ---- the rig's own settings, adjusted from the operating position (D17) ----
    //
    // The rig items appear in this mode's preferences, first in the list and prefixed, and only while a link is up.
    // Everything here is non-blocking: a request is handed to the send task, never transmitted from the caller, and
    // the values the menu shows come from the cached copy so it opens instantly. A fetch on entry corrects them.

    /// True when a link is up AND the rig on the other end answers configuration requests. An older rig never sets
    /// the capability bit, so the items stay hidden rather than waiting for an answer that cannot come.
    bool rigCfgAvailable();

    /// The last values the rig reported. Valid whenever rigCfgAvailable() is true; seeded at link-up.
    /// Indices are the six rig prefPos values, in the order the menu shows them.
    uint8_t rigCfgValue(uint8_t index);

    /// Number of rig settings, and the prefPos each index maps to.
    uint8_t rigCfgCount();
    prefPos rigCfgPref(uint8_t index);

    /// Ask the rig to send its current settings. Returns at once; the answer updates the cache.
    void rigCfgFetch();

    /// Change one setting on the rig: updates the cache so the menu responds immediately, and queues the set.
    /// The rig replies with what it actually stored, which corrects the cache if it clamped anything.
    void rigCfgSet(uint8_t index, uint8_t value);

    /// True while a set is still unacknowledged - the preferences menu waits for this before letting the long press
    /// return to keying, so the rig's flash write finishes first (D17 decision 3).
    bool rigCfgPending();
}
#endif /* MORSEKIPKEYER_H_ */
