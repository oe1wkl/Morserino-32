# cw/play text path — host test

What the M32 sounds for `PUT cw/play/<text>`, checked on the host against the firmware's own
code. `extract.py` copies the functions verbatim out of `m32_v6.ino` (`getM32PWord()`,
`cleanUpText()`, `utf8umlaut()`, `generateCWword()`, `playCWHasMore()`, the `CWchars`/`pool`
tables) and the Koch string out of `MorsePreferences.h` into `extracted.inc`. The test then
runs `fetchNewWord()`'s playCW loop over them. No device, no PlatformIO:

```
make run
```

It pins down that prosigns survive `cw/play` (`<sk>`, `[sk]`, `\sk`, any case, alone or inside a
group), and that `playCWHasMore()` sees the last word. That's what the M32 Trainer relies on. The
Makefile re-extracts the code whenever `m32_v6.ino` changes. If a function is renamed, `extract.py`
stops with the name it could not find.

Checked by mutation: without the `utf8umlaut()` call in `cleanUpText()`, 12 of the 18 checks fail.
`string_shim.h` is just enough of Arduino's `String` for this code.
