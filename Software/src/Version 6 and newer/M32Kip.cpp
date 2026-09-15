/******************************************************************************************************************************
 *  M32KIP protocol core — crypto, packet codecs, replay window. See M32Kip.h for the contract and devdocs/m32kip/ for the
 *  specification. Host-testable: no Arduino, FreeRTOS or ESP-IDF dependency.
 *****************************************************************************************************************************/

#include "M32Kip.h"
#include <string.h>

namespace M32Kip {

// ============================================================ SHA-256
//
// Self-contained rather than mbedTLS, so that the firmware and the host tests compute byte-identical MACs from the
// same source, and the core stays free of platform headers. Cost on the ESP32 is ~30 µs for a packet-sized HMAC,
// which runs in the network task, off the timing-critical path (spec §9). If profiling ever wants the hardware
// accelerator, replace these two functions — nothing else knows how the digest is produced.

namespace {

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitLen;
    uint8_t  buf[64];
    size_t   bufLen;
};

inline uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

const uint32_t K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

void shaCompress(Sha256Ctx& c, const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) | ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i-15], 7) ^ ror32(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = ror32(w[i-2], 17) ^ ror32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = c.state[0], b = c.state[1], cc = c.state[2], d = c.state[3];
    uint32_t e = c.state[4], f = c.state[5], g = c.state[6], h = c.state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c.state[0] += a; c.state[1] += b; c.state[2] += cc; c.state[3] += d;
    c.state[4] += e; c.state[5] += f; c.state[6] += g; c.state[7] += h;
}

void shaInit(Sha256Ctx& c) {
    c.state[0] = 0x6a09e667u; c.state[1] = 0xbb67ae85u; c.state[2] = 0x3c6ef372u; c.state[3] = 0xa54ff53au;
    c.state[4] = 0x510e527fu; c.state[5] = 0x9b05688cu; c.state[6] = 0x1f83d9abu; c.state[7] = 0x5be0cd19u;
    c.bitLen = 0; c.bufLen = 0;
}

void shaUpdate(Sha256Ctx& c, const uint8_t* data, size_t len) {
    c.bitLen += (uint64_t)len * 8;
    while (len) {
        size_t take = 64 - c.bufLen;
        if (take > len) take = len;
        memcpy(c.buf + c.bufLen, data, take);
        c.bufLen += take; data += take; len -= take;
        if (c.bufLen == 64) { shaCompress(c, c.buf); c.bufLen = 0; }
    }
}

void shaFinal(Sha256Ctx& c, uint8_t out[32]) {
    uint64_t bits = c.bitLen;
    uint8_t pad = 0x80;
    shaUpdate(c, &pad, 1);
    c.bitLen = bits;                                   // padding is not message content
    uint8_t zero = 0x00;
    while (c.bufLen != 56) { shaUpdate(c, &zero, 1); c.bitLen = bits; }
    uint8_t lenBe[8];
    for (int i = 0; i < 8; i++) lenBe[i] = (uint8_t)(bits >> (56 - 8*i));
    shaUpdate(c, lenBe, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(c.state[i] >> 24);
        out[i*4+1] = (uint8_t)(c.state[i] >> 16);
        out[i*4+2] = (uint8_t)(c.state[i] >> 8);
        out[i*4+3] = (uint8_t)(c.state[i]);
    }
}

// ---- little-endian helpers ----
inline void put8 (uint8_t* b, size_t& o, uint8_t v)  { b[o++] = v; }
inline void put16(uint8_t* b, size_t& o, uint16_t v) { b[o++] = (uint8_t)v; b[o++] = (uint8_t)(v >> 8); }
inline void put32(uint8_t* b, size_t& o, uint32_t v) { b[o++] = (uint8_t)v; b[o++] = (uint8_t)(v >> 8);
                                                       b[o++] = (uint8_t)(v >> 16); b[o++] = (uint8_t)(v >> 24); }
inline uint8_t  get8 (const uint8_t* b, size_t& o) { return b[o++]; }
inline uint16_t get16(const uint8_t* b, size_t& o) { uint16_t v = (uint16_t)b[o] | ((uint16_t)b[o+1] << 8); o += 2; return v; }
inline uint32_t get32(const uint8_t* b, size_t& o) { uint32_t v = (uint32_t)b[o] | ((uint32_t)b[o+1] << 8) |
                                                                  ((uint32_t)b[o+2] << 16) | ((uint32_t)b[o+3] << 24); o += 4; return v; }

void putHeader(uint8_t* b, size_t& o, const Header& h) {
    put8(b, o, MAGIC); put8(b, o, VERSION); put8(b, o, h.type); put8(b, o, h.flags);
    put32(b, o, h.session); put16(b, o, h.seq); put16(b, o, 0);     // reserved
}

/// Appends the truncated MAC over the first `o` bytes and returns the total length.
size_t seal(uint8_t* b, size_t cap, size_t o, const uint8_t key[32]) {
    if (o + MAC_LEN > cap) return 0;
    uint8_t mac[32];
    hmacSha256(key, 32, b, o, mac);
    memcpy(b + o, mac, MAC_LEN);
    return o + MAC_LEN;
}

/// Verifies magic, version, length and MAC, and fills the header. `payloadLen` is what the caller expects to find
/// between the header and the MAC; pass SIZE_MAX to accept any length (KEY packets are variable).
bool openPacket(const uint8_t* b, size_t len, const uint8_t key[32], uint8_t type, size_t payloadLen, Header& h) {
    if (!peekHeader(b, len, h)) return false;
    if (h.type != type) return false;
    if (payloadLen != (size_t)-1 && len != HEADER_LEN + payloadLen + MAC_LEN) return false;
    return verifyMac(b, len, key);
}

} // anonymous namespace

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    Sha256Ctx c; shaInit(c); shaUpdate(c, data, len); shaFinal(c, out);
}

void hmacSha256(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t len, uint8_t out[32]) {
    uint8_t k[64];
    memset(k, 0, sizeof(k));
    if (keyLen > 64) sha256(key, keyLen, k);
    else             memcpy(k, key, keyLen);
    uint8_t inner[64], outer[64];
    for (int i = 0; i < 64; i++) { inner[i] = k[i] ^ 0x36; outer[i] = k[i] ^ 0x5c; }
    uint8_t ih[32];
    Sha256Ctx c;
    shaInit(c); shaUpdate(c, inner, 64); shaUpdate(c, data, len); shaFinal(c, ih);
    shaInit(c); shaUpdate(c, outer, 64); shaUpdate(c, ih, 32);    shaFinal(c, out);
}

void deriveBaseKey(const char* psk, uint8_t out[32]) {
    sha256((const uint8_t*)psk, strlen(psk), out);
}

void deriveSessionKey(const uint8_t base[32], const uint8_t nonceC[8], const uint8_t nonceS[8], uint8_t out[32]) {
    uint8_t material[16];
    memcpy(material, nonceC, 8);
    memcpy(material + 8, nonceS, 8);
    hmacSha256(base, 32, material, sizeof(material), out);
}

// ============================================================ packet codecs

size_t encodeHello(uint8_t* buf, size_t cap, const Header& h, const Hello& p, const uint8_t key[32]) {
    if (cap < HEADER_LEN + 16 + MAC_LEN) return 0;
    size_t o = 0;
    Header hh = h; hh.type = PKT_HELLO;
    putHeader(buf, o, hh);
    memcpy(buf + o, p.nonceC, 8); o += 8;
    put32(buf, o, p.tNow);
    put8(buf, o, p.wpm); put8(buf, o, p.redundancy); put8(buf, o, p.source); put8(buf, o, p.caps);
    return seal(buf, cap, o, key);
}

size_t encodeHelloAck(uint8_t* buf, size_t cap, const Header& h, const HelloAck& p, const uint8_t key[32]) {
    if (cap < HEADER_LEN + 24 + MAC_LEN) return 0;
    size_t o = 0;
    Header hh = h; hh.type = PKT_HELLO_ACK;
    putHeader(buf, o, hh);
    memcpy(buf + o, p.nonceC, 8); o += 8;
    memcpy(buf + o, p.nonceS, 8); o += 8;
    put32(buf, o, p.session);
    put16(buf, o, p.maxKeydownMs);
    put8(buf, o, p.pttLeadMs); put8(buf, o, p.flags);
    return seal(buf, cap, o, key);
}

size_t encodeNack(uint8_t* buf, size_t cap, const Header& h, const Nack& p, const uint8_t key[32]) {
    if (cap < HEADER_LEN + 2 + MAC_LEN) return 0;
    size_t o = 0;
    Header hh = h; hh.type = PKT_NACK;
    putHeader(buf, o, hh);
    put8(buf, o, p.reason); put8(buf, o, 0);
    return seal(buf, cap, o, key);
}

size_t encodeKey(uint8_t* buf, size_t cap, const Header& h, const KeyPkt& p, const uint8_t key[32]) {
    if (p.n > MAX_EDGES) return 0;
    if (cap < HEADER_LEN + 8 + (size_t)p.n * EDGE_LEN + MAC_LEN) return 0;
    size_t o = 0;
    Header hh = h; hh.type = PKT_KEY;
    putHeader(buf, o, hh);
    put32(buf, o, p.tNow);
    put8(buf, o, p.wpm); put8(buf, o, p.n); put8(buf, o, p.source); put8(buf, o, 0);
    for (uint8_t i = 0; i < p.n; i++) { put32(buf, o, p.edges[i].t); put8(buf, o, p.edges[i].state); }
    return seal(buf, cap, o, key);
}

size_t encodeStats(uint8_t* buf, size_t cap, const Header& h, const Stats& p, const uint8_t key[32]) {
    if (cap < HEADER_LEN + 20 + MAC_LEN) return 0;
    size_t o = 0;
    Header hh = h; hh.type = PKT_STATS;
    putHeader(buf, o, hh);
    put16(buf, o, p.playoutMs); put16(buf, o, p.jitter100us);
    put8(buf, o, p.lossPct); put8(buf, o, p.lateEdges); put8(buf, o, p.underruns); put8(buf, o, p.rigState);
    put16(buf, o, p.maxKeydownMs); put16(buf, o, p.ditEst100us);
    put32(buf, o, p.tEcho); put32(buf, o, p.tRig);
    return seal(buf, cap, o, key);
}

size_t encodeBye(uint8_t* buf, size_t cap, const Header& h, const uint8_t key[32]) {
    if (cap < HEADER_LEN + MAC_LEN) return 0;
    size_t o = 0;
    Header hh = h; hh.type = PKT_BYE;
    putHeader(buf, o, hh);
    return seal(buf, cap, o, key);
}

bool peekHeader(const uint8_t* buf, size_t len, Header& h) {
    if (len < HEADER_LEN + MAC_LEN) return false;
    if (buf[0] != MAGIC || buf[1] != VERSION) return false;
    size_t o = 2;
    h.type  = get8(buf, o);
    h.flags = get8(buf, o);
    h.session = get32(buf, o);
    h.seq   = get16(buf, o);
    return true;
}

bool verifyMac(const uint8_t* buf, size_t len, const uint8_t key[32]) {
    if (len < HEADER_LEN + MAC_LEN) return false;
    uint8_t mac[32];
    hmacSha256(key, 32, buf, len - MAC_LEN, mac);
    uint8_t diff = 0;                                   // constant time: no early exit on the first wrong byte
    for (size_t i = 0; i < MAC_LEN; i++) diff |= (uint8_t)(mac[i] ^ buf[len - MAC_LEN + i]);
    return diff == 0;
}

bool decodeHello(const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, Hello& p) {
    if (!openPacket(buf, len, key, PKT_HELLO, 16, h)) return false;
    size_t o = HEADER_LEN;
    memcpy(p.nonceC, buf + o, 8); o += 8;
    p.tNow = get32(buf, o);
    p.wpm = get8(buf, o); p.redundancy = get8(buf, o); p.source = get8(buf, o); p.caps = get8(buf, o);
    return true;
}

bool decodeHelloAck(const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, HelloAck& p) {
    if (!openPacket(buf, len, key, PKT_HELLO_ACK, 24, h)) return false;
    size_t o = HEADER_LEN;
    memcpy(p.nonceC, buf + o, 8); o += 8;
    memcpy(p.nonceS, buf + o, 8); o += 8;
    p.session = get32(buf, o);
    p.maxKeydownMs = get16(buf, o);
    p.pttLeadMs = get8(buf, o); p.flags = get8(buf, o);
    return true;
}

bool decodeNack(const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, Nack& p) {
    if (!openPacket(buf, len, key, PKT_NACK, 2, h)) return false;
    size_t o = HEADER_LEN;
    p.reason = get8(buf, o);
    return true;
}

bool decodeKey(const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, KeyPkt& p) {
    if (!openPacket(buf, len, key, PKT_KEY, (size_t)-1, h)) return false;
    if (len < HEADER_LEN + 8 + MAC_LEN) return false;
    size_t o = HEADER_LEN;
    p.tNow = get32(buf, o);
    p.wpm = get8(buf, o);
    p.n = get8(buf, o);
    p.source = get8(buf, o);
    (void)get8(buf, o);                                 // reserved
    if (p.n > MAX_EDGES) return false;
    if (len != HEADER_LEN + 8 + (size_t)p.n * EDGE_LEN + MAC_LEN) return false;
    for (uint8_t i = 0; i < p.n; i++) {
        p.edges[i].t = get32(buf, o);
        p.edges[i].state = get8(buf, o);
        if (p.edges[i].state > 1) return false;
    }
    return true;
}

bool decodeStats(const uint8_t* buf, size_t len, const uint8_t key[32], Header& h, Stats& p) {
    if (!openPacket(buf, len, key, PKT_STATS, 20, h)) return false;
    size_t o = HEADER_LEN;
    p.playoutMs = get16(buf, o); p.jitter100us = get16(buf, o);
    p.lossPct = get8(buf, o); p.lateEdges = get8(buf, o); p.underruns = get8(buf, o); p.rigState = get8(buf, o);
    p.maxKeydownMs = get16(buf, o); p.ditEst100us = get16(buf, o);
    p.tEcho = get32(buf, o); p.tRig = get32(buf, o);
    return true;
}

bool decodeBye(const uint8_t* buf, size_t len, const uint8_t key[32], Header& h) {
    return openPacket(buf, len, key, PKT_BYE, 0, h);
}

// ============================================================ replay window

void ReplayWindow::reset() { highest_ = 0; seen_ = 0; started_ = false; }

bool ReplayWindow::accept(uint16_t seq) {
    if (!started_) { started_ = true; highest_ = seq; seen_ = 1; return true; }
    int32_t d = (int16_t)(seq - highest_);              // signed 16-bit difference: handles the wrap
    if (d > 0) {
        if (d >= 64) { highest_ = seq; seen_ = 1; return true; }   // jumped clear of the window: restart it
        seen_ <<= d;
        seen_ |= 1;
        highest_ = seq;
        return true;
    }
    if (d <= -64) return false;                         // too old to judge
    uint64_t bit = (uint64_t)1 << (uint32_t)(-d);
    if (seen_ & bit) return false;                      // already seen
    seen_ |= bit;
    return true;
}

uint32_t ReplayWindow::missingInWindow() const {
    if (!started_) return 0;
    uint32_t missing = 0;
    for (int i = 0; i < 64; i++)
        if (!(seen_ & ((uint64_t)1 << i))) missing++;
    return missing;
}


// ============================================================ speed estimate (spec §7.5)

void SpeedEstimator::reset() {
    count_ = 0; next_ = 0; dit_ = DEFAULT_DIT; has_ = false;
    for (uint8_t i = 0; i < WINDOW; i++) marks_[i] = 0;
}

void SpeedEstimator::addMark(uint32_t lenTicks) {
    if (lenTicks == 0 || lenTicks > MAX_MARK) return;       // a tuning carrier says nothing about sending speed
    marks_[next_] = lenTicks;
    next_ = (uint8_t)((next_ + 1) % WINDOW);
    if (count_ < WINDOW) count_++;

    // The "short" cluster is everything below 0.6 x the longest mark in the window: dits when dahs are present,
    // and nothing at all when only one length has been seen (only dahs so far, or a single element). In that case
    // the previous estimate stands, exactly as the spec asks.
    uint32_t longest = 0;
    for (uint8_t i = 0; i < count_; i++) if (marks_[i] > longest) longest = marks_[i];
    uint32_t threshold = (longest * 6) / 10;

    uint32_t shortMarks[WINDOW];
    uint8_t  m = 0;
    for (uint8_t i = 0; i < count_; i++) if (marks_[i] < threshold) shortMarks[m++] = marks_[i];
    if (m == 0) {
        // One cluster only: nothing here distinguishes a dit from a dah, so the previous estimate stands —
        // and if there has never been one, DEFAULT_DIT (60 ms) does. Adopting the longest mark instead would
        // let a run of dahs, or a tuning carrier, quietly redefine the dit as three times its true length.
        return;
    }
    for (uint8_t i = 1; i < m; i++) {                       // insertion sort: at most 16 values
        uint32_t v = shortMarks[i];
        int8_t j = (int8_t)(i - 1);
        while (j >= 0 && shortMarks[j] > v) { shortMarks[j + 1] = shortMarks[j]; j--; }
        shortMarks[j + 1] = v;
    }
    dit_ = shortMarks[m / 2];
    has_ = true;
}

uint32_t SpeedEstimator::idleGapTicks() const {
    uint32_t six = dit_ * 6;
    return six > MIN_IDLE ? six : MIN_IDLE;
}

// ============================================================ edge queue

bool EdgeQueue::insert(const Edge& e, bool& duplicate) {
    duplicate = false;
    uint8_t i = 0;
    while (i < n_ && tickDiff(q_[i].t, e.t) < 0) i++;
    if (i < n_ && q_[i].t == e.t) { duplicate = true; return true; }
    if (n_ >= CAPACITY) return false;
    for (uint8_t j = n_; j > i; j--) q_[j] = q_[j - 1];
    q_[i] = e;
    n_++;
    return true;
}

void EdgeQueue::popFront() {
    if (n_ == 0) return;
    for (uint8_t i = 1; i < n_; i++) q_[i - 1] = q_[i];
    n_--;
}

void EdgeQueue::removeAt(uint8_t i) {
    if (i >= n_) return;
    for (uint8_t j = (uint8_t)(i + 1); j < n_; j++) q_[j - 1] = q_[j];
    n_--;
}

// ============================================================ Rig unit reconstruction

void RigSession::begin(uint32_t rigNow, const RigConfig& cfg, uint8_t source) {
    cfg_ = cfg;
    queue_.clear();
    speed_.reset();
    replay_.reset();
    counters_ = RigCounters();

    if (cfg_.firstExtMs) {
        // An advanced key-down is due earlier than its timestamp says, which eats into the margin the playout delay
        // keeps against network jitter. Give the margin back by raising the floor and the start value by the same.
        uint16_t dMin = (uint16_t)(cfg_.dMinMs + cfg_.firstExtMs);
        uint16_t dDef = (uint16_t)(cfg_.dDefaultMs + cfg_.firstExtMs);
        cfg_.dMinMs     = dMin > cfg_.dMaxMs ? cfg_.dMaxMs : dMin;
        cfg_.dDefaultMs = dDef > cfg_.dMaxMs ? cfg_.dMaxMs : dDef;
    }
    off_ = 0;
    d_ = msToTicks(cfg_.dDefaultMs);
    dPending_ = 0;
    jitter_ = 0; jAvg_ = 0;
    curMin_ = prevMin_ = 0; bucketStart_ = rigNow; curValid_ = prevValid_ = false;

    source_ = source;
    keyState_ = KEY_UP;
    keyDownSince_ = rigNow;
    lastKeyUpAt_ = rigNow;
    markStartSender_ = 0;
    markAborted_ = false;
    haveEmitted_ = false;
    lastEmittedT_ = 0;
    lastEmittedState_ = KEY_UP;

    lastPacketAt_ = rigNow;
    lastEcho_ = 0;
    started_ = false; alive_ = true; watchdogTripped_ = false; keydownLimitHit_ = false;
    lastLateAt_ = rigNow;
    offAdjustedThisGap_ = false;
    dropPending_ = false;
    dropReason_ = DROP_NONE;
    errWindowStart_ = rigNow;
    errInWindow_ = 0;
    rejNext_ = rejCount_ = 0;
    lastDecreaseAt_ = rigNow;
    lossWindowStart_ = rigNow;
    lossAccepted_ = 0;
    lossFirstSeq_ = 0;
    lossPct_ = 0;
}

uint16_t RigSession::maxKeydownMs() const {
    return (source_ == SRC_KEYER) ? cfg_.maxKeydownKeyerMs : cfg_.maxKeydownManualMs;
}

uint32_t RigSession::nextEmitTime() const {
    return queue_.empty() ? 0 : emitTimeOf(queue_.front());
}

uint32_t RigSession::hangTicks() const {
    if (cfg_.hangDitsX2) {
        // Until there is a speed estimate - early in a session, or while only one mark length has been seen -
        // ditEst() is the estimator's 60 ms default (20 WPM). Counting every gap as a changeover instead would
        // lengthen every mark inside every word; a wrong default speed misjudges at most one element per pause.
        return speed_.ditEst() * cfg_.hangDitsX2 / 2;
    }
    return msToTicks(cfg_.hangMs);
}

uint32_t RigSession::breakInAdvance(const Edge& e, bool havePrev, uint32_t prevT) const {
    if (e.state != KEY_DOWN || cfg_.firstExtMs == 0) return 0;
    uint32_t adv = msToTicks(cfg_.firstExtMs);
    if (!havePrev) return adv;                      // the session's first mark: the transceiver is surely receiving
    int32_t gap = tickDiff(e.t, prevT);             // sender clock: pure keying timing, untouched by the network
    if (gap <= 0) return 0;
    if ((uint32_t)gap < hangTicks()) return 0;      // still inside the transceiver's hang: it is transmitting
    if (adv > (uint32_t)gap / 2) adv = (uint32_t)gap / 2;   // never move a key-down across the gap before it
    return adv;
}

/// Offset, sliding minimum and jitter, from the sender timestamp each packet carries (spec §7.2).
void RigSession::trackArrival(uint32_t tNow, uint32_t rigNow) {
    if (!started_) {
        started_ = true;
        off_ = rigNow - tNow;                       // by construction the first packet has deviation 0
        curMin_ = 0; curValid_ = true; bucketStart_ = rigNow;
        return;
    }
    int32_t d = tickDiff(rigNow, tNow + off_);

    // Sliding minimum over ~5 s, as two 2.5 s buckets: cheap, bounded, and it forgets an old fast path
    // instead of holding onto it for ever.
    if (tickDiff(rigNow, bucketStart_) >= (int32_t)msToTicks(2500)) {
        prevMin_ = curMin_; prevValid_ = curValid_;
        curMin_ = d; curValid_ = true; bucketStart_ = rigNow;
    } else if (!curValid_ || d < curMin_) {
        curMin_ = d; curValid_ = true;
    }
    int32_t dMin = curMin_;
    if (prevValid_ && prevMin_ < dMin) dMin = prevMin_;

    int32_t excess = d - dMin;
    if (excess < 0) excess = 0;
    jAvg_ = jAvg_ + (uint32_t)(((int32_t)excess - (int32_t)jAvg_) >> 3);     // EWMA, alpha = 1/8
    jitter_ = jAvg_ * 3;                                                     // ~3 sigma
}

bool RigSession::onKey(const Header& h, const KeyPkt& pkt, uint32_t rigNow) {
    if (!alive_) return false;
    if (!replay_.accept(h.seq)) { counters_.replays++; return false; }

    counters_.packets++;
    lossAccepted_++;
    lastPacketAt_ = rigNow;
    watchdogTripped_ = false;
    lastEcho_ = pkt.tNow;

    if (pkt.source != source_) {
        // The operator switched keyer mode mid-session (spec §6.4). The mark limit follows on the next idle gap;
        // the speed estimate is worthless across the change, so it starts again.
        source_ = pkt.source;
        speed_.reset();
    }

    trackArrival(pkt.tNow, rigNow);
    for (uint8_t i = 0; i < pkt.n; i++) acceptEdge(pkt.edges[i], rigNow);

    // A storm of malformed edges - more than ten in one second (spec §8) - is corruption or an attack, and
    // the safe answer is to stop. It is a RATE: the first version counted over the whole session, so a
    // long and perfectly healthy session would have died on its 32nd stray edge.
    if (errInWindow_ > 10) die(DROP_ERRORS);
    return true;
}

void RigSession::acceptEdge(const Edge& e, uint32_t rigNow) {
    counters_.edges++;

    if (haveEmitted_ && tickDiff(e.t, lastEmittedT_) <= 0) { counters_.duplicates++; return; }
    for (uint8_t i = 0; i < rejCount_; i++)             // a redundant copy of an edge already refused: redundancy
        if (rejected_[i] == e.t) { counters_.duplicates++; return; }   // must not multiply one fault into many

    // State consistency against the neighbours it would sit between (spec §7.4.4): the earlier edge wins.
    uint8_t pos = 0;
    while (pos < queue_.size() && tickDiff(queue_.at(pos).t, e.t) < 0) pos++;
    if (pos < queue_.size() && queue_.at(pos).t == e.t) { counters_.duplicates++; return; }

    uint8_t prevState;
    bool havePrev = false;
    if (pos > 0)          { prevState = queue_.at((uint8_t)(pos - 1)).state; havePrev = true; }
    else if (haveEmitted_){ prevState = lastEmittedState_;                   havePrev = true; }
    else                  { prevState = KEY_UP; }
    if (havePrev && prevState == e.state) { noteProtocolError(e, rigNow, true); return; }
    if (!haveEmitted_ && queue_.empty() && e.state != KEY_DOWN) {
        noteProtocolError(e, rigNow, true);          // the first edge of a session must be a key-down
        return;
    }
    if (pos < queue_.size() && queue_.at(pos).state == e.state) {
        queue_.removeAt(pos);                       // the incoming edge is earlier, so it is the authoritative one
        noteProtocolError(e, rigNow, false);
    }

    uint32_t prevT = 0;                                 // the edge this one follows, for break-in compensation (D16)
    if (pos > 0)          prevT = queue_.at((uint8_t)(pos - 1)).t;
    else if (haveEmitted_) prevT = lastEmittedT_;
    uint32_t tEmit = e.t + off_ + d_ - breakInAdvance(e, havePrev, prevT);
    int32_t lateBySigned = tickDiff(rigNow, tEmit);     // positive means the edge is overdue (spec §7.4.3)
    // Only a genuinely overdue edge is late. Spec §7.4.2 writes the test against `rig_now + slack`, which reads
    // as though an edge arriving half a millisecond BEFORE it is due were late by a negative amount; computing
    // L that way underflowed the unsigned subtraction and reported a lateness of four billion ticks, which
    // pinned the playout delay to its ceiling. The slack only ever meant "do not bother arming a timer for
    // something this close" — poll() emits anything already due — so the lateness test needs no slack at all.
    if (lateBySigned > 0) {
        counters_.late++;
        lastLateAt_ = rigNow;
        uint32_t lateBy = (uint32_t)lateBySigned;
        if (lateBy > counters_.maxLate) counters_.maxLate = lateBy;
        if (e.state == KEY_UP && lateBy <= msToTicks(2)) {
            // Stretch the mark by up to 2 ms rather than shift the whole timeline for it. Marks may be
            // lengthened, never shortened (design principle 5).
        } else {
            // Grow the playout delay so the edges after this one are not late too. Shifting D shifts every
            // queued edge with it, which preserves their relative timing — but only if the shift lands in a
            // gap. Applied while the key is DOWN it would move the queued key-up later and leave the already
            // emitted key-down where it was, stretching that one mark by the whole increase: at 35 WPM the
            // simulation showed a dah coming out 50 ms long, which is a different element, not a late one.
            // So while a mark is in progress the increase waits for its key-up (spec §7.3 says to shift the
            // timeline, without saying when; noted for Draft 0.3).
            uint32_t grow = lateBy + msToTicks(10);
            uint32_t maxD = msToTicks(cfg_.dMaxMs);
            if (keyState_ == KEY_DOWN) {
                if (dPending_ + grow > maxD) dPending_ = maxD; else dPending_ += grow;
            } else {
                d_ = (d_ + grow > maxD) ? maxD : d_ + grow;
            }
            counters_.underruns++;
        }
    }

    bool duplicate = false;
    if (!queue_.insert(e, duplicate)) {
        counters_.overflows++;
        die(DROP_OVERFLOW);                         // a full queue means reconstruction has lost the plot
        return;
    }
    if (duplicate) counters_.duplicates++;
}

void RigSession::noteProtocolError(const Edge& e, uint32_t rigNow, bool refused) {
    counters_.protocolErrors++;
    if (tickDiff(rigNow, errWindowStart_) >= (int32_t)msToTicks(1000)) {
        errWindowStart_ = rigNow;
        errInWindow_ = 0;
    }
    if (errInWindow_ < 0xFFFF) errInWindow_++;
    if (refused) {
        rejected_[rejNext_] = e.t;
        rejNext_ = (uint8_t)((rejNext_ + 1) % 8);
        if (rejCount_ < 8) rejCount_++;
    }
}

void RigSession::die(RigDropReason why) {
    if (!alive_) return;
    alive_ = false;
    dropPending_ = true;                            // poll() lifts the key and reports RIG_DROP
    dropReason_ = (uint8_t)why;
}

void RigSession::applyPendingD() {
    if (!dPending_) return;
    uint32_t maxD = msToTicks(cfg_.dMaxMs);
    d_ = (d_ + dPending_ > maxD) ? maxD : d_ + dPending_;
    dPending_ = 0;
}

bool RigSession::inIdleGap(uint32_t rigNow) const {
    if (keyState_ != KEY_UP) return false;
    if (!queue_.empty()) return false;
    return tickDiff(rigNow, lastKeyUpAt_) >= (int32_t)speed_.idleGapTicks();
}

void RigSession::housekeeping(uint32_t rigNow) {
    if (!inIdleGap(rigNow)) return;

    // Drift: walk the offset toward the fastest observed path, at most 1 ms at a time, and only here — never
    // inside a character, and never while an edge is waiting (spec §7.2).
    int32_t dMin = curMin_;
    if (prevValid_ && prevMin_ < dMin) dMin = prevMin_;
    // One adjustment per idle gap. The spec caps the step at 1 ms but says nothing about how often a step may
    // be taken, and "as often as the caller polls" is not a specification: the simulation took nineteen steps
    // inside a single word gap and moved that gap 19 ms earlier, and on a real Rig the number would depend on
    // the loop rate. One step per gap is ample for the drift this corrects — tens of ppm is a couple of ms a
    // minute, and there are several word gaps in a minute of sending. (Noted for Draft 0.3.)
    if (dMin != 0 && !offAdjustedThisGap_) {
        offAdjustedThisGap_ = true;
        int32_t step = dMin;
        int32_t cap = (int32_t)msToTicks(1);
        if (step >  cap) step =  cap;
        if (step < -cap) step = -cap;
        off_ = off_ + (uint32_t)step;       // d = rigNow - (tNow + off): raising off lowers d toward zero
        curMin_ -= step;                    // the recorded minima are relative to the offset, so they follow it

        if (prevValid_) prevMin_ -= step;
        counters_.offSteps++;
    }

    if (!cfg_.adaptive) return;
    // Shrink D toward the target slowly: 5 ms at a time, at most every 2 s, and only after 10 s with no late edge.
    if (tickDiff(rigNow, lastLateAt_)      < (int32_t)msToTicks(10000)) return;
    if (tickDiff(rigNow, lastDecreaseAt_)  < (int32_t)msToTicks(2000))  return;
    uint32_t target = jitter_ + msToTicks(cfg_.safetyMs);
    uint32_t lo = msToTicks(cfg_.dMinMs), hi = msToTicks(cfg_.dMaxMs);
    if (target < lo) target = lo;
    if (target > hi) target = hi;
    if (d_ > target) {
        uint32_t step = msToTicks(5);
        d_ = (d_ - target < step) ? target : d_ - step;
        lastDecreaseAt_ = rigNow;
        counters_.dLowers++;
    }
}

bool RigSession::poll(uint32_t rigNow, RigAction& out) {
    out = RigAction();
    if (!alive_) {
        // Fail safe = key up (design principle 6). A session that died inside onKey() - an error storm, a
        // full queue - used to go quiet here with the key line exactly where it was, so a mark on the air at
        // that instant stayed on the air. Now its death is reported like any other: key up first, then drop.
        if (!dropPending_) return false;
        if (keyState_ == KEY_DOWN) {
            keyState_ = KEY_UP;
            lastKeyUpAt_ = rigNow;
            out.type = RIG_FORCE_KEYUP; out.state = KEY_UP; out.at = rigNow;
            return true;
        }
        dropPending_ = false;
        out.type = RIG_DROP; out.at = rigNow;
        return true;
    }

    // --- safety first (spec §8) ---
    if (keyState_ == KEY_DOWN) {
        uint32_t limit = msToTicks(maxKeydownMs());
        if (tickDiff(rigNow, keyDownSince_) >= (int32_t)limit) {
            keyState_ = KEY_UP;
            lastKeyUpAt_ = rigNow;
            markAborted_ = true;                    // swallow this mark's key-up when it arrives
            keydownLimitHit_ = true;
            applyPendingD();
            counters_.keydownLimits++;
            out.type = RIG_FORCE_KEYUP; out.state = KEY_UP; out.at = rigNow;
            return true;
        }
    }
    if (tickDiff(rigNow, lastPacketAt_) >= (int32_t)msToTicks(cfg_.keepaliveTimeoutMs)) {
        if (keyState_ == KEY_DOWN) {
            keyState_ = KEY_UP;
            lastKeyUpAt_ = rigNow;
            markAborted_ = true;
            watchdogTripped_ = true;
            applyPendingD();
            out.type = RIG_FORCE_KEYUP; out.state = KEY_UP; out.at = rigNow;
            return true;
        }
        watchdogTripped_ = true;
        if (tickDiff(rigNow, lastPacketAt_) >= (int32_t)msToTicks(cfg_.keepaliveTimeoutMs + 5000)) {
            alive_ = false;
            dropReason_ = DROP_KEEPALIVE;
            out.type = RIG_DROP; out.at = rigNow;
            return true;
        }
    }

    // --- edges that are due ---
    if (!queue_.empty()) {
        Edge e = queue_.front();
        if (tickDiff(rigNow, emitTimeOf(e)) >= 0) {
            queue_.popFront();
            haveEmitted_ = true;
            lastEmittedT_ = e.t;
            lastEmittedState_ = e.state;

            if (e.state == KEY_DOWN) {
                markStartSender_ = e.t;
                keyState_ = KEY_DOWN;
                keyDownSince_ = rigNow;
                markAborted_ = false;
                offAdjustedThisGap_ = false;        // a new gap begins when this mark ends
                out.type = RIG_EMIT; out.state = KEY_DOWN; out.at = rigNow; out.senderT = e.t;
                return true;
            }
            // key-up: the mark just ended, so its length feeds the speed estimate whether or not we emit
            if (markStartSender_ != 0) speed_.addMark((uint32_t)tickDiff(e.t, markStartSender_));
            if (markAborted_) {                     // the limit already lifted the key; nothing to do
                markAborted_ = false;
                applyPendingD();
                return poll(rigNow, out);           // look for the next due edge in the same pass
            }
            keyState_ = KEY_UP;
            lastKeyUpAt_ = rigNow;
            applyPendingD();
            out.type = RIG_EMIT; out.state = KEY_UP; out.at = rigNow; out.senderT = e.t;
            return true;
        }
    }

    housekeeping(rigNow);

    // --- loss estimate over a rolling 10 s ---
    if (tickDiff(rigNow, lossWindowStart_) >= (int32_t)msToTicks(10000)) {
        uint16_t span = (uint16_t)(replay_.highest() - lossFirstSeq_ + 1);
        if (span > 0 && lossAccepted_ <= span) {
            uint32_t lost = span - lossAccepted_;
            lossPct_ = (uint8_t)((lost * 100) / span);
        } else {
            lossPct_ = 0;
        }
        lossWindowStart_ = rigNow;
        lossAccepted_ = 0;
        lossFirstSeq_ = replay_.highest();
    }
    return false;
}

void RigSession::fillStats(Stats& s, uint32_t rigNow) const {
    s.playoutMs    = (uint16_t)ticksToMs(d_);
    s.jitter100us  = (uint16_t)(jitter_ > 65535 ? 65535 : jitter_);
    s.lossPct      = lossPct_;
    s.lateEdges    = (uint8_t)(counters_.late      > 255 ? 255 : counters_.late);
    s.underruns    = (uint8_t)(counters_.underruns > 255 ? 255 : counters_.underruns);
    s.rigState     = (uint8_t)((keyState_ == KEY_DOWN ? RIG_KEY_DOWN : 0)
                             | (watchdogTripped_      ? RIG_WATCHDOG : 0)
                             | (keydownLimitHit_      ? RIG_KEYDOWN_LIMIT : 0));
    s.maxKeydownMs = maxKeydownMs();
    s.ditEst100us  = (uint16_t)(speed_.hasEstimate() ? speed_.ditEst() : 0);
    s.tEcho        = lastEcho_;
    s.tRig         = rigNow;
}

// ============================================================ Keyer unit packet construction

void KeyerSession::begin(uint8_t redundancy, uint16_t repeatMs, uint8_t repeats, uint16_t keepaliveMs) {
    r_ = redundancy;
    if (r_ < 1) r_ = 1;
    if (r_ > MAX_EDGES) r_ = MAX_EDGES;
    n_ = 0;
    repeatMs_ = repeatMs;
    repeats_ = repeats;
    keepaliveMs_ = keepaliveMs;
    repeatsLeft_ = 0;
    nextSendAt_ = 0;
    scheduled_ = false;
}

void KeyerSession::addEdge(uint32_t t, uint8_t state) {
    if (n_ < MAX_EDGES) {
        hist_[n_++] = Edge(t, state);
    } else {
        for (uint8_t i = 1; i < MAX_EDGES; i++) hist_[i - 1] = hist_[i];
        hist_[MAX_EDGES - 1] = Edge(t, state);
    }
    repeatsLeft_ = repeats_;        // this edge starts a fresh run of quick repeats (D13)
}

bool KeyerSession::sendDue(uint32_t tNow) const {
    return scheduled_ && tickDiff(tNow, nextSendAt_) >= 0;
}

void KeyerSession::noteSent(uint32_t tNow) {
    if (repeatsLeft_ > 0) {
        repeatsLeft_--;
        nextSendAt_ = tNow + msToTicks(repeatMs_);
    } else {
        nextSendAt_ = tNow + msToTicks(keepaliveMs_);
    }
    scheduled_ = true;
}

size_t KeyerSession::buildKey(uint8_t* buf, size_t cap, uint32_t tNow, uint8_t wpm, uint8_t source,
                              uint32_t session, uint16_t seq, const uint8_t key[32], bool newEdge) {
    uint8_t want = newEdge ? r_ : (uint8_t)(r_ - 1);
    if (want > n_) want = n_;

    KeyPkt p;
    p.tNow = tNow; p.wpm = wpm; p.source = source; p.n = want;
    for (uint8_t i = 0; i < want; i++) p.edges[i] = hist_[n_ - want + i];   // newest last, as the wire format says

    Header h;
    h.session = session;
    h.seq = seq;
    return encodeKey(buf, cap, h, p, key);
}

// ---------------------------------------------------------------- GlitchFilter

void GlitchFilter::release() {
    if (nReady_ < READY) ready_[nReady_++] = Edge(chatterFrom_, pendState_);   // the caller drains after every push
    level_ = pendState_;
    pending_ = false;
}

void GlitchFilter::push(uint32_t t, uint8_t state) {
    if (pending_ && tickDiff(t, pendT_) >= (int32_t)width_)
        release();                                  // it held until this edge arrived: a real one
    if (pending_) {
        if (state == level_) {                      // reverted inside the window: no edge yet, but maybe chatter
            pending_ = false;
            chatter_ = true;
            lastRevertT_ = t;
        }
        return;                                     // (a repeat of the pending level changes nothing)
    }
    if (state == level_) return;                    // not a transition
    if (!(chatter_ && tickDiff(t, lastRevertT_) < (int32_t)width_))
        chatterFrom_ = t;                           // a fresh contact; inside a burst, keep the burst's first one
    chatter_ = false;
    pending_ = true;
    pendT_ = t;                                     // holding is measured from the latest change
    pendState_ = state;
}

bool GlitchFilter::pop(uint32_t now, uint32_t& t, uint8_t& state) {
    if (!nReady_ && pending_ && tickDiff(now, pendT_) >= (int32_t)width_)
        release();
    if (!nReady_) return false;
    t = ready_[0].t;
    state = ready_[0].state;
    for (uint8_t i = 1; i < nReady_; i++) ready_[i - 1] = ready_[i];
    nReady_--;
    return true;
}

} // namespace M32Kip
