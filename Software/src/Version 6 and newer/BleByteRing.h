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

// compiler barrier: keep buffer writes from being reordered past the index
// publication (the ESP32 cores are cache-coherent; this is about the compiler)
static inline void bleRingBarrier() { __asm__ __volatile__("" ::: "memory"); }

template <uint16_t N>
struct BleByteRing {
  static_assert(N != 0 && (N & (N - 1)) == 0, "size must be a power of two");
  static_assert(N <= 16384, "free-running uint16_t indices need N well below 65536");

  volatile uint16_t head = 0;             // producer-owned, free-running (masked on access)
  volatile uint16_t tail = 0;             // consumer-owned, free-running
  volatile uint16_t connectMark = 0;      // latched by the producer task at session start
  volatile bool overflow = false;         // set by producer on a dropped write, cleared by consumer
  uint8_t buf[N];

  uint16_t used() const { return (uint16_t)(head - tail); }
  uint16_t free_() const { return (uint16_t)(N - used()); }

  // producer: all-or-nothing — a torn command line must never enter the ring
  bool produce(const uint8_t *d, uint16_t len) {
    if (len > free_()) {
      overflow = true;
      return false;
    }
    uint16_t h = head;
    for (uint16_t i = 0; i < len; ++i)
      buf[(uint16_t)(h + i) & (N - 1)] = d[i];
    bleRingBarrier();
    head = (uint16_t)(h + len);
    return true;
  }

  // producer variant for the loop-task-owned TX ring: accept what fits
  uint16_t produceSome(const uint8_t *d, uint16_t len) {
    uint16_t n = free_();
    if (n > len)
      n = len;
    if (n == 0)
      return 0;
    uint16_t h = head;
    for (uint16_t i = 0; i < n; ++i)
      buf[(uint16_t)(h + i) & (N - 1)] = d[i];
    bleRingBarrier();
    head = (uint16_t)(h + n);
    return n;
  }

  // consumer
  bool consume(uint8_t &b) {
    if (used() == 0)
      return false;
    b = buf[tail & (N - 1)];
    bleRingBarrier();
    tail = (uint16_t)(tail + 1);
    return true;
  }

  void latchMark() { connectMark = head; }    // producer task only (connect callback)
  void resetToMark() { tail = connectMark; }  // consumer task only: tail <= mark <= head always holds
  void hardReset() {                          // ONLY legal with the producer stopped (server down)
    head = tail = connectMark = 0;
    overflow = false;
  }
};

// consumer: stage up to maxLen ring bytes into a contiguous chunk buffer
template <uint16_t N>
static inline uint16_t bleStageChunk(BleByteRing<N> &r, uint8_t *dst, uint16_t maxLen) {
  uint16_t n = 0;
  uint8_t b;
  while (n < maxLen && r.consume(b))
    dst[n++] = b;
  return n;
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
