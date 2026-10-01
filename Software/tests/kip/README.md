# M32KIP — protocol core tests

Host tests for **M32KIP**, the remote-keying protocol described in `devdocs/m32kip/SPEC.md`.
They need no Morserino and no Arduino toolchain: the protocol core
(`Software/src/Version 6 and newer/M32Kip.{h,cpp}`) is deliberately free of Arduino, FreeRTOS and
ESP-IDF dependencies precisely so that the parts that are easy to get subtly wrong — the wire
layout, the MAC derivation, the replay window, the jitter-buffer reconstruction — can be tested
on a laptop, at speed, under sanitizers.

```sh
make                      # build and run the C++ tests
python3 check_vectors.py  # rebuild the same packets in Python and compare byte for byte
```

Both are hardware-free and belong in CI.

## What is here

| File | What it does |
|---|---|
| `test_core.cpp` | The C++ tests: SHA-256/HMAC against the published RFC vectors, packet layout against the spec's byte tables, codec round trips, everything that must be *refused* (wrong key, every single-bit flip, wrong magic, wrong version, truncation, inconsistent edge counts), and the replay window including its behaviour across the 16-bit sequence wrap. |
| `check_vectors.py` | An independent implementation of the wire format in Python (`struct` + `hashlib` + `hmac`). It rebuilds the packets `test_core --vectors` prints and compares them byte for byte. Two implementations agreeing is what makes the format trustworthy; the C++ tests alone would only prove the codec is self-consistent. |
| `reference_peer.py` | A working Keyer unit and Rig unit in Python, for testing one end of the link against something that is not the other end of the same firmware. `keyer` plays a CW message at a Morserino running in Rig mode; `rig` reconstructs the key line from a Morserino running in Keyer mode and prints it. |
| `hello_probe.py` | Asks a Rig unit whether it is free and names the reply: `NACK` busy (a Keyer is linked — the hands-off link proof), version/auth refusals, or `HELLO_ACK` (it was free; the won session is released with three BYEs). Run from Terminal — a tool shell's UDP is blocked by macOS Local Network privacy. |
| `spike_load.py` | Phase 0 leftover: a UDP echo flooder for the timing spike (`M32KipSpike.cpp`). Also useful as a quick link-quality probe. See `devdocs/m32kip/PHASE0_RESULTS.md`. |
| `Makefile` | Builds `test_core` with the address and undefined-behaviour sanitizers, so a buffer slip in a codec fails the run here rather than becoming a field bug on the ESP32. |

## The simulation

The last and most valuable section of `test_core.cpp` is an end-to-end one. A CW stream is generated,
keyed through the real encoder, pushed through a network model, decoded, and reconstructed by a real
`RigSession`; the reproduced key line is then compared with the original, mark by mark and gap by gap.
The network model runs the impairment profiles of spec §13.2 — clean link, 120 ms with 40 ms jitter,
the same with 2 % and 5 % loss, bursts of three lost packets — at 15, 25 and 35 WPM, plus a clock-drift
run of a thousand marks.

What it asserts is graded by what each thing is for, rather than one blanket tolerance:

- **No mark is ever shortened.** No profile, no speed, no exception. This is design principle 5 and the
  one promise the whole thing rests on.
- **Spacing inside a character never moves** by more than 2 ms: that is what would change the rhythm a
  listener hears.
- **A word gap may lose one offset step and one playout step**, because word gaps are exactly where the
  reconstruction is allowed to take up clock drift and shorten the playout delay.
- **On the burst profile, at most one mark is disturbed per burst**, which is the spec's own weaker
  acceptance for that case.

Three protocol findings came out of these runs rather than out of reading the specification. They are
written up in `devdocs/m32kip/PHASE1_FINDINGS.md`.

## Why the bit-flip test

`testRejection()` flips every single bit of a valid HELLO in turn and insists each one is refused.
That is 288 checks for one packet, and it is the cheapest possible guard on the one property the
whole safety story rests on: **nobody but the licensed operator can key the transmitter** (spec
§1.2). A truncated 8-byte MAC is only worth what the verification is worth.
