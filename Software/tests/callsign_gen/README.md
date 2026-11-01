# getRandomCall() on the host — fixture for the M32 Trainer

`extract.py` copies the firmware's call sign generator (`getRandomCall()` and its helpers) verbatim out of
`m32_v6.ino`. `gen_fixture.cpp` runs it against `callsign_prefixes.h`, with `random()` replaced by the
M32 Trainer engine's generator (`mulberry32`, and `random(a, b)` = `a + floor(r·(b−a))`). It writes the
calls for every Call Length × four regions × Common on/off × two seeds to
`../trainer/fixtures/random_calls.json`:

```
make run
```

`../trainer/callsign.test.js` then checks that the engine's `randomCall()` makes exactly the same calls,
continents and CQ zones. **Re-run `make run` after any change to `getRandomCall()` or the prefix table,
and commit the new fixture.** Not covered: the VK/ZL-only generator, which the trainer doesn't port.
