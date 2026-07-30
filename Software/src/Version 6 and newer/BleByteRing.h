#ifndef BLEBYTERING_H_
#define BLEBYTERING_H_

/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                            ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

/// Plain-buffer building blocks for BLE Serial (MorseBleSerial.cpp): a
/// single-producer/single-consumer byte ring with a mark-based session reset,
/// and the notify flow-control policy. Deliberately free of BLE and Arduino
/// types so the tricky logic is host-testable (see devdocs/ble-serial/).
///
/// Threading contract (same discipline as ftpRxRing in MorsePileup.cpp):
/// exactly one task advances head (producer), exactly one task advances tail
/// (consumer). The session reset never violates this: the producer-side
/// connect callback latches connectMark = head (serialized with produce() on
/// the same Bluedroid task), and the consumer later jumps tail to that mark —
/// discarding old-session bytes while preserving anything the new central
/// wrote after connecting. Only when no producer can run (server torn down)
/// may hardReset() touch both indices.

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// compiler barrier: keep buffer writes from being reordered past the index
// publication (the ESP32 cores are cache-coherent; this is about the compiler)
static inline void bleRingBarrier() { __asm__ __volatile__("" ::: "memory"); }

template <uint16_t N>
struct BleByteRing {
  static_assert(N != 0 && (N & (N - 1)) == 0, "size must be a power of two");
  static_assert(N <= 16384, "free-running uint16_t indices need N well below 65536");

  volatile uint16_t head = 0;             // producer-owned, free-running (masked on access)
  volatile uint16_t tail = 0;             // consumer-owned, free-running
  volatile uint16_t connectMark = 0;      // latched by the producer task at session start/end
  volatile bool poisoned = false;         // set by producer on a dropped write, cleared by consumer once
                                          // drained: while set, NO new write is admitted, so a torn line can
                                          // never be spliced to post-overflow bytes (the seam is always at
                                          // ring-empty) — and it doubles as the report-once overflow latch
  uint8_t buf[N];

  uint16_t used() const { return (uint16_t)(head - tail); }
  uint16_t free_() const { return (uint16_t)(N - used()); }

  // producer: all-or-nothing — a torn command line must never enter the ring
  bool produce(const uint8_t *d, uint16_t len) {
    if (poisoned || len > free_()) {
      poisoned = true;
      return false;
    }
    copyIn(d, len);
    return true;
  }

  // producer variant for the loop-task-owned TX ring: accept what fits
  uint16_t produceSome(const uint8_t *d, uint16_t len) {
    uint16_t n = free_();
    if (n > len)
      n = len;
    if (n)
      copyIn(d, n);
    return n;
  }

  // consumer, single byte (line assembly)
  bool consume(uint8_t &b) {
    if (used() == 0)
      return false;
    b = buf[tail & (N - 1)];
    bleRingBarrier();
    tail = (uint16_t)(tail + 1);
    return true;
  }

  // consumer, bulk (chunk staging): up to two contiguous memcpy segments,
  // one barrier, one index publication
  uint16_t consumeBulk(uint8_t *dst, uint16_t maxLen) {
    uint16_t n = used();
    if (n > maxLen)
      n = maxLen;
    if (n == 0)
      return 0;
    uint16_t t = tail;
    uint16_t idx = t & (N - 1);
    uint16_t first = (uint16_t)(N - idx) < n ? (uint16_t)(N - idx) : n;
    memcpy(dst, buf + idx, first);
    if (n > first)
      memcpy(dst + first, buf, (size_t)(n - first));
    bleRingBarrier();
    tail = (uint16_t)(t + n);
    return n;
  }

  // producer task only. Must be called at BOTH session edges (connect AND
  // disconnect): the mark separates sessions, and a stale mark would make
  // resetToMark() rewind tail over already-consumed bytes and replay them.
  void latchMark() { connectMark = head; }
  void resetToMark() { tail = connectMark; }  // consumer task only; valid while the mark is fresh (see above)
  void clearPoisoned() { poisoned = false; }  // consumer task only, after draining to empty
  void hardReset() {                          // ONLY legal with the producer stopped (server down)
    head = tail = connectMark = 0;
    poisoned = false;
  }

 private:
  // up to two contiguous memcpy segments, one barrier, one index publication
  void copyIn(const uint8_t *d, uint16_t n) {
    uint16_t h = head;
    uint16_t idx = h & (N - 1);
    uint16_t first = (uint16_t)(N - idx) < n ? (uint16_t)(N - idx) : n;
    memcpy(buf + idx, d, first);
    if (n > first)
      memcpy(buf, d + first, (size_t)(n - first));
    bleRingBarrier();
    head = (uint16_t)(h + n);
  }
};

// consumer: stage up to maxLen ring bytes into a contiguous chunk buffer
template <uint16_t N>
static inline uint16_t bleStageChunk(BleByteRing<N> &r, uint8_t *dst, uint16_t maxLen) {
  return r.consumeBulk(dst, maxLen);
}

/// Notify flow control (PLAN D17). Two monotonic single-writer counters:
/// notifyCount is written only by the loop task (after each notify()),
/// confCount only by the Bluedroid task (ESP_GATTS_CONF_EVT — which fires on
/// hand-off to the controller for notifications, so credits pace the queue,
/// they do not confirm delivery). in-flight = uint16 difference; a shared
/// read-modify-write credit would lose updates across the two cores.
struct BleFlowGate {
  static const uint16_t MAX_IN_FLIGHT = 4;    // notifies allowed into the stack at once
  static const uint16_t WRAP_CLAMP = 8;       // an impossible in-flight reads as desync -> treat as empty
  static const uint32_t STALL_MS = 2000;      // watchdog: resync if confirmations stop arriving

  volatile uint16_t notifyCount = 0;          // loop-task-owned
  volatile uint16_t confCount = 0;            // BT-task-owned
  volatile bool congested = false;            // BT-task-owned (ESP_GATTS_CONGEST_EVT)
  uint16_t lastConf = 0;                      // watchdog bookkeeping, loop task only
  uint32_t lastProgress = 0;

  uint16_t inFlight() const {
    uint16_t d = (uint16_t)(notifyCount - confCount);
    return (d > WRAP_CLAMP) ? 0 : d;
  }
  bool canSend() const { return !congested && inFlight() < MAX_IN_FLIGHT; }
  void onNotified() { notifyCount = (uint16_t)(notifyCount + 1); }  // loop task

  // watchdog, called from the loop task with a monotonic ms clock: if sends
  // are pending but confCount has not moved for STALL_MS, resync by writing
  // notifyCount (which the loop task owns) to the confCount snapshot
  void service(uint32_t nowMs) {
    uint16_t c = confCount;
    if ((uint16_t)(notifyCount - c) == 0 || c != lastConf) {
      lastConf = c;
      lastProgress = nowMs;
      return;
    }
    if ((uint32_t)(nowMs - lastProgress) >= STALL_MS) {
      notifyCount = c;
      lastProgress = nowMs;
    }
  }

  // session reset, loop task. congested is BT-task-owned; clearing it here
  // races only a connection that is being torn down anyway and self-corrects
  // on the next CONGEST event of the new connection
  void resetSession(uint32_t nowMs) {
    notifyCount = confCount;
    congested = false;
    lastConf = confCount;
    lastProgress = nowMs;
  }
};

#endif /* #ifndef BLEBYTERING_H_ */
