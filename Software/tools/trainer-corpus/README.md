# M32 Trainer corpus

The texts the M32 Trainer (`Software/Utilities/m32_trainer.html`) practises besides random groups:

| Tier | What | Source |
|---|---|---|
| A | single words and word chains | `english_words.h` (the Oxford-based list, 4,881 plain words, most frequent first) |
| B | plain-language phrases, 2–8 words | `tier_b_en.txt` (authored), and Tommy OZ1THC's `Practice Files/phrases_2/3/4_words.txt` (contributed 2019; apostrophe-less spellings written out, `tier_b_exclude.txt` leaves some out), and `tier_b_willi_en.txt` (a list Willi generated with Google Gemini, kept as received: `!` dropped, apostrophes written out) |
| C | QSO fragments, 2–8 words | `Practice Files/RealQSOs.TXT` by Bill G0ELZ and Gordon G3DVW (SK), used with Bill's permission: each QSO cut at `=`, prosigns normalised, cut-off pieces dropped, `realqsos_exclude.txt` for the rest |
| C | QSO lines with slots, 2–8 words | `tier_c_en.txt` (authored, seeded from the QSO Bot); slot values from `qso_content.h` and the `@pool` lines |

```
python3 build_corpus.py          # writes Software/Utilities/m32_trainer_corpus.json and devdocs/training-addon/CORPUS_REVIEW.md
python3 build_corpus.py --check  # validates only
```

The script refuses to write anything if a line has a character the M32 doesn't teach, has fewer than 2 or more
than 8 words, uses an unknown slot, is a duplicate, or could fill to more than `cw/play` takes.
`CORPUS_REVIEW.md` lists every item by the lesson it becomes usable at, with a coverage table for the four
Koch sequences in the firmware (M32, LCWO, CW Academy, LICW).

Item ids are a hash of the text, so an edited line becomes a new item. The engine keeps no per-item state
in v1, so that costs nothing.

Correct CW idiom is a product requirement: Willi reviews tiers B and C, and a second experienced operator
reviews tier C before release (decision 12c in `devdocs/training-addon/LEAN_V1_SCOPE.md`).
