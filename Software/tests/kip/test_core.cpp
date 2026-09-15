/******************************************************************************************************************************
 *  Host tests for the M32KIP protocol core (Software/src/Version 6 and newer/M32Kip.*).
 *
 *      make                build and run
 *      make vectors        emit the wire vectors as hex for check_vectors.py to verify against Python's hashlib
 *
 *  Built with the address and undefined-behaviour sanitizers, so a buffer slip in the codecs fails the run rather than
 *  turning into a field bug on the ESP32.
 *****************************************************************************************************************************/

#include "M32Kip.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

using namespace M32Kip;

static int failures = 0, checks = 0;

static void ok(bool cond, const char* what) {
    checks++;
    if (!cond) { failures++; std::printf("  FAIL  %s\n", what); }
}
static void okEq(uint32_t got, uint32_t want, const char* what) {
    checks++;
    if (got != want) { failures++; std::printf("  FAIL  %s: got %u, want %u\n", what, got, want); }
}
static void section(const char* name) { std::printf("%s\n", name); }

static std::string hex(const uint8_t* b, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; i++) { s += d[b[i] >> 4]; s += d[b[i] & 15]; }
    return s;
}

// ---------------------------------------------------------------- SHA-256 / HMAC against the published vectors

static void testCrypto() {
    section("crypto");
    uint8_t out[32];

    sha256((const uint8_t*)"", 0, out);
    ok(hex(out, 32) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 of the empty string");

    sha256((const uint8_t*)"abc", 3, out);
    ok(hex(out, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256(\"abc\")");

    // A message spanning several blocks, to exercise the buffering in shaUpdate.
    std::string longMsg(1000, 'a');
    sha256((const uint8_t*)longMsg.data(), longMsg.size(), out);
    ok(hex(out, 32) == "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3", "SHA-256 of 1000 x 'a'");

    // RFC 4231 test case 1.
    uint8_t key[20];
    memset(key, 0x0b, sizeof(key));
    hmacSha256(key, sizeof(key), (const uint8_t*)"Hi There", 8, out);
    ok(hex(out, 32) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "HMAC-SHA256 RFC 4231 case 1");

    // RFC 4231 test case 2.
    hmacSha256((const uint8_t*)"Jefe", 4, (const uint8_t*)"what do ya want for nothing?", 28, out);
    ok(hex(out, 32) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "HMAC-SHA256 RFC 4231 case 2");

    // A key longer than the 64-byte block, which must be hashed down first (RFC 4231 case 6).
    uint8_t longKey[131];
    memset(longKey, 0xaa, sizeof(longKey));
    hmacSha256(longKey, sizeof(longKey), (const uint8_t*)"Test Using Larger Than Block-Size Key - Hash Key First", 54, out);
    ok(hex(out, 32) == "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54", "HMAC-SHA256 RFC 4231 case 6");

    // Key derivation is defined in terms of those two, so this only pins the wiring.
    uint8_t base[32], sess[32];
    deriveBaseKey("morserino test psk", base);
    uint8_t want[32];
    sha256((const uint8_t*)"morserino test psk", 18, want);
    ok(memcmp(base, want, 32) == 0, "K_base is SHA-256 of the passphrase");

    const uint8_t nc[8] = {1,2,3,4,5,6,7,8}, ns[8] = {9,10,11,12,13,14,15,16};
    deriveSessionKey(base, nc, ns, sess);
    uint8_t material[16];
    memcpy(material, nc, 8); memcpy(material + 8, ns, 8);
    hmacSha256(base, 32, material, 16, want);
    ok(memcmp(sess, want, 32) == 0, "K_sess is HMAC(K_base, nonce_c || nonce_s)");
    ok(memcmp(sess, base, 32) != 0, "the session key differs from the base key");
}

// ---------------------------------------------------------------- packet layout and round trips

static void fillKey(uint8_t k[32], uint8_t seed) { for (int i = 0; i < 32; i++) k[i] = (uint8_t)(seed + i); }

static void testLayout() {
    section("wire layout (spec 6)");
    uint8_t key[32]; fillKey(key, 0x10);
    uint8_t buf[MAX_PACKET];

    Header h; h.session = 0xDEADBEEF; h.seq = 0x1234; h.flags = 0;

    Hello hello;
    for (int i = 0; i < 8; i++) hello.nonceC[i] = (uint8_t)(0xA0 + i);
    hello.tNow = 0x11223344; hello.wpm = 25; hello.redundancy = 4; hello.source = SRC_STRAIGHT; hello.caps = 0;
    size_t n = encodeHello(buf, sizeof(buf), h, hello, key);
    okEq((uint32_t)n, HEADER_LEN + 16 + MAC_LEN, "HELLO is 36 bytes on the wire");
    okEq(buf[0], MAGIC, "magic");
    okEq(buf[1], VERSION, "version");
    okEq(buf[2], PKT_HELLO, "type");
    okEq(buf[4] | (buf[5] << 8) | (buf[6] << 16) | ((uint32_t)buf[7] << 24), 0xDEADBEEF, "session is little-endian at offset 4");
    okEq(buf[8] | (buf[9] << 8), 0x1234, "seq at offset 8");
    okEq(buf[10] | (buf[11] << 8), 0, "reserved is zero");

    okEq((uint32_t)encodeHelloAck(buf, sizeof(buf), h, HelloAck(), key), HEADER_LEN + 24 + MAC_LEN, "HELLO_ACK is 44 bytes");
    okEq((uint32_t)encodeNack(buf, sizeof(buf), h, Nack(), key),         HEADER_LEN + 2 + MAC_LEN,  "NACK is 22 bytes");
    okEq((uint32_t)encodeStats(buf, sizeof(buf), h, Stats(), key),       HEADER_LEN + 20 + MAC_LEN, "STATS is 40 bytes");
    okEq((uint32_t)encodeBye(buf, sizeof(buf), h, key),                  HEADER_LEN + MAC_LEN,      "BYE is 20 bytes");

    KeyPkt k8; k8.n = 8;
    okEq((uint32_t)encodeKey(buf, sizeof(buf), h, k8, key), HEADER_LEN + 8 + 40 + MAC_LEN, "a full KEY packet is 68 bytes");
    ok(HEADER_LEN + 8 + 40 + MAC_LEN <= 200, "the largest packet is well under the 200-byte budget (spec 5)");
}

static void testRoundTrip() {
    section("codec round trips");
    uint8_t key[32]; fillKey(key, 0x20);
    uint8_t buf[MAX_PACKET];
    Header h, hd;

    { // HELLO
        h.session = 0; h.seq = 7;
        Hello p, d;
        for (int i = 0; i < 8; i++) p.nonceC[i] = (uint8_t)(i * 17);
        p.tNow = 123456789; p.wpm = 0; p.redundancy = 6; p.source = SRC_EXTERNAL; p.caps = 0;
        size_t n = encodeHello(buf, sizeof(buf), h, p, key);
        ok(decodeHello(buf, n, key, hd, d), "HELLO decodes");
        okEq(hd.seq, 7, "HELLO seq survives");
        okEq(d.tNow, 123456789, "HELLO t_now survives");
        okEq(d.redundancy, 6, "HELLO redundancy survives");
        okEq(d.source, SRC_EXTERNAL, "HELLO source survives");
        ok(memcmp(d.nonceC, p.nonceC, 8) == 0, "HELLO nonce survives");
    }
    { // HELLO_ACK
        HelloAck p, d;
        for (int i = 0; i < 8; i++) { p.nonceC[i] = (uint8_t)i; p.nonceS[i] = (uint8_t)(255 - i); }
        p.session = 0x01020304; p.maxKeydownMs = 10000; p.pttLeadMs = 20; p.flags = 1;
        size_t n = encodeHelloAck(buf, sizeof(buf), h, p, key);
        ok(decodeHelloAck(buf, n, key, hd, d), "HELLO_ACK decodes");
        okEq(d.session, 0x01020304, "HELLO_ACK session survives");
        okEq(d.maxKeydownMs, 10000, "HELLO_ACK max_keydown survives");
        okEq(d.pttLeadMs, 20, "HELLO_ACK ptt_lead survives");
        okEq(d.flags, 1, "HELLO_ACK flags survive");
        ok(memcmp(d.nonceS, p.nonceS, 8) == 0, "HELLO_ACK server nonce survives");
    }
    { // KEY with a full edge list
        KeyPkt p, d;
        p.tNow = 5000; p.wpm = 40; p.source = SRC_KEYER; p.n = MAX_EDGES;
        for (uint8_t i = 0; i < MAX_EDGES; i++) { p.edges[i].t = 1000u + i * 37u; p.edges[i].state = (uint8_t)(i & 1); }
        size_t n = encodeKey(buf, sizeof(buf), h, p, key);
        ok(decodeKey(buf, n, key, hd, d), "KEY decodes");
        okEq(d.n, MAX_EDGES, "KEY edge count survives");
        okEq(d.wpm, 40, "KEY wpm survives");
        bool same = true;
        for (uint8_t i = 0; i < MAX_EDGES; i++)
            if (d.edges[i].t != p.edges[i].t || d.edges[i].state != p.edges[i].state) same = false;
        ok(same, "every edge survives, in order");
    }
    { // KEY keepalive with no edges at all
        KeyPkt p, d;
        p.tNow = 77; p.n = 0;
        size_t n = encodeKey(buf, sizeof(buf), h, p, key);
        okEq((uint32_t)n, HEADER_LEN + 8 + MAC_LEN, "an empty KEY is 28 bytes");
        ok(decodeKey(buf, n, key, hd, d) && d.n == 0, "an empty KEY decodes");
    }
    { // STATS
        Stats p, d;
        p.playoutMs = 150; p.jitter100us = 88; p.lossPct = 3; p.lateEdges = 2; p.underruns = 1;
        p.rigState = RIG_KEY_DOWN | RIG_KEYDOWN_LIMIT; p.maxKeydownMs = 3000; p.ditEst100us = 480;
        p.tEcho = 0xAABBCCDD; p.tRig = 0x99887766;
        size_t n = encodeStats(buf, sizeof(buf), h, p, key);
        ok(decodeStats(buf, n, key, hd, d), "STATS decodes");
        okEq(d.playoutMs, 150, "STATS playout survives");
        okEq(d.ditEst100us, 480, "STATS dit estimate survives");
        okEq(d.tEcho, 0xAABBCCDD, "STATS t_echo survives");
        okEq(d.tRig, 0x99887766, "STATS t_rig survives");
        okEq(d.rigState, RIG_KEY_DOWN | RIG_KEYDOWN_LIMIT, "STATS rig state survives");
    }
    { // BYE
        size_t n = encodeBye(buf, sizeof(buf), h, key);
        ok(decodeBye(buf, n, key, hd), "BYE decodes");
    }
}

static void testRejection() {
    section("what must be refused");
    uint8_t key[32], wrong[32];
    fillKey(key, 0x30); fillKey(wrong, 0x31);
    uint8_t buf[MAX_PACKET];
    Header h, hd;
    Hello p, d;
    p.tNow = 42;
    size_t n = encodeHello(buf, sizeof(buf), h, p, key);

    ok(!decodeHello(buf, n, wrong, hd, d), "a packet under the wrong key is refused");

    for (size_t i = 0; i < n; i++) {                    // every single-bit change must break the MAC
        for (int bit = 0; bit < 8; bit++) {
            uint8_t save = buf[i];
            buf[i] ^= (uint8_t)(1 << bit);
            bool accepted = decodeHello(buf, n, key, hd, d);
            buf[i] = save;
            if (accepted) { std::printf("  FAIL  bit %d of offset %zu was accepted\n", bit, i); failures++; }
            checks++;
        }
    }

    buf[0] = 0x4C;
    ok(!decodeHello(buf, n, key, hd, d), "the wrong magic is refused");
    buf[0] = MAGIC;
    buf[1] = 0x02;
    ok(!decodeHello(buf, n, key, hd, d), "an unsupported version is refused");
    buf[1] = VERSION;

    HelloAck wrongType;
    ok(!decodeHelloAck(buf, n, key, hd, wrongType), "a HELLO is not accepted as a HELLO_ACK");
    ok(!decodeHello(buf, n - 1, key, hd, d), "a truncated packet is refused");
    ok(!decodeHello(buf, HEADER_LEN + MAC_LEN - 1, key, hd, d), "a runt shorter than header+MAC is refused");

    { // a KEY claiming more edges than it carries
        KeyPkt kp; kp.n = 2; kp.tNow = 1;
        uint8_t kb[MAX_PACKET];
        size_t kn = encodeKey(kb, sizeof(kb), h, kp, key);
        kb[HEADER_LEN + 5] = 8;                         // n := 8 without adding bytes
        uint8_t mac[32];
        hmacSha256(key, 32, kb, kn - MAC_LEN, mac);     // re-seal so it fails on the length, not the MAC
        memcpy(kb + kn - MAC_LEN, mac, MAC_LEN);
        KeyPkt kd;
        ok(!decodeKey(kb, kn, key, hd, kd), "a KEY whose edge count does not match its length is refused");
    }
    { // an edge state outside {0,1}
        KeyPkt kp; kp.n = 1; kp.tNow = 1; kp.edges[0].t = 5; kp.edges[0].state = 1;
        uint8_t kb[MAX_PACKET];
        size_t kn = encodeKey(kb, sizeof(kb), h, kp, key);
        kb[HEADER_LEN + 8 + 4] = 7;
        uint8_t mac[32];
        hmacSha256(key, 32, kb, kn - MAC_LEN, mac);
        memcpy(kb + kn - MAC_LEN, mac, MAC_LEN);
        KeyPkt kd;
        ok(!decodeKey(kb, kn, key, hd, kd), "an edge state other than 0 or 1 is refused");
    }
    { // encoding into a buffer that is too small must fail rather than overrun
        uint8_t tiny[8];
        okEq((uint32_t)encodeHello(tiny, sizeof(tiny), h, p, key), 0, "encoding into a short buffer returns 0");
        KeyPkt kp; kp.n = 9;
        okEq((uint32_t)encodeKey(buf, sizeof(buf), h, kp, key), 0, "encoding more than 8 edges is refused");
    }
}

// ---------------------------------------------------------------- replay window

static void testReplay() {
    section("replay window (spec 9)");
    ReplayWindow w;
    ok(w.accept(100), "the first packet is accepted");
    ok(!w.accept(100), "the same seq twice is refused");
    ok(w.accept(101), "the next seq is accepted");
    ok(w.accept(105), "a small forward jump is accepted");
    ok(w.accept(103), "a reordered packet inside the window is accepted");
    ok(!w.accept(103), "and only once");
    ok(w.accept(102), "another reordered packet is accepted");
    ok(!w.accept(101), "an old duplicate is still refused");
    ok(!w.accept(41),  "a seq more than 64 behind is refused");
    ok(w.accept(200),  "a jump clear of the window restarts it");
    ok(!w.accept(105), "and everything before the jump is then out of the window");
    ok(w.accept(199),  "but a packet just behind the new head is accepted");

    ReplayWindow ww;                                    // the seq field wraps at 65536
    ok(ww.accept(65530), "a seq near the wrap is accepted");
    ok(ww.accept(65535), "the last seq before the wrap is accepted");
    ok(ww.accept(2),     "a seq after the wrap is accepted");
    ok(!ww.accept(65535),"a duplicate across the wrap is refused");
    ok(ww.accept(65533), "a reordered packet across the wrap is accepted");
}

// ---------------------------------------------------------------- vectors for the Python cross-check

static void emitVectors() {
    uint8_t key[32];
    deriveBaseKey("morserino keying over ip", key);
    uint8_t buf[MAX_PACKET];
    Header h; h.session = 0x0A0B0C0D; h.seq = 4242;

    std::printf("psk morserino keying over ip\n");
    std::printf("kbase %s\n", hex(key, 32).c_str());

    Hello hello;
    for (int i = 0; i < 8; i++) hello.nonceC[i] = (uint8_t)(0x31 + i);
    hello.tNow = 0x01020304; hello.wpm = 25; hello.redundancy = 4; hello.source = SRC_KEYER;
    Header h0; h0.seq = 1;
    std::printf("hello %s\n", hex(buf, encodeHello(buf, sizeof(buf), h0, hello, key)).c_str());

    uint8_t sess[32];
    uint8_t nonceS[8]; for (int i = 0; i < 8; i++) nonceS[i] = (uint8_t)(0x91 + i);
    deriveSessionKey(key, hello.nonceC, nonceS, sess);
    std::printf("ksess %s\n", hex(sess, 32).c_str());

    HelloAck ack;
    memcpy(ack.nonceC, hello.nonceC, 8); memcpy(ack.nonceS, nonceS, 8);
    ack.session = 0x0A0B0C0D; ack.maxKeydownMs = 3000; ack.pttLeadMs = 0; ack.flags = 0;
    Header h1; h1.session = 0x0A0B0C0D; h1.seq = 1;
    std::printf("helloack %s\n", hex(buf, encodeHelloAck(buf, sizeof(buf), h1, ack, sess)).c_str());

    KeyPkt kp;
    kp.tNow = 0x00100000; kp.wpm = 25; kp.source = SRC_KEYER; kp.n = 4;
    for (uint8_t i = 0; i < 4; i++) { kp.edges[i].t = 0x000FFF00u + i * 480u; kp.edges[i].state = (uint8_t)(1 - (i & 1)); }
    std::printf("key %s\n", hex(buf, encodeKey(buf, sizeof(buf), h, kp, sess)).c_str());

    Stats st;
    st.playoutMs = 150; st.jitter100us = 88; st.lossPct = 1; st.lateEdges = 0; st.underruns = 0;
    st.rigState = RIG_KEY_DOWN; st.maxKeydownMs = 3000; st.ditEst100us = 480;
    st.tEcho = 0x00100000; st.tRig = 0x00200000;
    std::printf("stats %s\n", hex(buf, encodeStats(buf, sizeof(buf), h, st, sess)).c_str());
    std::printf("bye %s\n", hex(buf, encodeBye(buf, sizeof(buf), h, sess)).c_str());
}


// ---------------------------------------------------------------- speed estimate

static void testSpeedEstimator() {
    section("speed estimate (spec 7.5)");
    SpeedEstimator e;
    ok(!e.hasEstimate(), "there is no estimate before any mark");
    okEq(e.ditEst(), 600, "the fallback is 60 ms, i.e. 20 WPM");
    okEq(e.idleGapTicks(), 3600, "the idle gap at 20 WPM is 6 dits");

    // 25 WPM: dit 48 ms, dah 144 ms
    for (int i = 0; i < 6; i++) { e.addMark(480); e.addMark(480); e.addMark(1440); }
    ok(e.hasEstimate(), "a mixed stream yields an estimate");
    okEq(e.ditEst(), 480, "the dit is picked out of a dit/dah mixture at 25 WPM");

    SpeedEstimator only;
    for (int i = 0; i < 8; i++) only.addMark(1440);
    okEq(only.ditEst(), 600, "a run of dahs alone does not redefine the dit");

    SpeedEstimator tune;
    tune.addMark(480); tune.addMark(480); tune.addMark(1440);
    uint32_t before = tune.ditEst();
    tune.addMark(120000);                               // 12 s of key-down: tuning
    okEq(tune.ditEst(), before, "a tuning carrier is ignored");

    SpeedEstimator slow;                                // 15 WPM: dit 80 ms
    for (int i = 0; i < 6; i++) { slow.addMark(800); slow.addMark(800); slow.addMark(2400); }
    okEq(slow.ditEst(), 800, "the dit is found at 15 WPM");
    okEq(slow.idleGapTicks(), 4800, "the idle gap at 15 WPM is 480 ms");

    SpeedEstimator fast;                                // 40 WPM: dit 30 ms, where the 300 ms floor takes over
    for (int i = 0; i < 6; i++) { fast.addMark(300); fast.addMark(300); fast.addMark(900); }
    okEq(fast.ditEst(), 300, "the dit is found at 40 WPM");
    okEq(fast.idleGapTicks(), 3000, "the 300 ms floor holds at 40 WPM");
}

// ---------------------------------------------------------------- edge queue

static void testEdgeQueue() {
    section("edge queue");
    EdgeQueue q;
    bool dup = false;
    ok(q.empty(), "a new queue is empty");
    ok(q.insert(Edge(300, KEY_DOWN), dup) && !dup, "insert");
    ok(q.insert(Edge(100, KEY_DOWN), dup) && !dup, "an earlier edge inserts before it");
    ok(q.insert(Edge(200, KEY_UP), dup)   && !dup, "and one in the middle");
    okEq(q.size(), 3, "three edges are queued");
    okEq(q.at(0).t, 100, "sorted: first");
    okEq(q.at(1).t, 200, "sorted: second");
    okEq(q.at(2).t, 300, "sorted: third");

    ok(q.insert(Edge(200, KEY_UP), dup) && dup, "the same timestamp is reported as a duplicate");
    okEq(q.size(), 3, "and does not grow the queue");

    q.popFront();
    okEq(q.at(0).t, 200, "popFront removes the earliest");
    q.removeAt(1);
    okEq(q.size(), 1, "removeAt removes the one asked for");

    EdgeQueue full;
    for (uint8_t i = 0; i < EdgeQueue::CAPACITY; i++) ok(full.insert(Edge(i * 10u, KEY_DOWN), dup), "fills up");
    ok(full.full(), "the queue reports itself full");
    ok(!full.insert(Edge(99999, KEY_DOWN), dup), "and refuses one more");

    EdgeQueue wrap;                                     // ordering must survive the 32-bit tick wrap
    wrap.insert(Edge(0xFFFFFF00u, KEY_DOWN), dup);
    wrap.insert(Edge(0x00000100u, KEY_UP), dup);
    wrap.insert(Edge(0xFFFFFFF0u, KEY_UP), dup);
    okEq(wrap.at(0).t, 0xFFFFFF00u, "across the wrap, the oldest is still first");
    okEq(wrap.at(1).t, 0xFFFFFFF0u, "then the next");
    okEq(wrap.at(2).t, 0x00000100u, "and the wrapped one is last, not first");
}

// ---------------------------------------------------------------- keyer redundancy

static void testKeyerRedundancy() {
    section("keyer redundancy (spec 6.4)");
    uint8_t key[32]; fillKey(key, 0x40);
    uint8_t buf[MAX_PACKET];
    Header hd; KeyPkt got;

    KeyerSession k;
    k.begin(4);
    k.addEdge(1000, KEY_DOWN);
    size_t n = k.buildKey(buf, sizeof(buf), 1000, 25, SRC_KEYER, 1, 1, key, true);
    ok(decodeKey(buf, n, key, hd, got), "the first KEY decodes");
    okEq(got.n, 1, "the first packet carries the one edge there is");

    k.addEdge(1480, KEY_UP); k.addEdge(1960, KEY_DOWN); k.addEdge(2440, KEY_UP); k.addEdge(2920, KEY_DOWN);
    n = k.buildKey(buf, sizeof(buf), 2920, 25, SRC_KEYER, 1, 2, key, true);
    ok(decodeKey(buf, n, key, hd, got), "a later KEY decodes");
    okEq(got.n, 4, "R = 4 means the new edge plus the three before it");
    okEq(got.edges[3].t, 2920, "the newest edge is last");
    okEq(got.edges[0].t, 1480, "and the oldest of the four is first");

    n = k.buildKey(buf, sizeof(buf), 3200, 25, SRC_KEYER, 1, 3, key, false);
    ok(decodeKey(buf, n, key, hd, got), "a keepalive decodes");
    okEq(got.n, 3, "a keepalive repeats the last R-1 edges");
    okEq(got.edges[2].t, 2920, "ending with the newest");
    okEq(got.tNow, 3200, "and carries the current sender time");

    KeyerSession one;
    one.begin(1);
    one.addEdge(50, KEY_DOWN);
    n = one.buildKey(buf, sizeof(buf), 50, 0, SRC_STRAIGHT, 1, 1, key, true);
    decodeKey(buf, n, key, hd, got);
    okEq(got.n, 1, "R = 1 sends just the new edge");
    n = one.buildKey(buf, sizeof(buf), 60, 0, SRC_STRAIGHT, 1, 2, key, false);
    decodeKey(buf, n, key, hd, got);
    okEq(got.n, 0, "and its keepalive carries none");

    KeyerSession clamp;
    clamp.begin(99);
    okEq(clamp.redundancy(), MAX_EDGES, "an absurd R is clamped to the packet maximum");
}

// ---------------------------------------------------------------- send schedule (D13)

static void testSendSchedule() {
    section("send schedule (D13)");
    KeyerSession k;
    k.begin(4, 20, 3, 250);                             // 3 repeats 20 ms apart, then a 250 ms keepalive

    k.noteSent(1000);                                   // arming send at session start, no edges yet
    okEq(k.nextSendAt(), 1000 + 2500, "with nothing sent yet the next packet is one keepalive away");

    k.addEdge(5000, KEY_DOWN);
    okEq(k.repeatsLeft(), 3, "an edge arms three repeats");
    k.noteSent(5000);                                   // the packet reporting the edge
    okEq(k.nextSendAt(), 5000 + 200, "the first repeat follows 20 ms later");
    ok(!k.sendDue(5100), "and is not due before then");
    ok(k.sendDue(5200), "and is due at 20 ms");

    k.noteSent(5200); okEq(k.repeatsLeft(), 1, "second repeat consumed");
    okEq(k.nextSendAt(), 5200 + 200, "repeats are 20 ms apart");
    k.noteSent(5400); okEq(k.repeatsLeft(), 0, "third repeat consumed");
    k.noteSent(5600);
    okEq(k.nextSendAt(), 5600 + 2500, "after the repeats the schedule falls back to the keepalive");

    k.addEdge(6000, KEY_UP);
    okEq(k.repeatsLeft(), 3, "the next edge arms the repeats again");

    KeyerSession plain;                                 // the Draft 0.2 cadence, for comparison runs
    plain.begin(4, 20, 0, 250);
    plain.addEdge(100, KEY_DOWN);
    okEq(plain.repeatsLeft(), 0, "with repeats disabled an edge arms none");
    plain.noteSent(100);
    okEq(plain.nextSendAt(), 100 + 2500, "and the cadence is the plain keepalive");
}

// ---------------------------------------------------------------- end-to-end simulation
//
// A CW stream is generated, keyed through the real encoder, pushed through a network model, decoded and
// reconstructed by a real RigSession, and the reproduced key line is compared with the original. This is where
// the spec's promise is actually tested: marks are never shortened, whatever the network does.

namespace sim {

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed) {}
    uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
    uint32_t below(uint32_t n) { return n ? next() % n : 0; }
};

struct Profile {
    const char* name;
    uint32_t delayMs;
    uint32_t jitterMs;
    uint32_t lossPct;
    uint32_t burstEvery;        // 0 = none
    uint32_t burstLen;
    int32_t  driftPpm;
    uint32_t keepaliveMs;       // 0 = the spec's 250 ms
    uint16_t repeatMs;          // D13 send schedule; repeats = 0 is the Draft 0.2 cadence
    uint8_t  repeats;
};

struct Mark { uint32_t start, len; };

struct Result {
    uint32_t marksSent, marksHeard;
    int32_t  worstShortening;   // ticks a mark came out shorter than it was sent; must never be positive
    int32_t  worstStretch;
    int32_t  worstGapShortening;
    uint32_t underruns, late, duplicates, protocolErrors;
    uint32_t finalDMs, maxDMs;
    bool     everDropped;
    // Where the worst of each happened, and what the reconstruction state was at the time. Printed when an
    // assertion fails, so a regression is diagnosed from the run rather than guessed at.
    uint32_t stretchAt, stretchSent, stretchHeard, stretchDdown, stretchDup;
    uint32_t gapAt, gapSent, gapHeard, gapDup, gapDnext, gapOffUp, gapOffNext;
    // Gaps are judged by what they are for: spacing inside a character must not move at all, while a word gap
    // is where the reconstruction is allowed to take up drift and shorten the playout delay (spec 7.2, 7.3).
    int32_t  worstShortGapShrink;   // gaps below 300 ms: element and character spacing
    int32_t  worstIdleGapShrink;    // gaps of 300 ms and up: the idle gaps housekeeping may use
    uint32_t bursts, stretchedMarks, maxLateTicks;
    uint32_t packetsSent;       // what the send schedule costs
};

// A plausible element stream: characters of one to four elements, inter-element, inter-character and word gaps.
static void generateCw(Rng& rng, uint32_t ditTicks, uint32_t chars, std::vector<Mark>& marks, uint32_t start) {
    uint32_t t = start;
    for (uint32_t c = 0; c < chars; c++) {
        uint32_t nElem = 1 + rng.below(4);
        for (uint32_t e = 0; e < nElem; e++) {
            uint32_t len = (rng.below(2) ? 3u : 1u) * ditTicks;
            Mark m; m.start = t; m.len = len;
            marks.push_back(m);
            t += len;
            t += (e + 1 < nElem) ? ditTicks : ((c % 5 == 4) ? 7 * ditTicks : 3 * ditTicks);
        }
    }
}

struct Packet { uint32_t arrival; uint8_t bytes[MAX_PACKET]; size_t len; };

static Result run(const Profile& p, uint32_t wpm, uint32_t chars, uint32_t seed) {
    Rng rng(seed);
    const uint32_t dit = 12000 / wpm;                   // ticks: 1200/wpm ms
    const uint32_t senderStart = 0x00001000u;
    const uint32_t trueOffset  = 0x7F000000u;           // rig clock lives in a quite different place
    const uint32_t netDelay    = msToTicks(p.delayMs);

    std::vector<Mark> marks;
    generateCw(rng, dit, chars, marks, senderStart);

    uint8_t base[32], sess[32];
    deriveBaseKey("simulation psk", base);
    const uint8_t nc[8] = {1,2,3,4,5,6,7,8}, ns[8] = {8,7,6,5,4,3,2,1};
    deriveSessionKey(base, nc, ns, sess);

    // ---- keyer side: edges as they happen, plus whatever the send schedule asks for (D13) ----
    std::vector<Edge> edgeList;
    for (size_t i = 0; i < marks.size(); i++) {
        edgeList.push_back(Edge(marks[i].start, KEY_DOWN));
        edgeList.push_back(Edge(marks[i].start + marks[i].len, KEY_UP));
    }
    uint32_t lastT = marks.empty() ? senderStart : marks.back().start + marks.back().len;
    const uint32_t endSend = lastT + msToTicks(1500);

    KeyerSession keyer;
    keyer.begin(4, p.repeatMs ? p.repeatMs : 20, p.repeats,
                (uint16_t)(p.keepaliveMs ? p.keepaliveMs : 250));

    std::vector<Packet> wire;
    uint16_t seq = 0;
    uint32_t sinceBurst = 0, burstLeft = 0, bursts = 0, packetsSent = 0;
    size_t ei = 0;
    uint32_t t = senderStart;

    // Everything below the send decision is the network; the two are kept apart so the schedule can be
    // changed without touching the impairment model.
    struct Emit {
        static void go(std::vector<Packet>& wire, KeyerSession& keyer, const Profile& pr, Rng& rng,
                       uint32_t tSend, uint32_t senderStart, uint32_t trueOffset, uint32_t netDelay,
                       uint16_t seq, uint32_t wpm, const uint8_t* sess, bool newEdge,
                       uint32_t& sinceBurst, uint32_t& burstLeft, uint32_t& bursts, uint32_t& packetsSent) {
            Packet pk;
            pk.len = keyer.buildKey(pk.bytes, sizeof(pk.bytes), tSend, (uint8_t)wpm,
                                    SRC_KEYER, 0x11223344u, seq, sess, newEdge);
            keyer.noteSent(tSend);
            if (pk.len == 0) return;
            packetsSent++;

            bool drop = false;
            if (pr.burstEvery && ++sinceBurst >= pr.burstEvery) { sinceBurst = 0; burstLeft = pr.burstLen; bursts++; }
            if (burstLeft) { drop = true; burstLeft--; }
            else if (pr.lossPct && rng.below(100) < pr.lossPct) drop = true;
            if (drop) return;

            uint32_t senderDelta = tSend - senderStart;
            uint32_t drift = (uint32_t)((int64_t)senderDelta * pr.driftPpm / 1000000);
            uint32_t jitter = pr.jitterMs ? rng.below(msToTicks(pr.jitterMs)) : 0;
            pk.arrival = senderStart + senderDelta + drift + trueOffset + netDelay + jitter;
            wire.push_back(pk);
        }
    };

    Emit::go(wire, keyer, p, rng, t, senderStart, trueOffset, netDelay, ++seq, wpm, sess, false,
             sinceBurst, burstLeft, bursts, packetsSent);       // one packet to arm the schedule

    while (ei < edgeList.size() || tickDiff(keyer.nextSendAt(), endSend) <= 0) {
        uint32_t tEdge = (ei < edgeList.size()) ? edgeList[ei].t : 0;
        bool edgeFirst = (ei < edgeList.size()) && tickDiff(tEdge, keyer.nextSendAt()) <= 0;
        if (edgeFirst) {
            t = tEdge;
            keyer.addEdge(edgeList[ei].t, edgeList[ei].state);
            ei++;
            Emit::go(wire, keyer, p, rng, t, senderStart, trueOffset, netDelay, ++seq, wpm, sess, true,
                     sinceBurst, burstLeft, bursts, packetsSent);
        } else {
            t = keyer.nextSendAt();
            if (tickDiff(t, endSend) > 0) break;
            Emit::go(wire, keyer, p, rng, t, senderStart, trueOffset, netDelay, ++seq, wpm, sess, false,
                     sinceBurst, burstLeft, bursts, packetsSent);
        }
    }

    std::sort(wire.begin(), wire.end(), [](const Packet& a, const Packet& b) { return a.arrival < b.arrival; });

    // ---- rig side ----
    RigConfig cfg;
    RigSession rig;
    uint32_t rigNow = senderStart + trueOffset;
    rig.begin(rigNow, cfg, SRC_KEYER);

    Result res;
    memset(&res, 0, sizeof(res));
    res.marksSent = (uint32_t)marks.size();
    res.maxDMs = ticksToMs(rig.playoutTicks());

    std::vector<Mark> heard;
    std::vector<uint32_t> dAtDown, dAtUp, offAtUp;      // reconstruction state captured at each emitted edge
    bool keyDown = false;
    uint32_t downAt = 0, dDown = 0;
    size_t nextPkt = 0;
    uint32_t endTime = wire.empty() ? rigNow : wire.back().arrival + msToTicks(2000);

    while (tickDiff(rigNow, endTime) <= 0) {
        while (nextPkt < wire.size() && tickDiff(wire[nextPkt].arrival, rigNow) <= 0) {
            Header h; KeyPkt kp;
            if (decodeKey(wire[nextPkt].bytes, wire[nextPkt].len, sess, h, kp))
                rig.onKey(h, kp, rigNow);
            nextPkt++;
        }
        RigAction act;
        while (rig.poll(rigNow, act)) {
            if (act.type == RIG_DROP) { res.everDropped = true; break; }
            if (act.type == RIG_EMIT || act.type == RIG_FORCE_KEYUP) {
                if (act.state == KEY_DOWN) {
                    keyDown = true; downAt = rigNow; dDown = ticksToMs(rig.playoutTicks());
                } else if (keyDown) {
                    Mark m; m.start = downAt; m.len = rigNow - downAt;
                    heard.push_back(m);
                    dAtDown.push_back(dDown);
                    dAtUp.push_back(ticksToMs(rig.playoutTicks()));
                    offAtUp.push_back(rig.offset());
                    keyDown = false;
                }
            }
        }
        uint32_t d = ticksToMs(rig.playoutTicks());
        if (d > res.maxDMs) res.maxDMs = d;

        uint32_t next = rigNow + msToTicks(1);          // poll at least every ms, and exactly when an edge is due
        if (nextPkt < wire.size() && tickDiff(wire[nextPkt].arrival, next) < 0) next = wire[nextPkt].arrival;
        if (rig.hasPending() && tickDiff(rig.nextEmitTime(), next) < 0 && tickDiff(rig.nextEmitTime(), rigNow) > 0)
            next = rig.nextEmitTime();
        rigNow = next;
    }

    res.marksHeard = (uint32_t)heard.size();
    size_t n = heard.size() < marks.size() ? heard.size() : marks.size();
    for (size_t i = 0; i < n; i++) {
        int32_t delta = (int32_t)heard[i].len - (int32_t)marks[i].len;
        if (-delta > res.worstShortening) res.worstShortening = -delta;
        if (delta > (int32_t)msToTicks(2)) res.stretchedMarks++;
        if (delta > res.worstStretch) {
            res.worstStretch = delta;
            res.stretchAt = (uint32_t)i; res.stretchSent = marks[i].len; res.stretchHeard = heard[i].len;
            res.stretchDdown = dAtDown[i]; res.stretchDup = dAtUp[i];
        }
        if (i + 1 < n) {
            int32_t gapSent  = (int32_t)(marks[i+1].start - (marks[i].start + marks[i].len));
            int32_t gapHeard = (int32_t)(heard[i+1].start - (heard[i].start + heard[i].len));
            int32_t shrink = gapSent - gapHeard;
            if (gapSent >= (int32_t)msToTicks(300)) {
                if (shrink > res.worstIdleGapShrink) res.worstIdleGapShrink = shrink;
            } else {
                if (shrink > res.worstShortGapShrink) res.worstShortGapShrink = shrink;
            }
            if (shrink > res.worstGapShortening) {
                res.worstGapShortening = shrink;
                res.gapAt = (uint32_t)i; res.gapSent = (uint32_t)gapSent; res.gapHeard = (uint32_t)gapHeard;
                res.gapDup = dAtUp[i]; res.gapDnext = dAtDown[i+1];
                res.gapOffUp = offAtUp[i]; res.gapOffNext = offAtUp[i+1];
            }
        }
    }
    res.underruns      = rig.counters().underruns;
    res.late           = rig.counters().late;
    res.duplicates     = rig.counters().duplicates;
    res.protocolErrors = rig.counters().protocolErrors;
    res.finalDMs       = ticksToMs(rig.playoutTicks());
    res.bursts         = bursts;
    res.maxLateTicks   = rig.counters().maxLate;
    res.packetsSent    = packetsSent;
    return res;
}

} // namespace sim

static void checkProfile(const sim::Profile& p, uint32_t wpm, uint32_t chars, uint32_t seed) {
    sim::Result r = sim::run(p, wpm, chars, seed);
    char what[200];

    std::snprintf(what, sizeof(what), "[%s @ %u WPM] every mark sent is heard (%u of %u)",
                  p.name, wpm, r.marksHeard, r.marksSent);
    ok(r.marksHeard == r.marksSent, what);

    // The one promise that holds on every profile without exception (design principle 5).
    std::snprintf(what, sizeof(what), "[%s @ %u WPM] no mark is ever shortened (worst %d ticks)",
                  p.name, wpm, r.worstShortening);
    ok(r.worstShortening <= 0, what);

    // Spacing inside a character must not move: that is what would change the rhythm a listener hears.
    std::snprintf(what, sizeof(what), "[%s @ %u WPM] no element or character gap loses more than 2 ms (worst %d ticks)",
                  p.name, wpm, r.worstShortGapShrink);
    ok(r.worstShortGapShrink <= (int32_t)msToTicks(2), what);

    // Word gaps are where the reconstruction is allowed to take up drift and shrink the playout delay.
    // One offset step (1 ms) plus one playout step (5 ms) is the most a single gap may lose.
    std::snprintf(what, sizeof(what), "[%s @ %u WPM] a word gap loses at most one offset and one playout step (worst %d ticks)",
                  p.name, wpm, r.worstIdleGapShrink);
    ok(r.worstIdleGapShrink <= (int32_t)msToTicks(8), what);

    if (p.burstEvery == 0) {
        // Without burst loss nothing should ever arrive late enough to stretch a mark.
        std::snprintf(what, sizeof(what), "[%s @ %u WPM] no mark is stretched by more than 2 ms (worst %d ticks)",
                      p.name, wpm, r.worstStretch);
        ok(r.worstStretch <= (int32_t)msToTicks(2), what);
    } else {
        // Spec 13.2 acceptance for the burst profile is weaker: no shortened mark, and at most one disturbed
        // element per burst. A burst delays an edge past the playout delay, and a mark already on the air
        // cannot be retroactively shortened, so what the burst costs is one stretched mark.
        std::snprintf(what, sizeof(what), "[%s @ %u WPM] at most one mark disturbed per burst (%u marks, %u bursts)",
                      p.name, wpm, r.stretchedMarks, r.bursts);
        ok(r.stretchedMarks <= r.bursts, what);
    }

    std::snprintf(what, sizeof(what), "[%s @ %u WPM] the session survives", p.name, wpm);
    ok(!r.everDropped, what);

    std::snprintf(what, sizeof(what), "[%s @ %u WPM] the playout delay stays inside its clamp (max %u ms)",
                  p.name, wpm, r.maxDMs);
    ok(r.maxDMs <= 600, what);

    std::printf("    %-30s %2u WPM  marks %4u/%-4u  late %3u (worst %4u ticks)  stretched %2u/%-3u  pkts %5u  D %3u->%3u ms\n",
                p.name, wpm, r.marksHeard, r.marksSent, r.late, r.maxLateTicks,
                r.stretchedMarks, r.bursts, r.packetsSent, 150u, r.finalDMs);
    if (r.worstShortGapShrink > (int32_t)msToTicks(2) || r.worstIdleGapShrink > (int32_t)msToTicks(8))
        std::printf("        worst gap loss after mark %u: sent %u, heard %u ticks; D %u->%u ms, offset %u->%u\n",
                    r.gapAt, r.gapSent, r.gapHeard, r.gapDup, r.gapDnext, r.gapOffUp, r.gapOffNext);
}

static void testSimulation() {
    section("end-to-end reconstruction (spec 13.2 profiles)");
    // (a) and (b) are the spec's clean and hostile links; (c) and (d) add loss; (e) is the burst case.
    // All profiles run the ratified D13 send schedule: three repeats 20 ms apart after each edge, then the
    // 250 ms idle keepalive.
    const uint16_t RP = 20; const uint8_t RN = 3;
    sim::Profile a = {"a: 50 ms, 10 ms jitter",       50,  10, 0,  0, 0,  0, 0, RP, RN};
    sim::Profile b = {"b: 120 ms, 40 ms jitter",     120,  40, 0,  0, 0,  0, 0, RP, RN};
    sim::Profile c = {"c: b + 2 % loss",             120,  40, 2,  0, 0,  0, 0, RP, RN};
    sim::Profile d = {"d: b + 5 % loss",             120,  40, 5,  0, 0,  0, 0, RP, RN};
    sim::Profile e = {"e: bursts of 3 lost",         120,  40, 0, 40, 3,  0, 0, RP, RN};
    sim::Profile f = {"drift: 30 ppm over minutes",   60,  15, 0,  0, 0, 30, 0, RP, RN};
    // The same burst profile on the Draft 0.2 cadence, kept as the control: it is the run that showed the
    // 313 ms lateness and made the case for D13. Both are checked, so a regression in either is visible.
    sim::Profile old = {"e on the old 250 ms cadence", 120, 40, 0, 40, 3, 0, 0,  0,  0};

    for (uint32_t wpm = 15; wpm <= 40; wpm += 10) {
        checkProfile(a, wpm, 60, 0xC0FFEE + wpm);
        checkProfile(b, wpm, 60, 0xBEEF00 + wpm);
        checkProfile(c, wpm, 60, 0x123456 + wpm);
        checkProfile(d, wpm, 60, 0xABCDEF + wpm);
        checkProfile(e, wpm, 60, 0x555555 + wpm);
        checkProfile(old, wpm, 60, 0x555555 + wpm);
    }
    checkProfile(f, 25, 400, 0x99AA55);
}

// ---------------------------------------------------------------- safety (spec 8)

static void testSafety() {
    section("safety (spec 8)");
    uint8_t key[32]; fillKey(key, 0x50);
    uint8_t buf[MAX_PACKET];

    { // a mark longer than the limit is cut off, and keying resumes afterwards
        RigConfig cfg; cfg.maxKeydownKeyerMs = 1000; cfg.adaptive = false;
        RigSession rig;
        uint32_t now = 100000;
        rig.begin(now, cfg, SRC_KEYER);

        KeyerSession k; k.begin(4);
        k.addEdge(50000, KEY_DOWN);
        size_t n = k.buildKey(buf, sizeof(buf), 50000, 25, SRC_KEYER, 1, 1, key, true);
        Header h; KeyPkt kp; decodeKey(buf, n, key, h, kp);
        rig.onKey(h, kp, now);

        bool sawDown = false, sawForced = false;
        for (uint32_t step = 0; step < 30000; step += 10) {
            RigAction act;
            uint32_t t = now + step;
            while (rig.poll(t, act)) {
                if (act.type == RIG_EMIT && act.state == KEY_DOWN) sawDown = true;
                if (act.type == RIG_FORCE_KEYUP) sawForced = true;
            }
            if (sawForced) break;
            // keep the link alive so the keepalive watchdog is not what trips
            if ((step % 2000) == 0) {
                size_t m = k.buildKey(buf, sizeof(buf), 50000 + step, 25, SRC_KEYER, 1, (uint16_t)(2 + step / 2000), key, false);
                Header hh; KeyPkt pp;
                if (decodeKey(buf, m, key, hh, pp)) rig.onKey(hh, pp, t);
            }
        }
        ok(sawDown, "the key goes down");
        ok(sawForced, "and the max-keydown limit lifts it again");
        okEq(rig.keyState(), KEY_UP, "the key line is up afterwards");
        ok(rig.counters().keydownLimits == 1, "the limit is counted once");
        Stats st; rig.fillStats(st, now + 30000);
        ok((st.rigState & RIG_KEYDOWN_LIMIT) != 0, "and reported in STATS");
    }
    { // the limit depends on the source: a straight key may hold down far longer
        RigConfig cfg;
        RigSession rig;
        rig.begin(0, cfg, SRC_KEYER);
        okEq(rig.maxKeydownMs(), 3000, "an internal keyer is held to 3 s");
        RigSession rig2;
        rig2.begin(0, cfg, SRC_STRAIGHT);
        okEq(rig2.maxKeydownMs(), 10000, "a straight key gets 10 s, so tuning works");
    }
    { // silence lifts the key and then drops the session
        RigConfig cfg; cfg.keepaliveTimeoutMs = 1000;
        RigSession rig;
        uint32_t now = 500000;
        rig.begin(now, cfg, SRC_KEYER);

        KeyerSession k; k.begin(4);
        k.addEdge(1000, KEY_DOWN);
        size_t n = k.buildKey(buf, sizeof(buf), 1000, 25, SRC_KEYER, 1, 1, key, true);
        Header h; KeyPkt kp; decodeKey(buf, n, key, h, kp);
        rig.onKey(h, kp, now);

        bool forced = false, dropped = false;
        for (uint32_t step = 0; step < msToTicks(9000); step += 10) {
            RigAction act;
            while (rig.poll(now + step, act)) {
                if (act.type == RIG_FORCE_KEYUP) forced = true;
                if (act.type == RIG_DROP) dropped = true;
            }
        }
        ok(forced, "losing the link lifts the key");
        ok(dropped, "and the session is dropped after the grace period");
        ok(!rig.alive(), "the session is no longer alive");
    }
    { // a replayed packet is refused by the window, and duplicated edges change nothing
        RigConfig cfg;
        RigSession rig;
        rig.begin(0, cfg, SRC_KEYER);
        KeyerSession k; k.begin(4);
        k.addEdge(1000, KEY_DOWN);
        size_t n = k.buildKey(buf, sizeof(buf), 1000, 25, SRC_KEYER, 1, 1, key, true);
        Header h; KeyPkt kp; decodeKey(buf, n, key, h, kp);
        ok(rig.onKey(h, kp, 0), "the packet is accepted");
        ok(!rig.onKey(h, kp, 0), "the very same packet again is refused by the replay window");
        okEq(rig.counters().replays, 1, "and counted as a replay");
    }
}


// ---------------------------------------------------------------- fail-safe: every way a session dies (D14)

namespace failsafe {

struct Bench {
    RigConfig  cfg;
    RigSession rig;
    uint32_t   now;
    uint16_t   seq;
    Bench() : now(1000), seq(0) {
        cfg.maxKeydownKeyerMs = 30000;              // keep the mark-length limit out of these tests
        cfg.adaptive = false;
        rig.begin(now, cfg, SRC_KEYER);
    }
    void packet(const Edge* es, uint8_t n) {
        Header h; h.seq = ++seq;
        KeyPkt k; k.tNow = 5000 + (now - 1000); k.n = n;
        for (uint8_t i = 0; i < n; i++) k.edges[i] = es[i];
        rig.onKey(h, k, now);
    }
    void one(uint32_t t, uint8_t st) { Edge e(t, st); packet(&e, 1); }
    void run(uint32_t ticks, std::vector<RigActionType>& seen) {
        for (uint32_t i = 0; i < ticks; i += 10) {
            now += 10;
            RigAction a;
            while (rig.poll(now, a)) seen.push_back(a.type);
        }
    }
    bool keyDown() {                                // put a mark on the air
        one(5000, KEY_DOWN);
        std::vector<RigActionType> s;
        run(2000, s);
        return rig.keyState() == KEY_DOWN;
    }
};

} // namespace failsafe

static bool contains(const std::vector<RigActionType>& v, RigActionType t) {
    return std::find(v.begin(), v.end(), t) != v.end();
}

static void testRigFailSafe() {
    section("fail-safe: every way a session dies releases the key (D14)");
    using failsafe::Bench;

    {   Bench b;
        ok(b.keyDown(), "storm: a mark is on the air");
        for (int i = 0; i < 11; i++) b.one(6000u + (uint32_t)i * 10u, KEY_DOWN);   // eleven malformed edges in a second
        std::vector<RigActionType> s; b.run(100, s);
        ok(!s.empty() && s[0] == RIG_FORCE_KEYUP, "an error storm lifts the key first");
        ok(contains(s, RIG_DROP), "and then reports the session dropped");
        okEq(b.rig.keyState(), KEY_UP, "the key line is up afterwards");
        okEq(b.rig.dropReason(), DROP_ERRORS, "the reason given is the error storm");
    }
    {   Bench b;
        ok(b.keyDown(), "rate: a mark is on the air");
        std::vector<RigActionType> s;
        uint32_t t = 6000;
        for (int sec = 0; sec < 5; sec++) {
            for (int i = 0; i < 8; i++) { b.one(t, KEY_DOWN); t += 10; }
            b.run(5000, s);
            b.packet(nullptr, 0);                   // a keepalive half way through the second
            b.run(5000, s);
        }
        ok(b.rig.alive(), "eight stray edges a second for five seconds do not end a session");
        okEq(b.rig.counters().protocolErrors, 40, "all forty are counted - the old cumulative limit died at 32");
        okEq(b.rig.keyState(), KEY_DOWN, "and the mark stays on the air");
        ok(!contains(s, RIG_DROP), "no drop is reported");
    }
    {   Bench b;
        ok(b.keyDown(), "redundancy: a mark is on the air");
        for (int i = 0; i < 12; i++) b.one(7000, KEY_DOWN);                     // one bad edge, redelivered
        okEq(b.rig.counters().protocolErrors, 1, "twelve redundant copies of one malformed edge count once");
        ok(b.rig.alive(), "and do not end the session");
    }
    {   Bench b;
        ok(b.keyDown(), "overflow: a mark is on the air");
        uint32_t t = 100000; uint8_t st = KEY_UP;
        for (int p = 0; p < 7 && b.rig.alive(); p++) {
            Edge es[8];
            for (int i = 0; i < 8; i++) { es[i] = Edge(t, st); t += 500; st = (st == KEY_UP) ? KEY_DOWN : KEY_UP; }
            b.packet(es, 8);
        }
        std::vector<RigActionType> s; b.run(100, s);
        ok(!s.empty() && s[0] == RIG_FORCE_KEYUP, "a full edge queue lifts the key first");
        ok(contains(s, RIG_DROP), "and reports the session dropped");
        okEq(b.rig.keyState(), KEY_UP, "the key line is up afterwards");
        okEq(b.rig.dropReason(), DROP_OVERFLOW, "the reason given is the overflow");
    }
    {   Bench b;
        b.one(5000, KEY_DOWN);
        std::vector<RigActionType> s; b.run(80000, s);                         // eight seconds of silence
        ok(contains(s, RIG_DROP), "silence drops the session");
        okEq(b.rig.dropReason(), DROP_KEEPALIVE, "and the reason given is the timeout");
        okEq(b.rig.keyState(), KEY_UP, "with the key up");
    }
}

// ---------------------------------------------------------------- Keyer glitch filter (spec §10.1)

static void testGlitchFilter() {
    section("glitch filter");
    const uint32_t W = msToTicks(3);
    uint32_t t; uint8_t s;
    {   GlitchFilter f; f.setWidth(W);
        f.push(1000, KEY_DOWN);
        ok(!f.pop(1000 + W - 1, t, s), "an edge is held inside the window");
        ok(f.pop(1000 + W, t, s), "and released once it has held for the width");
        okEq(t, 1000, "with the time of the first change, not of the confirmation");
        okEq(s, KEY_DOWN, "and its level");
        ok(!f.pop(1000 + 10 * W, t, s), "it is released once only");
    }
    {   GlitchFilter f; f.setWidth(W);
        f.push(1000, KEY_DOWN);
        f.push(1000 + msToTicks(1), KEY_UP);                 // a 1 ms bounce
        ok(!f.pop(1000 + msToTicks(100), t, s), "a level that reverts inside the window yields neither edge");
        ok(!f.pending(), "and leaves nothing pending");
        f.push(5000, KEY_DOWN);
        ok(f.pop(5000 + W, t, s) && s == KEY_DOWN, "the next real key-down is still accepted afterwards");
        okEq(t, 5000, "and reports its own time, not the glitch's");
    }
    {   GlitchFilter f; f.setWidth(W);                       // chatter that settles
        f.push(1000, KEY_DOWN); f.push(1005, KEY_UP); f.push(1010, KEY_DOWN);
        ok(!f.pop(1010 + W - 1, t, s), "the level has to hold for the width after the last bounce");
        ok(f.pop(1010 + W, t, s) && t == 1000, "and the edge then reports the first contact of the burst");
    }
    {   GlitchFilter f; f.setWidth(W);                       // a late send task sees both edges before popping
        f.push(1000, KEY_DOWN);
        f.push(1000 + msToTicks(60), KEY_UP);
        ok(f.pop(1000 + msToTicks(60), t, s), "a mark proven by the next edge is released at once");
        okEq(t, 1000, "the key-down first");
        ok(!f.pop(1000 + msToTicks(62), t, s), "the key-up still has to hold for the width");
        ok(f.pop(1000 + msToTicks(63), t, s) && s == KEY_UP && t == 1000 + msToTicks(60), "then comes out with its own time");
    }
    {   GlitchFilter f; f.setWidth(W);                       // late task, bounce: judged by edge times, not the clock
        f.push(1000, KEY_DOWN);
        f.push(1000 + msToTicks(1), KEY_UP);
        ok(!f.pop(1000 + msToTicks(50), t, s), "a bounce seen late is still a bounce");
    }
    {   GlitchFilter f; f.setWidth(W);
        f.push(1000, KEY_DOWN);
        f.push(1000 + msToTicks(1), KEY_DOWN);               // keyOut() making sure
        int n = 0; while (f.pop(1000 + msToTicks(10), t, s)) n++;
        okEq(n, 1, "a repeated level is not a second edge");
        f.push(2000, KEY_UP); f.push(2000 + msToTicks(20), KEY_UP);
        n = 0; while (f.pop(2000 + msToTicks(30), t, s)) n++;
        okEq(n, 1, "not for the key-up either");
    }
    {   GlitchFilter f;                                      // width 0: the iambic keyer
        f.push(1000, KEY_DOWN);
        ok(f.pop(1000, t, s) && t == 1000, "width 0 passes an edge straight through");
        f.push(1001, KEY_UP);
        ok(f.pop(1001, t, s) && s == KEY_UP, "even one tick later");
    }
    {   // A straight key with bounce on every contact: marks and gaps 60-200 ms, 1-3 bounces of 0.3-2 ms at each
        // make and break. Every mark must come out once, with its true first-contact time, and the stream alternates.
        GlitchFilter f; f.setWidth(W);
        uint32_t seed = 12345;
        auto rnd = [&](uint32_t lo, uint32_t hi) { seed = seed * 1103515245u + 12345u; return lo + (seed >> 8) % (hi - lo + 1); };
        struct In { uint32_t t; uint8_t s; };
        std::vector<In> in;
        std::vector<uint32_t> truth;
        uint32_t now = 10000;
        for (int m = 0; m < 200; m++) {
            for (int edge = 0; edge < 2; edge++) {
                uint8_t lvl = edge ? KEY_UP : KEY_DOWN, other = edge ? KEY_DOWN : KEY_UP;
                truth.push_back(now);
                in.push_back({now, lvl});
                uint32_t bt = now;
                int bounces = (int)rnd(1, 3);
                for (int b = 0; b < bounces; b++) {          // chatter inside the first ~2.5 ms
                    bt += rnd(3, 8);  in.push_back({bt, other});
                    bt += rnd(3, 8);  in.push_back({bt, lvl});
                }
                now += msToTicks(rnd(60, 200));
            }
        }
        std::vector<In> out;
        size_t i = 0;
        for (uint32_t clock = 10000; clock <= now + msToTicks(10); clock += msToTicks(1)) {   // a 1 ms task loop
            while (i < in.size() && in[i].t <= clock) { f.push(in[i].t, in[i].s); i++; }
            while (f.pop(clock, t, s)) out.push_back({t, s});
        }
        okEq((uint32_t)out.size(), (uint32_t)truth.size(), "a bouncing straight key yields exactly one edge per contact change");
        bool alt = true, times = out.size() == truth.size();
        for (size_t k = 0; k < out.size(); k++) {
            if (out[k].s != ((k % 2) ? KEY_UP : KEY_DOWN)) alt = false;
            if (times && out[k].t != truth[k]) times = false;
        }
        ok(alt, "the filtered stream alternates, starting with a key-down");
        ok(times, "every edge carries its true first-contact time");
    }
}

// ---------------------------------------------------------------- break-in compensation (D16)

namespace breakin {

struct Emit { uint32_t at; uint8_t state; };

/// Feeds each edge in its own packet at the moment the sender would have sent it, polls every tick, and returns what
/// went on the line.
static std::vector<Emit> play(const RigConfig& cfg, const std::vector<Edge>& edges, uint32_t tail = 20000) {
    RigSession rig;
    uint32_t now = 1000;
    rig.begin(now, cfg, SRC_KEYER);
    std::vector<Emit> out;
    uint16_t seq = 0;
    auto step = [&](uint32_t until) {
        while (now < until) {
            now++;
            RigAction a;
            while (rig.poll(now, a))
                if (a.type == RIG_EMIT || a.type == RIG_FORCE_KEYUP) out.push_back({a.at, a.state});
        }
    };
    for (const Edge& e : edges) {
        step(1000 + (e.t - 5000));
        Header h; h.seq = ++seq;
        KeyPkt k; k.tNow = e.t; k.n = 1; k.edges[0] = e;
        rig.onKey(h, k, now);
    }
    step(now + tail);
    return out;
}

static RigConfig base() {
    RigConfig c;
    c.adaptive = false;
    c.maxKeydownKeyerMs = 30000;
    return c;
}

static uint32_t mark(const std::vector<Emit>& v, size_t i) { return v[2 * i + 1].at - v[2 * i].at; }
static uint32_t gapBefore(const std::vector<Emit>& v, size_t i) { return v[2 * i].at - v[2 * i - 1].at; }

} // namespace breakin

static void testBreakIn() {
    section("break-in compensation (D16)");
    using namespace breakin;
    // Two dits inside one transmission, then a long pause, then a third: ms timing, 60 ms dits.
    std::vector<Edge> e = { Edge(5000, KEY_DOWN), Edge(5600, KEY_UP), Edge(6200, KEY_DOWN), Edge(6800, KEY_UP),
                            Edge(13000, KEY_DOWN), Edge(13600, KEY_UP) };
    {   std::vector<Emit> v = play(base(), e);
        okEq((uint32_t)v.size(), 6, "off: all six edges emitted");
        ok(v.size() == 6 && mark(v, 0) == 600 && mark(v, 1) == 600 && mark(v, 2) == 600, "off: every mark exactly as keyed");
    }
    {   RigConfig c = base(); c.firstExtMs = 10; c.hangMs = 500;
        std::vector<Emit> v = play(c, e);
        okEq((uint32_t)v.size(), 6, "on: all six edges emitted");
        if (v.size() == 6) {
            okEq(mark(v, 0), 700, "the session's first mark starts 10 ms early");
            okEq(mark(v, 1), 600, "a mark 60 ms after the last is inside the hang and untouched");
            okEq(gapBefore(v, 1), 600, "and so is the gap before it");
            okEq(mark(v, 2), 700, "a mark after a 620 ms pause (> 500 ms hang) starts 10 ms early again");
            okEq(gapBefore(v, 2), 6100, "by shortening the pause, not by moving the key-up");
        }
        RigSession r; r.begin(0, c, SRC_KEYER);
        okEq(ticksToMs(r.playoutTicks()), 160, "the playout delay starts higher by the extension");
    }
    {   // Full QSK: hang 0 makes every mark a "first" one - keying compensation - but never across a short gap.
        RigConfig c = base(); c.firstExtMs = 30; c.hangMs = 0;
        std::vector<Edge> q = { Edge(5000, KEY_DOWN), Edge(5200, KEY_UP), Edge(5400, KEY_DOWN), Edge(5600, KEY_UP) };
        std::vector<Emit> v = play(c, q);
        okEq((uint32_t)v.size(), 4, "QSK: all four edges emitted");
        if (v.size() == 4) {
            okEq(mark(v, 0), 500, "hang 0: the first mark gets the full 30 ms");
            okEq(gapBefore(v, 1), 100, "a 20 ms gap gives up at most half of itself");
            okEq(mark(v, 1), 300, "so the next mark grows by 10 ms, not 30");
        }
    }
    {   // Hang in dits (Icom style): 7 dits at 60 ms is 420 ms. Twenty dits first, so the speed estimate settles.
        RigConfig c = base(); c.firstExtMs = 10; c.hangDitsX2 = 14;
        std::vector<Edge> d;
        uint32_t t = 5000;
        for (int i = 0; i < 20; i++) { d.push_back(Edge(t, KEY_DOWN)); d.push_back(Edge(t + 600, KEY_UP)); t += 1200; }
        t += 2400;                                                  // key up 3000 ticks = 5 dits: inside the hang
        d.push_back(Edge(t, KEY_DOWN)); d.push_back(Edge(t + 600, KEY_UP));
        t += 600 + 6000;                                            // 10 dits: the transceiver has dropped out
        d.push_back(Edge(t, KEY_DOWN)); d.push_back(Edge(t + 600, KEY_UP));
        std::vector<Emit> v = play(c, d);
        okEq((uint32_t)v.size(), 44, "dits: all edges emitted");
        if (v.size() == 44) {
            okEq(mark(v, 20), 600, "a 5-dit pause is inside a 7-dit hang: untouched");
            okEq(mark(v, 21), 700, "a 10-dit pause is beyond it: the mark starts 10 ms early");
        }
    }
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--vectors") == 0) { emitVectors(); return 0; }

    testCrypto();
    testLayout();
    testRoundTrip();
    testRejection();
    testReplay();
    testSpeedEstimator();
    testEdgeQueue();
    testKeyerRedundancy();
    testSendSchedule();
    testGlitchFilter();
    testBreakIn();
    testSafety();
    testRigFailSafe();
    testSimulation();

    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
