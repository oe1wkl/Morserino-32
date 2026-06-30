# QSO Bot matcher — host unit tests

Off-device unit tests for the pure token-classification primitives in
`Software/src/Version 6 and newer/MorseQsoBotMatch.h` (callsign shape, RST /
cut numbers, contest zone/serial, prosigns, noise filtering, the
end-of-over / repeat / slot-keyword tokens, and the `SLOT_INFO` keyword-field
parser).

These compile the **same header the firmware uses**, against a tiny Arduino
`String` shim (`arduino_string_shim.h`) — so a green run means the firmware's
per-token decisions are unchanged. Only a C++11 compiler is needed; no device,
no PlatformIO.

```sh
make run     # build + run; exits non-zero on failure (CI-friendly)
make         # build only
make clean
```

## Scope

Covered: every *pure* decision the matcher makes on a single token (plus the
multi-token `InfoParser` accumulation). This is the layer that has produced
real regressions — e.g. the "callsign starting with K triggers a premature
qrz?" bug (commit `28c03f3`), guarded here by `test_noise()` /
`test_callsign_shape()`.

Not covered (runtime/integration, lives in `MorseQsoBot.cpp`): timeouts and
turn-taking, the concat-buffer reassembly of split tokens (`5 9 9` → `599`),
the CW engine, and display. Those need the device or a much larger harness.

## Callsign shape

`looksLikeCallsign` parses a call from the end as `[prefix][area-digit][suffix]`
with an optional `/TAIL`, so digit-leading prefixes (`2E0…`, `4X1…`, `9A1…`,
`3D2…`) are recognised while RST-like tokens (`5NN`, `599`) are still rejected
(T3.4). The tests pin both the accepted and rejected forms.

## Adding cases

When you change matcher behaviour, update `MorseQsoBotMatch.h` and add/adjust a
`CHECK(...)` in `test_qso_bot_matcher.cpp`. If a primitive needs new `String`
methods, extend the shim to match Arduino semantics.
