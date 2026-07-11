# TODO(audit) Resolutions

Factual answers to every `TODO(audit)` marker in `CLAUDE.md` and
`devdocs/UX_CONVENTIONS.md`, with file/function citations. **Do not edit those
two documents from this file** — these are findings for Willi to fold in.

All source citations are relative to `Software/src/Version 6 and newer/` unless
noted. Line numbers are at audit HEAD (`9478983`).

Where the code gives multiple conflicting answers, all are listed and the
conflict is cross-referenced to `divergences.md` rather than resolved here.

---

## Part A — CLAUDE.md markers

### A1. §1 line 31 — "list all existing config flags and where they are defined"

**Defined in:** `Software/src/platformio.ini`, per-environment `build_flags`
(grouped into reusable `[common]` fragments). They are plain `-D` macros; there
is no central header of feature flags.

Complete list of compile-time flags that gate behavior (frequency = number of
`#if(def)` sites in the firmware):

| Flag | Meaning | Notes |
|---|---|---|
| `CONFIG_TFT` | Pocket / LCD (LovyanGFX) build | 53 sites — the master variant switch |
| `CONFIG_CW_GAME` | Compiles the four games | 37 sites; TFT-only in practice |
| `CONFIG_MCP73871` | MCP73871 power-path / battery | 29 sites (Pocket Wroom) |
| `CONFIG_BLUETOOTH_KEYBOARD` | BT-keyboard CW output | 28 sites |
| `CONFIG_BLE_SERIAL` | M32 serial protocol over BLE (NUS GATT server) | added 2026-07; **both** canonical variants; see `devdocs/ble-serial/` |
| `CONFIG_QSO_BOT` | QSO Bot mode | 22 sites; **both** variants |
| `LORA_RADIOLIB` | LoRa present (derived: set when `LORA_DISABLED` unset) | 17 sites |
| `CONFIG_TLV320AIC3100` | TLV320 audio codec (+`_INT`,`_RST`,`_SDA`,`_SCL`) | 15 sites |
| `CONFIG_SOUND_I2S` | I2S sidetone path | 14 sites |
| `LORA_DISABLED` | Removes LoRa Trx menu entry + radio | 12 sites; **set on Pocket** |
| `INTERNAL_PULLUP` | Encoder/paddle pull-ups | 11 sites |
| `ORIGINAL_M32` | Classic Heltec V2 board | 5 sites |
| `CONFIG_DECODER_I2S` | I2S-based audio decoder | 5 sites |
| `TOUCHPADDLES_DISABLED` | No capacitive paddles | 4 sites |
| `CONFIG_ENGLISH_OXFORD` | Oxford word list selection | 4 sites |
| `CONFIG_WM8960` | WM8960 codec (Heltec V3) | 3 sites |
| `SKIP_BATTERY_PROTECT`, `SKIP_LEGACY_PINDEFS` | misc guards | 1–2 sites |

Plus pin-mapping `-D`s (`OLED_*`, `PIN_*`, `TFT_*`, `LoRa_*`, `CONFIG_I2S_*_PIN`,
`CONFIG_*_SDA/SCL`) that are configuration, not feature gates.

**Divergence flagged:** variant gating is *not* always done through these named
feature flags — there are bare board/display `#ifdef`s in several places (e.g.
`#ifdef ARDUINO_heltec_wifi_kit_32_V3` for `VOLT_CALIBRATE` in `morsedefs.h:160`;
`#ifndef CONFIG_TFT` to choose `NoOfVisibleLines` in `MorseOutput.h:20`). See
`divergences.md` → "Ad-hoc variant gating".

### A2. §1 line 35 — "exact PlatformIO environment names and build commands per variant"

Run from `Software/src/` (where `platformio.ini` lives; `src_dir = Version 6 and newer`).

**Two canonical variants** (the ones every change must build):

```
# Classic M32 (OLED, original Heltec V2, has LoRa, no Games)
pio run -e heltec_wifi_lora_32_V2

# M32 Pocket (LCD/TFT, ESP32-S3 Wroom — this is default_envs)
pio run -e pocketwroom
```

All environments (9 total):

| Env | Display | Variant class | Notes |
|---|---|---|---|
| `heltec_wifi_lora_32_V2` | OLED | Classic | `ORIGINAL_M32`, LoRa **on** |
| `heltec_wifi_kit_32_V3` | OLED | Classic-ish | WM8960 codec |
| `heltec_wifi_lora_32_V3` | OLED | Classic-ish | LoRa on, WM8960 |
| `ESP32_S3_Devkit` | OLED | dev | Wroom, no touch paddles |
| `minipcb_lora` | OLED | Classic | LoRa on |
| `heltec-vision-master-t190` | TFT | Pocket | TLV320, Games |
| `pocketwroom` | TFT | **Pocket (default)** | Games, QSO Bot, LoRa off |
| `pocketwroom-lora` | TFT | Pocket | adds SX1262 LoRa |
| `pocketwroom-170x240` | TFT | Pocket | 240-wide panel variant |

**Build check at HEAD (audit ran both):** `heltec_wifi_lora_32_V2` →
**SUCCESS** (Flash 60.2 %, RAM 29.5 %); `pocketwroom` → **SUCCESS** (Flash
65.9 %, RAM 36.5 %). A pre-build hook `scripts/clean_ino_dupes.py`
(`extra_scripts` in `[env]`) strips stray `<sketch>.ino N.cpp` duplicates first —
see `divergences.md` → "Stray `m32_v6.ino 2.cpp`".

### A3. §2 line 45 — directory map

| Concern | Files |
|---|---|
| Main sketch / dispatch / classic mode loops | `m32_v6.ino` (`setup()`, `loop()`, keyer `doPaddleIambic()`, generator `generateCW()`/`fetchNewWord()`, `echoTrainerEval()`) |
| Iambic CW engine for games | `MorseCwEngine.cpp/.h` |
| Decoder | `MorseDecoder.cpp/.h` (`Decoder` class); tone detect `goertzel.cpp/.h`, `analog.c/.h` |
| Koch tables + word/call sources | `Koch` class in `MorsePreferences.cpp/.h`; `english_words.h/.cpp`, `abbrev.h`, `callsign_prefixes.h`; `getRandomCall()` etc. in `m32_v6.ino` |
| Output layer (display, sound, scroll, TOT) | `MorseOutput.cpp/.h` (`pwmTone`, `printToScroll`, `resetTOT`) |
| Display abstraction | TFT: **`oe1wkl/DisplayWrapper`** (external lib). OLED: `M32OledLGFX.h` (in-tree LGFX). **`M32PocketLGFX.h` = orphaned dead code** (reverted #157, included nowhere) |
| Menu / preferences / NVS | `MorseMenu.cpp/.h`; `MorsePreferences.cpp/.h` |
| Serial (M32) protocol | `MorseJSON.cpp/.h`; `serialEvent()`/`serialDecode()` in `m32_v6.ino` |
| Games | `MorseGame.cpp` (Morse Invaders), `MorsePileup.cpp` (Fight the Pileup), `MorseRadioCave.cpp` (Radio Cave), `MorseMorsel.cpp` (Morsel); shared: `MorseGameMode.cpp/.h`, `GameSprite.cpp/.h`, `GamePalette.h` |
| QSO Bot | `MorseQsoBot.cpp/.h`, `qso_content.h` |
| ESPNow / WiFi / LoRa multiplayer + Trx | `MorseWiFi.cpp/.h`; `onEspnowRecv()`/`setupESPNow()`/`wifiWarmup()`; `sendWithLora()`/`onLoraReceive()` in `m32_v6.ino`; QuickEspNow (pinned fork) |
| Bluetooth | `MorseBluetooth.cpp/.h` |
| Fonts | `wklfonts.cpp/.h` (OLED byte-array), `IntelOneMono_*`, `DejaVuSansMono_*` (TFT) |
| Manuals (EN+DE) + PDF pipeline | `Documentation/User Manual/`; PDF filter `Documentation/User Manual/Version 8.x/normalize_ids.lua` (pandoc + weasyprint) |

### A4. §3 line 80 — additional invariants uncovered

Candidate invariants to add to the hard-rules list (evidence in code):

1. **A game/bot must never inherit a transmit `morseState`.** Games run under
   the dedicated `morseGame` state and the bot under `morseQsoBot`; both are
   excluded from every LoRa/WiFi/external-TX gate so they are local-sidetone-only
   (`morsedefs.h:324-333` comments; `keyOut()` gates `m32_v6.ino:3219-3232`).
   Reusing a stale `loraTrx`/`wifiTrx` state in a game dereferences torn-down
   subsystems and crashes (`MorseMenu.cpp:684-693`).
2. **`prefPos` additions need three parallel arrays in sync** — the `prefPos`
   enum (`morsedefs.h:387`), `pliste[]`, and `prefName[]` (`MorsePreferences.h:173,299`).
   A missing `prefName[]` entry is a silent boot-time NVS panic, not a compile
   error. (Already known; worth promoting to a hard rule.)
3. **`NoOfVisibleLines` differs by variant** (3 OLED / 4 TFT, `MorseOutput.h:20-26`)
   — confirms the existing §3.6 rule; never hardcode.
4. **`checkEncoder()` returns only -1/0/+1** and has a 10 ms debounce
   (`m32_v6.ino:426`). Modes must not assume multi-step deltas per call.

### A5. §4 line 85 — NVS namespace + key scheme + versioning

**There is no single canonical scheme — three namespaces are in use:**

| Namespace | Owner(s) | Representative keys |
|---|---|---|
| `morserino` | All preferences, snapshots, file-player; **+ Morsel scores, Fight-Pileup & QSO-Bot identity** | `wpm`, `sidetoneVolume`, `brightness`, `kochFilter`, `theme`, `lastExecuted`, `wlanSSID*`, `playerCall`, `playerName`, Morsel `hi`/`hv`/`wlen` |
| `m32game` | Morse Invaders only | `hs%d_s`, `hs%d_k`, `hs%d_l`, `hs%d_w` (top-N table) |
| `radiocave` | Radio Cave only | `save` (one blob) |

Key naming is **flat, ad-hoc** (`wpm`, `theme`, `hi`) — no `game.<name>.<item>`
convention exists anywhere. Citations: `MorsePreferences.cpp` (many `prefs.begin("morserino")`),
`MorseGame.cpp:161,178` (`m32game`), `MorseRadioCave.cpp:263-296` (`radiocave`),
`MorseMorsel.cpp:1154` & `MorsePileup.cpp:312-326` & `MorseQsoBot.cpp:187`
(`morserino`).

**Versioning:** preferences carry `version_major`/`version_minor` keys
(`MorsePreferences.h:71-72`); on read these gate a defaults/migration path. The
**game** namespaces (`m32game`, `radiocave`) and Morsel's `hi/hv` have **no
version stamp** — a struct-layout change to `radiocave`'s `save` blob or to the
Invaders score keys would load incompatible data with no guard. See
`divergences.md` → "NVS namespace sprawl" and "Unversioned game save-state".

### A6. §5 line 99 — English strings + display length limits

- **All device strings are English** (spot-checked across menu, modes, games);
  the only non-ASCII is the `©` copyright glyph stored as UTF-8 bytes
  (`morsedefs.h:41`).
- **Length limits:** scroll/status width is `NoOfCharsPerLine` and lines are
  `NoOfLines`/`NoOfVisibleLines` (`MorseOutput.h`). Status line writes are
  position-indexed (e.g. WpM at columns 7–12, `m32_v6.ino:2407-2428`), so status
  strings have hard column budgets. OLED is the tighter target (3 visible lines).
  No single documented "max chars" constant beyond `NoOfCharsPerLine`; menu
  labels are hand-fitted (e.g. `"Update Firmw"`, `"Disp MAC Addr"` in
  `MorseMenu.cpp:67-127`).

### A7. §5 line 104 — large word lists storage

`english_words.h` (~118 KB), `callsign_prefixes.h` (~27 KB) and `abbrev.h` are
plain `const` arrays — **not** wrapped in `PROGMEM`/`pgm_read_*` (grep: 0 PROGMEM
in `english_words.h`/`abbrev.h`, 1 in `callsign_prefixes.h`). This is correct
practice **for ESP32**: the toolchain places `const` data in flash `.rodata`
and the CPU reads it directly through the flash cache, so AVR-style `PROGMEM`
accessors are unnecessary. (Fonts in `wklfonts.cpp` do use `PROGMEM`, harmless.)
Practice to document: "large constant tables are `const` (flash-resident on
ESP32); do not add `PROGMEM` accessors."

---

## Part B — UX_CONVENTIONS.md markers

The classic modes are driven by the global `loop()` state machine
(`m32_v6.ino:884-1175`); button semantics there are the **incumbent standard**.

### B1. line 22 — games complete list

**Morse Invaders, Fight the Pileup, Radio Cave, Morsel** (`morsedefs.h:365`
menu enum; labels `MorseMenu.cpp:116`). All four are `CONFIG_CW_GAME`, TFT-only.

### B2. line 34 + B3 line 40 — black knob **turn** in running classic modes

Encoder turn dispatches on `encoderState` (`m32_v6.ino:1121-1153`):
`speedSettingMode → changeSpeed()`, `volumeSettingMode → changeVolume()`,
`scrollMode → scroll history`, `memSelMode → select keyer memory`. **Default
on mode entry is `speedSettingMode`** (`MorseMenu.cpp:267`), so the de-facto
answer is **"turn = speed"** until the user changes the encoder's role with the
red button. **Confirmed.**

### B4. line 41 — black knob **click** (single) in running modes

`modeButton.clicks == 1` (`m32_v6.ino:1081-1107`): in Generator/Echo Trainer =
**start/stop** (toggles `genIsActive`); in Keyer/CW-Trx = **select keyer memory**
(`memoryKeyer()`); in `memSelMode` = confirm the selected memory. So single
click is **context-dependent** (start-stop vs memory). **Confirmed** (and noted
as an intra-classic divergence — see `divergences.md`).

### B5. line 42 — black knob **double click**

`modeButton.clicks == 2` → **enter the preferences menu**
(`setupPreferences()`, `m32_v6.ino:1108`). **The document's guess ("volume
mode?") is wrong** — volume is the red button (B8). Double-click black = prefs,
everywhere in classic modes and in the QSO Bot (`MorseQsoBot.cpp:961`).

### B6. line 44 — red button universal meaning (classic)

`volButton` (`m32_v6.ino:1033-1074`): **single click** toggles the encoder
between **speed and volume** setting (and exits scroll); **long press** toggles
**scroll mode** (scroll back through history); **double click** steps **display
brightness** (`decreaseBrightness()`). The red button is *not* start/stop or
pause in classic modes — it is the speed/volume/scroll/brightness modifier.

### B7. line 62 — speed (WPM) change mechanism

Encoder **turn while `encoderState == speedSettingMode`** → `changeSpeed(t)`
(`m32_v6.ino:1125`, `2525`). Writes `MorsePreferences::wpm`, clamps to
`wpmMin..wpmMax`, recomputes timings, repaints the WpM slot. **Confirmed.**
Echo-Trainer adaptive speed and the prompt-speed limiter call the same
`changeSpeed()` (`echoTrainerEval()` `m32_v6.ino:2493,2508`), so the user's
setting is mutated and **restored** via `echoPromptSpeed` bookkeeping
(`m32_v6.ino:2474-2478`).

### B8. line 68 — volume mechanism

**Red single click → `volumeSettingMode`**, then encoder turn → `changeVolume(t)`
(`m32_v6.ino:1044-1058`, `2537`). A second red click returns to speed and
**persists** via `writeVolume()` (`m32_v6.ino:1046`). **Confirmed.**

### B9. line 71 — pitch set in preferences only

**Confirmed.** Pitch is `posPitch` (`morsedefs.h:391`), a preferences item only;
the running tone uses `pitch()` (`m32_v6.ino:2068`) reading that pref. No mode
exposes a live pitch control.

### B10. line 73 — preferences entry gesture + from which states

**Black knob double-click** (B5), available from any classic running state and
the QSO Bot. In Generator/Echo it sets `stopFlag` so the current run halts first
(`m32_v6.ino:1111-1114`). From the **top menu** the same double-click opens the
full preferences set (`MorseMenu.cpp:316`). Games do **not** offer prefs
mid-game — their settings live in the lobby (allowed by §2's escape clause).
**Document's guess ("long press red?") is wrong.**

### B11. line 77 — saved immediately vs on exit

- **Immediately:** volume on speed/volume toggle (`writeVolume()`); brightness on
  change (`writeBrightnessPreference()`); file-player word pointer
  (`writeWordPointer()`); last-executed menu item (`writeLastExecuted()`); game
  high scores at game-over; player identity at entry.
- **On preferences exit:** all `pliste[]` values via `writePreferences("morserino")`.
- **WPM:** held in RAM (`MorsePreferences::wpm`) during a session and persisted as
  part of `writePreferences`/snapshots — *not* written on every encoder tick.
See `divergences.md` for the immediate-vs-deferred inconsistency.

### B12. line 84 — classic mode entry / armed state

Each classic mode shows a brief start splash via `showStartDisplay()` then:
- **Keyer** (`"CW Keyer Start"`), **CW-Trx/iCW**, **LoRa/WiFi Trx**, **Decoder**:
  immediately **active** (paddle keys / decoder runs). `MorseMenu.cpp:457,620,663,731`.
- **Generator** (`"Generator / Start-Stop / press Paddle"`) and **Echo Trainer**
  (`"Echo Trainer: / Start: / Press paddle"`): enter **armed**, waiting for a
  paddle touch or black-knob click to begin (`MorseMenu.cpp:502,548`;
  `loop()` `m32_v6.ino:954,977`).

### B13. line 87 — game start gesture (click vs first CW input)

**Not unified today:**
- Morse Invaders: **paddle/touch** ("Touch:start", `MorseGame.cpp:661,701`).
- Morsel: **click *or* key** ("Click or key to start", `MorseMorsel.cpp:642`).
- Radio Cave: **click** ("Click … to start", `MorseRadioCave.cpp:734-738`).
- Fight the Pileup: **click** from the lobby (`MorsePileup.cpp:926`).
→ `divergences.md` → "Game start gesture not unified".

### B14. line 103 — top-bar inventory

See the **Top-bar inventory** table in `mode-matrix.md`. Classic standard
(`displayCWspeed()`/`updateTopLine()`, `m32_v6.ino:2407-2463`): keyer-mode/`d`
symbol @col 2, `SK`/`CK` @3, effective/ rx WpM @3–6, **WpM @7–8 (bold when in
speed mode)**, `"WpM"` @10–12, volume bar at right, LoRa/WiFi logo when
transmitting. Games replace the bar entirely with bespoke TFT HUDs (allowed),
but none reuse the standard WpM slot/format.

### B15. line 113 — echo / CW-source visual distinction

Standard mechanism = font attribute: incoming/machine CW is `REGULAR`
(`FONT_INCOMING`), user-outgoing is `BOLD` (`FONT_OUTGOING`, `MorseOutput.h:33-34`),
results `"OK"`/`"ERR"` printed `BOLD` (`echoTrainerEval()` `m32_v6.ino:2485,2499`).
This is **not applied uniformly**: Transceiver modes and QSO Bot use the same
scroll attributes, but the games render their own colored text and the
distinction is **not standardized** across Echo/Trx/Bot/games. → `divergences.md`.

### B16. line 117 — prosign rendering

`cleanUpProSigns()` (`m32_v6.ino:2561`) expands single-char codes to display
forms: `S→<as>`, `A→<ka>`, `N→<kn>`, `K→<sk>`, `E→<ve>`, `B→<bk>`, `H→ch`,
`R→<err>`, `U→*`. So the canonical rendering is lowercase angle-bracket
(`<as> <ka> <kn> <sk> <ve> <bk> <err>`). The encode side is `encodeProSigns()`
(`m32_v6.ino:3035`). **Note:** CLAUDE.md §3.4 names `buildCwStream` as the
prosign-preserving function — **no such function exists**; the real path is
`encodeProSigns()` → `generateCWword()` (`m32_v6.ino:1868`) → `cleanUpProSigns()`.
→ logged in `divergences.md` as a doc-accuracy fix.

### B17. line 127 — which modes filter by Koch level

Koch filtering is gated by the `kochActive` flag (`m32_v6.ino:217`), set **true**
only when entering a **Koch Trainer** sub-mode (`MorseMenu.cpp:569-614`) and used
in `getRandomWord/Abbrev/Chars` (`m32_v6.ino:1625,1640,1673`) and `fetchNewWord()`
(`OPT_KOCH`/`OPT_KOCH_ADAPTIVE`, `m32_v6.ino:2267,2271`). Plain **CW Generator**
and **Echo Trainer** do **not** filter by Koch level — they honor `posRandomOption`
/ content type instead (by design: they are the "free" trainers). Games:
Morse Invaders and Morsel are **Koch-aware** (lesson is part of their level
model, `MorseGame.cpp:166-171`); Radio Cave / Pileup are content-driven, not
Koch-filtered.

### B18. line 144 — uniform score reset

**None exists.** Radio Cave has an in-game `NEW` command (key `Y` to confirm)
that wipes its `save` blob (`MorseRadioCave.cpp:1306-1314`). Morse Invaders,
Morsel and Pileup expose **no** score-reset UI. `resetDefaults()`
(`MorsePreferences.cpp:1530`) clears only the `morserino` namespace — so it would
wipe Morsel's `hi/hv` and player identity but **not** Invaders (`m32game`) or
Radio Cave (`radiocave`). → `divergences.md`.

### B19. line 151 — timeout counts as a miss (uniform?)

Per-round timeouts are game-local and **not uniform**. Fight the Pileup has an
explicit per-attempt timeout shown as "Timeout %ds" (`MorsePileup.cpp:889`) that
counts against the player; Morse Invaders' miss model is invader-drop, not a
keying timeout; Morsel/Radio Cave have no per-round keying timeout. The **global
TOT** (inactivity shutdown) is separate and applies everywhere
(`checkShutDown()` `m32_v6.ino:2745`). No single "timeout = miss" rule is coded.
→ mark **INTENTIONAL?** in `divergences.md`.

### B20. line 157 — multiplayer pairing/start flow

Two unrelated patterns:
- **Classic Transceiver** (LoRa / WiFi): peer is configured in preferences
  (`wlanTRXPeer`), session starts on mode entry; WiFi resolves the peer or
  broadcasts (`MorseMenu.cpp:765-789`, `setupWifi()`), LoRa just `startReceive()`.
  No interactive pairing screen.
- **Game multiplayer** (Morsel, Fight the Pileup over ESP-NOW): an explicit
  **lobby** with "Start as Server" / join, roster, and "waiting for server"
  states (`MorseMorsel.cpp:1529-1545`, `MorsePileup.cpp` lobby). ESP-NOW is set up
  via `setupESPNow()` (`MorseMenu.cpp:805`) with a boot-time `wifiWarmup()`.
These do not share a pairing abstraction. → `divergences.md` (design decision
for Willi: should games' lobby concepts and Trx pairing converge?).

### B21. line 166 — settings reached/edited the same way; do games conform?

Classic + QSO Bot: the **`setupPreferences()` / `adjustKeyerPreference()`**
mechanism (`MorsePreferences.cpp`), entered by black double-click, editing
`pliste[]` items. Games **do not** use it for game-specific settings — those are
adjusted in bespoke lobby UIs (e.g. Invaders' FN-cycle over WPM/Vol/Level
`MorseGame.cpp:712`; Morsel "vol=len"). Some game settings *do* live in
`pliste[]` (`posInvaderOrient`, `posTheme`, `posQsoBotContestType`,
`morsedefs.h:402-410`) and are editable via the normal preferences menu;
others are lobby-only and not in preferences at all. → `divergences.md`.

---

## Cross-cutting: player identity (extra-scope item #2)

`playerCall` / `playerName` live in the **`morserino`** NVS namespace but are
**not** `prefPos` items and **cannot be set from the preferences menu**. They are
written only by (a) Fight the Pileup's in-game `enterString` prompts
(`MorsePileup.cpp:325-326,851`) and (b) the serial M32 protocol
(`m32_v6.ino:3852,3861`). They are read by Pileup, Morsel, QSO Bot (call only)
and `MorseJSON`. Recommendation and effort estimate in `divergences.md`.
