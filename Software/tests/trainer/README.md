# M32 Trainer tests

Host-side tests for the M32 Trainer page (`Software/Utilities/m32_trainer*`). They need
Node 18 or newer and no device:

```
node --test Software/tests/trainer/*.test.js
```

(Pass the files, not the folder: `node --test <folder>` treats a folder as a script.)

| File | What it covers |
|---|---|
| `link.test.js` | `m32_trainer_link.js`, the link to the Morserino: framing of the mixed JSON/echo stream (every split point, random chunking, noise, braces/quotes/escapes inside strings, torn and stray objects, the stall rule), the one-in-flight command queue, reply vs unsolicited routing, timeouts and the re-handshake, `cw/play` → `done` bookkeeping, cable loss and reopen, BLE consent, goodbye and `pagehide` |

Time is a fake clock (`fakeClock()` in the test file), so every timeout is exercised
exactly and the suite runs in well under a second. The device is a scripted transport
(`fakeM32()`); the objects it sends are the firmware's own, the `cw`/`trainer` events
captured on the M1 bench (`Software/tests/protocol/selftest.py`).

The tests were checked by breaking the module on purpose (quote and escape handling, the
salvage after a stall, the late-reply hold, the re-handshake, `replaced`, event-vs-reply
matching, the framer reset at the handshake, the `late` flag, BLE consent, the `pagehide`
steps): each break makes at least one test fail. Add a test whenever a link bug is
found on the bench.

Not covered here: `openWebSerial()`, which needs a browser and a device. It is benched with
the page (M3).
