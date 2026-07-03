// Host-side tests for BleByteRing.h (BLE Serial, Morserino-32).
// Build:  clang++ -std=c++17 -Wall -Wextra -Werror -I"<src dir>" test_blebytering.cpp -o test_ring && ./test_ring
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "BleByteRing.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void testRingBasics() {
  BleByteRing<16> r;
  const uint8_t msg[] = "hello";
  CHECK(r.produce(msg, 5));
  CHECK(r.used() == 5);
  uint8_t b = 0;
  for (int i = 0; i < 5; ++i) { CHECK(r.consume(b)); CHECK(b == msg[i]); }
  CHECK(!r.consume(b));           // empty
  CHECK(r.used() == 0);
}

static void testWrapAround() {
  BleByteRing<16> r;
  uint8_t d[10], b;
  // push/pop 1000 times x 10 bytes -> indices wrap the pow2 boundary and beyond uint16 range/4
  for (int round = 0; round < 1000; ++round) {
    for (int i = 0; i < 10; ++i) d[i] = (uint8_t)(round + i);
    CHECK(r.produce(d, 10));
    for (int i = 0; i < 10; ++i) { CHECK(r.consume(b)); CHECK(b == (uint8_t)(round + i)); }
  }
  CHECK(r.used() == 0);
}

static void testUint16FreeRunningWrap() {
  BleByteRing<1024> r;
  uint8_t d[64], b;
  // 2000 rounds x 64 bytes = 128000 > 65536: head/tail wrap uint16_t several times
  for (int round = 0; round < 2000; ++round) {
    for (int i = 0; i < 64; ++i) d[i] = (uint8_t)(round ^ i);
    CHECK(r.produce(d, 64));
    for (int i = 0; i < 64; ++i) { CHECK(r.consume(b)); CHECK(b == (uint8_t)(round ^ i)); }
  }
}

static void testOverflowAllOrNothing() {
  BleByteRing<16> r;
  uint8_t d[20];
  memset(d, 0xAA, sizeof(d));
  CHECK(r.produce(d, 12));
  CHECK(!r.produce(d, 8));        // 12 + 8 > 16: dropped whole
  CHECK(r.overflow);
  CHECK(r.used() == 12);          // ring content untouched
  r.overflow = false;
  CHECK(r.produce(d, 4));         // exactly full is fine
  CHECK(r.used() == 16);
  CHECK(!r.overflow);
}

static void testProduceSome() {
  BleByteRing<16> r;
  uint8_t d[24];
  for (int i = 0; i < 24; ++i) d[i] = (uint8_t)i;
  CHECK(r.produceSome(d, 24) == 16);   // accepts what fits
  CHECK(r.produceSome(d, 4) == 0);     // full
  uint8_t b;
  for (int i = 0; i < 16; ++i) { CHECK(r.consume(b)); CHECK(b == i); }
}

static void testMarkReset() {
  BleByteRing<64> r;
  const uint8_t stale[] = "put cw/play/OLD\n";
  const uint8_t fresh[] = "put device/protocol/on\n";
  // old session leaves unread bytes (incl. a torn line)
  CHECK(r.produce(stale, sizeof(stale) - 1));
  CHECK(r.produce((const uint8_t *)"put cw/pl", 9));    // torn
  // new central connects: producer task latches the mark...
  r.latchMark();
  // ...and writes its first command BEFORE the consumer ever runs
  CHECK(r.produce(fresh, sizeof(fresh) - 1));
  // consumer-side session reset: stale bytes below the mark vanish, early first write survives
  r.resetToMark();
  uint8_t b;
  for (size_t i = 0; i < sizeof(fresh) - 1; ++i) { CHECK(r.consume(b)); CHECK(b == fresh[i]); }
  CHECK(!r.consume(b));
}

static void testMarkResetIdleSession() {
  BleByteRing<32> r;
  const uint8_t d[] = "abc";
  CHECK(r.produce(d, 3));
  r.latchMark();                  // connect with nothing written after
  r.resetToMark();
  uint8_t b;
  CHECK(!r.consume(b));           // empty, nothing replayed
  CHECK(r.used() == 0);
}

static void testStageChunk() {
  BleByteRing<64> r;
  uint8_t d[50], out[64];
  for (int i = 0; i < 50; ++i) d[i] = (uint8_t)(i + 1);
  CHECK(r.produce(d, 50));
  CHECK(bleStageChunk(r, out, 20) == 20);    // pre-negotiation MTU payload
  CHECK(out[0] == 1 && out[19] == 20);
  CHECK(bleStageChunk(r, out, 64) == 30);    // rest, capped by availability
  CHECK(out[0] == 21 && out[29] == 50);
  CHECK(bleStageChunk(r, out, 64) == 0);
}

static void testFlowGateBudget() {
  BleFlowGate f;
  uint32_t now = 1000;
  for (int i = 0; i < 4; ++i) { CHECK(f.canSend()); f.onNotified(); f.service(now); }
  CHECK(!f.canSend());            // budget exhausted at MAX_IN_FLIGHT
  f.confCount = (uint16_t)(f.confCount + 2);   // BT task confirms two
  CHECK(f.inFlight() == 2);
  CHECK(f.canSend());
}

static void testFlowGateCongestion() {
  BleFlowGate f;
  CHECK(f.canSend());
  f.congested = true;
  CHECK(!f.canSend());
  f.congested = false;
  CHECK(f.canSend());
}

static void testFlowGateWrapClamp() {
  BleFlowGate f;
  f.notifyCount = 3;
  f.confCount = 65530;            // desync/wrap: "in-flight" would be 9 -> clamp reads 0
  CHECK(f.inFlight() == 0);
  CHECK(f.canSend());
  f.confCount = 65535;            // difference 4: plausible, honored
  CHECK(f.inFlight() == 4);
  CHECK(!f.canSend());
}

static void testFlowGateWatchdog() {
  BleFlowGate f;
  uint32_t now = 5000;
  f.service(now);
  for (int i = 0; i < 4; ++i) f.onNotified();
  CHECK(!f.canSend());
  f.service(now + 100);           // pending, no progress, not stalled yet
  CHECK(!f.canSend());
  f.service(now + 100 + BleFlowGate::STALL_MS);   // stalled: resync
  CHECK(f.inFlight() == 0);
  CHECK(f.canSend());
  // progress resets the stall clock
  for (int i = 0; i < 2; ++i) f.onNotified();
  f.service(now + 3000);
  f.confCount = (uint16_t)(f.confCount + 1);      // BT task confirms one
  f.service(now + 3100);                          // progress observed
  CHECK(f.inFlight() == 1);
  f.service(now + 3100 + BleFlowGate::STALL_MS - 1);
  CHECK(f.inFlight() == 1);                       // not stalled yet after progress
}

static void testFlowGateResetSession() {
  BleFlowGate f;
  for (int i = 0; i < 3; ++i) f.onNotified();
  f.congested = true;
  f.resetSession(9000);
  CHECK(f.inFlight() == 0);
  CHECK(f.canSend());
}

static void testMillisWrapWatchdog() {
  BleFlowGate f;
  uint32_t now = 0xFFFFFF00u;     // close to uint32 wrap
  f.service(now);
  f.onNotified();
  f.service((uint32_t)(now + BleFlowGate::STALL_MS + 512));   // wraps past zero
  CHECK(f.inFlight() == 0);       // resynced despite millis() wrap
}

int main() {
  testRingBasics();
  testWrapAround();
  testUint16FreeRunningWrap();
  testOverflowAllOrNothing();
  testProduceSome();
  testMarkReset();
  testMarkResetIdleSession();
  testStageChunk();
  testFlowGateBudget();
  testFlowGateCongestion();
  testFlowGateWrapClamp();
  testFlowGateWatchdog();
  testFlowGateResetSession();
  testMillisWrapWatchdog();
  if (failures) { printf("%d FAILURES\n", failures); return 1; }
  printf("all BleByteRing host tests passed\n");
  return 0;
}
