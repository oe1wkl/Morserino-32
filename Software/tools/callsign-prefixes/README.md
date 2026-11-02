# Call sign prefix table

`build_prefix_table.py` writes `Software/src/Version 6 and newer/callsign_prefixes.h`, the table behind every
generated call sign (CW Generator and Echo Trainer **Call Signs**, the QSO Bot, Fight the Pileup, Memory Chain)
and behind **Prefixes** (OE3, KD9, UA9, 3B8 …).

```sh
python3 build_prefix_table.py MASTER.SCP cty.dat     # writes callsign_prefixes.h in place
```

## Inputs

| File | Where from | Committed? |
|---|---|---|
| `MASTER.SCP` | Super Check Partial, <https://www.supercheckpartial.com/> — calls heard in contests | no — fetch a current one |
| `cty.dat` | AD1C country file, <https://www.country-files.com/cty/cty.dat> | no — fetch a current one |
| `base_prefixes.txt` | this folder: the ITU-block prefixes of the original (2026-03) table | yes |

The header records which releases it was built from (`MASTER.SCP release …, cty.dat VER…`).

## What the table holds

One entry per base prefix and continent: base prefix, continent, CQ zone, weight, a "common" flag, and ten
call-area digit weights packed into five bytes (14 bytes per entry). The firmware picks an entry by weight
(`buildRandomPrefix()` in `m32_v6.ino`), then the digit by the digit weights; an entry whose digit weights are all
zero is a whole prefix on its own (3B8, KH6, I1 — the calls are 3B8AB, KH6ABC, I1ABC).

- **Weight**: `255 · ln(n) / ln(n_max)`, n = MASTER.SCP calls under the `base_prefixes.txt` entry — the formula
  of the original table, so the mix of countries stays as it was. A block the original table lacked (KD, WA, IK,
  UA, R, TI …) is split off its old entry (KD from K, IK from I) and takes its share of that entry's weight.
- **Common** ("Call Prefixes: Common only"): the old entry's weight is at least 81, the firmware's original
  threshold.
- **Digits**: from MASTER.SCP when at least 20 calls back them (OE3 often, OE0 never), otherwise every digit
  cty.dat places on that continent. cty.dat also splits entries by continent where the call area decides it
  (R9/R0/UA9 = Asiatic Russia, EA8 = Canary Islands), so **Calls Region** gets them right.
- Left out: calls with `/`, special-event forms with more than one digit after the base (R59, YR95), and base
  prefixes cty.dat does not know (unissued blocks such as Y2–Y9 or 2B–2Z). Antarctica is kept apart as `CONT_AN`
  (cty.dat files it under SA).

The VK/ZL region does not use the table: it has its own hand-made area weights in `buildRandomPrefix()`.
