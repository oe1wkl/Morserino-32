# Word list licence check (R10) — 2026-11-08

*Facts gathered for Willi's decision. Not legal advice.*

## What we ship

| Where | What | Since |
|---|---|---|
| Firmware `english_words.h`, `#ifdef CONFIG_ENGLISH_OXFORD` | 4,881 words with frequency weights. Header: "words are most of the oxford 5000 / weights from hermitdave/FrequencyWords". Enabled in `M32_Pocket_build_flags` (every Pocket env) and in `heltec_wifi_lora_32_V2` | PR #118 (Harald Klein, 2025-02-26), V7; Koch words from it since `f2de99e` (I4, 2026-10-04) |
| Trainer `m32_trainer_corpus.json` (`words`) | The same 4,881 words, in frequency order; the weights themselves are not copied | M3b (`4b2aaa1`), not yet published |

The repository is GPL v3 (`Software/LICENSE`); `english_words.h` has no licence note of its own.

## The two sources

**hermitdave/FrequencyWords** — the README states "MIT License for code" and "CC-by-sa-4.0 for content"; the
lists are counted from the OpenSubtitles 2016/2018 corpora. CC BY-SA 4.0 is one-way compatible with GPL v3
(Creative Commons declared this in 2015), so using it in GPL firmware is fine. What is owed: **attribution**
(author, licence, link, that it was changed) and **ShareAlike** for whatever adapts it (the weights in
`english_words.h`; arguably the frequency order in the corpus). Easy to put right.

**Oxford 5000 (Oxford University Press)** — no open licence found. The about page and the list page
(oxfordlearnersdictionaries.com/about/wordlists/oxford3000-5000, …/wordlists/oxford3000-5000) offer the list
for browsing and as free PDF downloads for learners and teachers, but grant no reuse or redistribution
right. The site's Terms and Conditions (25 April 2018) are about paid products and say OUP and its licensors
own all intellectual property rights; nothing in them permits copying the content. OUP licenses the word
lists commercially (an OUP press release: the Oxford 2000 licensed to a South Korean app developer), so OUP treats
them as protected. The PDFs could not be fetched (403) to read their copyright line. A list of common words is
thin as a copyright work, but the *selection* of 5,000 was made by OUP with a corpus and an expert panel, and
in the EU (Austria) a substantial extraction from a database can also fall under the sui generis database
right. Shipping "most of" the list is a substantial part.

## Options

1. **Keep it, add attribution.** Least work. Attribution does not replace permission for the Oxford part;
   the exposure is the same as today's firmware (public on GitHub since V7).
2. **Ask OUP for permission** (ELT rights/permissions) for firmware and trainer. Slow; answer uncertain.
3. **Replace the Oxford selection with an open list**, for firmware and trainer together, e.g.
   - **NGSL 1.2** (New General Service List, Browne/Culligan/Phillips, CC BY-SA 4.0 per its site): ~2,800
     learner-core headwords, curated for learners like the Oxford 3000; fewer words than now.
   - **FrequencyWords top N**, filtered (names, profanity, contractions, subtitle noise): any size, but
     needs a filter list and a review, since it is spoken film English.

   Either way the FrequencyWords attribution is still owed if its weights stay. User-visible: the words the
   M32 and the trainer send change; Word Length / Koch words need a re-check. The manuals (V10, EN + DE) do
   not mention Oxford, so they need no change for the source itself.

## Decision (Willi, 2026-11-08)

**Keep the list; credit FrequencyWords in full; no Oxford credit.** Willi's reasoning: the list was most
likely made by filtering FrequencyWords with the Oxford 5000; the frequency list is the valuable part, the
Oxford 5000 only set the size and filtered out names, profanities and the like; OUP offers it for learning
and teaching, which covers our purpose. The manuals speak of "the 5,000 most common English words" and do not
mention Oxford.

Done (2026-11-08): CC BY-SA 4.0 credit for FrequencyWords (author, link, licence, "filtered and shortened",
the adapted list likewise CC BY-SA 4.0) in
- firmware `english_words.h` (master; replaces the "oxford 5000" comment);
- manuals V10 EN + DE, under *English Words* (master);
- trainer corpus `wordsSource` (`build_corpus.py`) and a *Sources* paragraph in the page's help, which also
  credits OZ1THC and RealQSOs (G0ELZ, G3DVW); page 2026-11-08.

The build flag keeps its name `CONFIG_ENGLISH_OXFORD` (renaming it is churn across 10 envs).
