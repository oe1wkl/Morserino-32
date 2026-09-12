#ifndef M32KIP_H_
#define M32KIP_H_
/******************************************************************************************************************************
 *  M32KIP — Morserino Keying over IP: protocol core.
 *
 *  Real-time CW keying between two Morserinos: the Keyer unit timestamps the edges of its key line and the Rig unit
 *  reproduces them at a constant delay D. Specification: devdocs/m32kip/SPEC.md; ratified decisions and the Phase 0
 *  measurements that constrain the implementation: devdocs/m32kip/DECISIONS.md, PHASE0_RESULTS.md.
 *
 *  This file is deliberately free of Arduino, FreeRTOS and ESP-IDF dependencies: it is the part that can be compiled and
 *  tested on the host (Software/tests/kip/). Sockets, timers, GPIO, display and preferences live in MorseKipKeyer.* and
 *  MorseKipRig.*, which own all the platform contact. Nothing here allocates, blocks or keeps wall-clock time of its own —
 *  every entry point is handed the caller's clock reading in ticks.
 *
 *  Time is in **ticks of 100 µs** throughout (spec §4.1), carried as uint32 and compared modularly: never write
 *  `a < b` on two tick values, always `tickDiff(a, b) < 0`. The wrap period is ~119 hours.
 *****************************************************************************************************************************/

#include <stdint.h>
#include <stddef.h>

namespace M32Kip {

// ---------------------------------------------------------------- wire constants (spec §6)

const uint8_t  MAGIC            = 0x4B;     // 'K'
const uint8_t  VERSION          = 0x01;
const uint16_t DEFAULT_PORT     = 7374;     // MOPP uses 7373; adjacent, distinct
const size_t   HEADER_LEN       = 12;
const size_t   MAC_LEN          = 8;        // HMAC-SHA256 truncated
const size_t   EDGE_LEN         = 5;
const uint8_t  MAX_EDGES        = 8;        // per KEY packet
const size_t   MAX_PACKET       = HEADER_LEN + 8 + MAX_EDGES * EDGE_LEN + MAC_LEN;   // 68 bytes

const uint32_t TICKS_PER_MS     = 10;       // 100 µs ticks
inline uint32_t msToTicks(uint32_t ms)      { return ms * TICKS_PER_MS; }
inline uint32_t ticksToMs(uint32_t ticks)   { return ticks / TICKS_PER_MS; }

/// Modular comparison of two tick values. Positive when `a` is later than `b`.
inline int32_t tickDiff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

enum PacketType {
    PKT_HELLO     = 1,      // Keyer → Rig
    PKT_HELLO_ACK = 2,      // Rig → Keyer
    PKT_KEY       = 3,      // Keyer → Rig
    PKT_STATS     = 4,      // Rig → Keyer
    PKT_BYE       = 5,      // either
    PKT_NACK      = 6       // Rig → Keyer
};

enum NackReason {
    NACK_BUSY    = 1,
    NACK_VERSION = 2
    // Reason 3 (auth) is deliberately absent: a packet that fails its MAC is dropped in silence
    // (D12c). A wrong pre-shared key shows up on the Keyer as a HELLO timeout, and an attacker
    // learns nothing about whether a key was close.
};

enum KeySource {
    SRC_KEYER    = 0,       // internal iambic / Ultimatic keyer: machine-exact elements
    SRC_STRAIGHT = 1,       // straight key or bug: human timing, marks can be arbitrarily long
    SRC_EXTERNAL = 2        // external keyer fed into the paddle jack
};

enum KeyState { KEY_UP = 0, KEY_DOWN = 1 };

// ---------------------------------------------------------------- packet structures
//
// These are plain structs with explicit little-endian codecs rather than packed structs mapped onto the buffer:
// the wire format then does not depend on the compiler's padding or the host's byte order, so the same code is
// correct on the ESP32 and in the host tests.

struct Header {
    uint8_t  type;
    uint8_t  flags;
    uint32_t session;       // 0 in HELLO; assigned by the Rig unit in HELLO_ACK
    uint16_t seq;           // per-sender, increments by 1 per packet, wraps
    Header() : type(0), flags(0), session(0), seq(0) {}
};

struct Hello {
    uint8_t  nonceC[8];
    uint32_t tNow;          // Keyer clock, ticks
    uint8_t  wpm;           // informational only; 0 = unknown (straight key, bug, external)
    uint8_t  redundancy;    // requested R
    uint8_t  source;        // KeySource
    uint8_t  caps;          // 0 in v1
    Hello() : tNow(0), wpm(0), redundancy(4), source(SRC_KEYER), caps(0) { for (int i = 0; i < 8; i++) nonceC[i] = 0; }
};

struct HelloAck {
    uint8_t  nonceC[8];     // echoed
    uint8_t  nonceS[8];
    uint32_t session;       // newly assigned, non-zero
    uint16_t maxKeydownMs;  // Rig-enforced mark limit for this session
    uint8_t  pttLeadMs;     // 0 = PTT disabled
    uint8_t  flags;         // bit0: PTT available
    HelloAck() : session(0), maxKeydownMs(0), pttLeadMs(0), flags(0) {
        for (int i = 0; i < 8; i++) { nonceC[i] = 0; nonceS[i] = 0; }
    }
};

struct Nack {
    uint8_t reason;
    Nack() : reason(0) {}
};

struct Edge {
    uint32_t t;             // Keyer clock, ticks
    uint8_t  state;         // KeyState
    Edge() : t(0), state(KEY_UP) {}
    Edge(uint32_t tt, uint8_t s) : t(tt), state(s) {}
};

struct KeyPkt {
    uint32_t tNow;          // Keyer clock at send time: drives offset tracking and the keepalive watchdog
    uint8_t  wpm;
    uint8_t  source;
    uint8_t  n;             // number of edges, 0..MAX_EDGES
    Edge     edges[MAX_EDGES];      // newest LAST
    KeyPkt() : tNow(0), wpm(0), source(SRC_KEYER), n(0) {}
};

struct Stats {
    uint16_t playoutMs;
    uint16_t jitter100us;
    uint8_t  lossPct;
    uint8_t  lateEdges;
    uint8_t  underruns;
    uint8_t  rigState;      // bit0 key down, bit1 PTT active, bit2 watchdog tripped, bit3 max-keydown hit
    uint16_t maxKeydownMs;
    uint16_t ditEst100us;
    uint32_t tEcho;         // tNow of the most recently accepted KEY
    uint32_t tRig;          // Rig clock at send time
    Stats() : playoutMs(0), jitter100us(0), lossPct(0), lateEdges(0), underruns(0),
              rigState(0), maxKeydownMs(0), ditEst100us(0), tEcho(0), tRig(0) {}
};

const uint8_t RIG_KEY_DOWN     = 0x01;
const uint8_t RIG_PTT_ACTIVE   = 0x02;
const uint8_t RIG_WATCHDOG     = 0x04;
const uint8_t RIG_KEYDOWN_LIMIT= 0x08;

// ---------------------------------------------------------------- crypto (spec §9)

void sha256(const uint8_t* data, size_t len, uint8_t out[32]);
void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t len, uint8_t out[32]);

/// K_base = SHA-256(PSK)
void deriveBaseKey(const char* psk, uint8_t out[32]);
/// K_sess = HMAC-SHA256(K_base, nonce_c ‖ nonce_s)
void deriveSessionKey(const uint8_t base[32], const uint8_t nonceC[8], const uint8_t nonceS[8], uint8_t out[32]);

// ---------------------------------------------------------------- packet codecs
//
// Every encode appends the 8-byte truncated MAC over header ‖ payload and returns the total length, or 0 if the
// buffer is too small. Every decode verifies magic, version and MAC before touching the payload, so a caller that
// gets `true` back has authenticated bytes in hand.

size_t encodeHello   (uint8_t* buf, size_t cap, const Header& h, const Hello& p,    const uint8_t key[32]);
size_t encodeHelloAck(uint8_t* buf, size_t cap, const Header& h, const HelloAck& p, const uint8_t key[32]);
size_t encodeNack    (uint8_t* buf, size_t cap, const Header& h, const Nack& p,     const uint8_t key[32]);
size_t encodeKey     (uint8_t* buf, size_t cap, const Header& h, const KeyPkt& p,   const uint8_t key[32]);
size_t encodeStats   (uint8_t* buf, size_t cap, const Header& h, const Stats& p,    const uint8_t key[32]);
size_t encodeBye     (uint8_t* buf, size_t cap, const Header& h,                    const uint8_t key[32]);

/// Reads magic, version and the header fields. Does NOT verify the MAC — use verifyMac() or one of the decoders.
bool peekHeader(const uint8_t* buf, size_t len, Header& h);
/// Constant-time comparison of the trailing MAC against HMAC(key, everything before it).
bool verifyMac(const uint8_t* buf, size_t len, const uint8_t key[32]);

bool decodeHello   (const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, Hello& p);
bool decodeHelloAck(const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, HelloAck& p);
bool decodeNack    (const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, Nack& p);
bool decodeKey     (const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, KeyPkt& p);
bool decodeStats   (const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, Stats& p);
bool decodeBye     (const uint8_t* buf, size_t len, const uint8_t key[32], Header& h);

// ---------------------------------------------------------------- replay window (spec §9)
//
// A KEY packet is accepted only if its seq is within 64 of the highest seen (either side, so reordering is fine)
// and has not been seen before. Edges are additionally deduplicated by timestamp, so a replayed packet cannot
// disturb the reconstruction even if it slipped through here.

class ReplayWindow {
public:
    ReplayWindow() { reset(); }
    void reset();
    /// true = fresh and accepted; false = duplicate or outside the window.
    bool accept(uint16_t seq);
    uint16_t highest() const { return highest_; }
    /// Packets counted as lost: the gaps below `highest` still unfilled in the current window.
    uint32_t missingInWindow() const;
private:
    uint16_t highest_;
    uint64_t seen_;         // bit i = (highest - i) has been seen; bit 0 is `highest` itself
    bool     started_;
};


// ---------------------------------------------------------------- speed estimate (spec §7.5)
//
// The Rig unit must know when the operator is between words rather than between elements, because that is the only
// safe moment to nudge the clock offset or shorten the playout delay. It cannot use the `wpm` field: that is 0 for a
// straight key or bug. So it keeps its own dit estimate from the edge stream, the same idea the M32 decoder uses.

class SpeedEstimator {
public:
    static const uint8_t WINDOW = 16;               // last 16 marks
    static const uint32_t DEFAULT_DIT = 600;        // 60 ms = 20 WPM, used until there is an estimate
    static const uint32_t MAX_MARK    = 10000;      // 1 s: longer is tuning, not sending, and is ignored
    static const uint32_t MIN_IDLE    = 3000;       // the 300 ms floor of the idle-gap threshold

    SpeedEstimator() { reset(); }
    void reset();
    /// Feed the length of a completed mark, in ticks.
    void addMark(uint32_t lenTicks);
    uint32_t ditEst() const { return dit_; }        // ticks
    bool hasEstimate() const { return has_; }
    /// A key-up period of at least this long counts as an idle gap: max(300 ms, 6 x dit).
    uint32_t idleGapTicks() const;
private:
    uint32_t marks_[WINDOW];
    uint8_t  count_, next_;
    uint32_t dit_;
    bool     has_;
};

// ---------------------------------------------------------------- edge queue
//
// Holds edges in **sender** time, not in emission time: the scheduled moment is computed as t + off + D whenever it
// is needed, so a change to D or to the offset shifts the whole timeline at once, which is exactly what spec §7.3
// requires and what keeps relative timing intact when a late edge forces D up.

class EdgeQueue {
public:
    static const uint8_t CAPACITY = 48;
    EdgeQueue() { clear(); }
    void clear() { n_ = 0; }
    uint8_t size() const { return n_; }
    bool empty() const { return n_ == 0; }
    bool full() const  { return n_ >= CAPACITY; }
    /// Sorted insert by edge time. Returns false when the queue is full; sets `duplicate` when an edge with the
    /// same timestamp is already queued (the normal case — redundancy means most edges arrive several times).
    bool insert(const Edge& e, bool& duplicate);
    const Edge& at(uint8_t i) const { return q_[i]; }
    const Edge& front() const { return q_[0]; }
    void popFront();
    void removeAt(uint8_t i);
private:
    Edge    q_[CAPACITY];
    uint8_t n_;
};

// ---------------------------------------------------------------- Rig unit reconstruction (spec §7, §8)

struct RigConfig {
    uint16_t dDefaultMs;
    uint16_t dMinMs, dMaxMs;
    uint16_t safetyMs;
    uint16_t maxKeydownKeyerMs;     // source 0
    uint16_t maxKeydownManualMs;    // sources 1 and 2
    uint16_t keepaliveTimeoutMs;
    bool     adaptive;
    RigConfig()
        : dDefaultMs(150), dMinMs(40), dMaxMs(600), safetyMs(20),
          maxKeydownKeyerMs(3000), maxKeydownManualMs(10000),
          keepaliveTimeoutMs(1000), adaptive(true) {}
};

enum RigActionType {
    RIG_NONE = 0,
    RIG_EMIT,           // write the key line to `state`
    RIG_FORCE_KEYUP,    // safety: key up now, whatever the stream says
    RIG_DROP            // tear the session down
};

struct RigAction {
    RigActionType type;
    uint8_t       state;    // for RIG_EMIT
    uint32_t      at;       // rig-clock time the action belongs to
    RigAction() : type(RIG_NONE), state(KEY_UP), at(0) {}
};

struct RigCounters {
    uint32_t packets, replays, edges, duplicates, late, underruns, protocolErrors, overflows, keydownLimits;
    uint32_t maxLate;       // ticks: how late the worst edge was. The stretch a burst puts on one mark.
    RigCounters() : packets(0), replays(0), edges(0), duplicates(0), late(0), underruns(0),
                    protocolErrors(0), overflows(0), keydownLimits(0), maxLate(0) {}
};

class RigSession {
public:
    RigSession() { RigConfig c; begin(0, c, SRC_KEYER); }
    void begin(uint32_t rigNow, const RigConfig& cfg, uint8_t source);
    /// A KEY packet whose MAC the transport has already verified. Returns false if the replay window refused it.
    bool onKey(const Header& h, const KeyPkt& pkt, uint32_t rigNow);
    /// Drain the actions that are due. Call repeatedly until it returns false; each call yields at most one action.
    bool poll(uint32_t rigNow, RigAction& out);

    bool     hasPending() const   { return !queue_.empty(); }
    /// Rig-clock time the head of the queue is due. Only meaningful while hasPending().
    uint32_t nextEmitTime() const;
    uint8_t  keyState() const     { return keyState_; }
    uint32_t playoutTicks() const { return d_; }
    uint32_t jitterTicks() const  { return jitter_; }
    uint32_t offset() const       { return off_; }
    uint16_t maxKeydownMs() const;
    const SpeedEstimator& speed() const { return speed_; }
    const RigCounters& counters() const { return counters_; }
    void fillStats(Stats& s, uint32_t rigNow) const;
    bool alive() const { return alive_; }

private:
    uint32_t emitTimeOf(const Edge& e) const { return e.t + off_ + d_; }
    void     trackArrival(uint32_t tNow, uint32_t rigNow);
    void     acceptEdge(const Edge& e, uint32_t rigNow);
    void     housekeeping(uint32_t rigNow);
    void     applyPendingD();
    bool     inIdleGap(uint32_t rigNow) const;

    RigConfig cfg_;
    EdgeQueue queue_;
    SpeedEstimator speed_;
    ReplayWindow  replay_;
    RigCounters   counters_;

    uint32_t off_, d_;
    uint32_t dPending_;             // a playout increase held back until the key is up (see acceptEdge)
    uint32_t jitter_;               // ~3 sigma, ticks
    uint32_t jAvg_;                 // EWMA of |d - dMin|, ticks
    int32_t  curMin_, prevMin_;     // two-bucket sliding minimum of the arrival deviation
    uint32_t bucketStart_;
    bool     curValid_, prevValid_;

    uint8_t  source_;
    uint8_t  keyState_;
    uint32_t keyDownSince_;         // rig clock
    uint32_t lastKeyUpAt_;          // rig clock, for the idle-gap test
    uint32_t markStartSender_;      // sender clock, to measure mark length for the speed estimate
    bool     markAborted_;          // the max-keydown limit cut this mark short; swallow its key-up
    bool     haveEmitted_;
    uint32_t lastEmittedT_;         // sender clock
    uint8_t  lastEmittedState_;

    uint32_t lastPacketAt_;         // rig clock
    uint32_t lastEcho_;             // sender clock of the newest accepted KEY
    bool     started_, alive_, watchdogTripped_, keydownLimitHit_;
    uint32_t lastLateAt_;
    bool     offAdjustedThisGap_;   // the clock offset moves at most once per idle gap
    uint32_t lastDecreaseAt_;
    uint32_t lossWindowStart_;
    uint32_t lossAccepted_;
    uint16_t lossFirstSeq_;
    uint8_t  lossPct_;
};

// ---------------------------------------------------------------- Keyer unit packet construction (spec §6.4, §10)

class KeyerSession {
public:
    KeyerSession() { begin(4); }
    /// R = requested redundancy, clamped to 1..MAX_EDGES.
    void begin(uint8_t redundancy);
    void reset() { n_ = 0; }
    /// Record an edge at the capture point. Newest last; only the last R are kept.
    void addEdge(uint32_t t, uint8_t state);
    uint8_t historyCount() const { return n_; }
    uint8_t redundancy() const { return r_; }

    /// Build a KEY packet.
    ///
    /// `newEdge` true: this packet reports the edge just added, and repeats the R-1 before it.
    /// `newEdge` false: a keepalive. Spec §6.4 calls a keepalive "n = 0" in one sentence and says it "carries the
    /// last R-1 edges once more" in the next; those cannot both hold. We resolve it the way the redundancy argument
    /// demands — a keepalive repeats the last R-1 edges, and n is 0 only when there is no history yet — and have
    /// noted it for Draft 0.3. Reading it the other way would throw away the cheap extra redundancy after the end
    /// of a word, which is precisely where the spec says it is wanted.
    size_t buildKey(uint8_t* buf, size_t cap, uint32_t tNow, uint8_t wpm, uint8_t source,
                    uint32_t session, uint16_t seq, const uint8_t key[32], bool newEdge);
private:
    Edge    hist_[MAX_EDGES];       // newest last
    uint8_t n_, r_;
};

} // namespace M32Kip
#endif /* M32KIP_H_ */
