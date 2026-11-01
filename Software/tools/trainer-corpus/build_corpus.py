#!/usr/bin/env python3
"""Builds the M32 Trainer's corpus (Software/Utilities/m32_trainer_corpus.json) and its review
document (devdocs/training-addon/CORPUS_REVIEW.md) from:

  tier_b_en.txt     plain-language phrases, one per line (authored, Willi reviews)
  tier_c_en.txt     QSO lines with [SLOT]s, and @pool lines for the slots the firmware has no pool for
  english_words.h   the Oxford-based word list (tier A: words and word chains), frequency order
  qso_content.h     the QSO Bot's pools: names, QTHs, rigs, antennas, weather, ages, SOTA/POTA refs
  MorsePreferences.h  the four Koch sequences, for the coverage table

Run it after editing either text file:   python3 build_corpus.py
It stops with a list of problems (unknown character, wrong length, unknown slot, duplicate) and
writes nothing then. `--check` validates only.
"""
import hashlib, json, pathlib, re, sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
FW = ROOT / 'Software' / 'src' / 'Version 6 and newer'
OUT_JSON = ROOT / 'Software' / 'Utilities' / 'm32_trainer_corpus.json'
OUT_REVIEW = ROOT / 'devdocs' / 'training-addon' / 'CORPUS_REVIEW.md'

SCHEMA = 1
MIN_WORDS, MAX_WORDS = 2, 8
CW_PLAY_MAX = 200            # bytes; cw/play's buffer index is a uint8_t (255), leave room

# Prosigns as the trainer writes them -> the one-character codes of the firmware's Koch strings
PROSIGN_CODE = {'<sk>': 'K', '<as>': 'S', '<kn>': 'N', '<ka>': 'A', '<ve>': 'E', '<bk>': 'B'}
PLAIN = set('abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+')

def die(problems):
    print('build_corpus.py: %d problem(s), nothing written:' % len(problems))
    for p in problems:
        print('  ' + p)
    sys.exit(1)

def tokens(text):
    """'cq de <sk>' -> ['c','q',' ','d','e',' ','<sk>'] (the engine's tokenize(), for corpus text)."""
    out, i = [], 0
    while i < len(text):
        if text[i] == ' ':
            out.append(' '); i += 1; continue
        m = re.match(r'<[a-z]{2,3}>', text[i:])
        if m:
            out.append(m.group(0)); i += len(m.group(0)); continue
        out.append(text[i]); i += 1
    return out

def codes(text):
    """The text's characters as Koch-string codes (spaces dropped)."""
    return {PROSIGN_CODE.get(t, t) for t in tokens(text) if t != ' '}

def item_id(prefix, text):
    return prefix + hashlib.sha1(text.encode()).hexdigest()[:8]

# ---- firmware sources ----------------------------------------------------------------

def read_words():
    t = (FW / 'english_words.h').read_text(encoding='utf-8')
    t = t[t.index('#ifdef  CONFIG_ENGLISH_OXFORD'):]
    pairs = re.findall(r'\{ "([^"]+)", (\d+) \}', t)
    seen, out = set(), []
    for w, n in sorted(pairs, key=lambda p: -int(p[1])):
        if re.fullmatch('[a-z]+', w) and w not in seen:     # the 19 hyphenated words are left out
            seen.add(w); out.append(w)
    return out

def read_pools():
    t = (FW / 'qso_content.h').read_text(encoding='utf-8')
    masks = {k: int(v, 16) for k, v in re.findall(r'#define (CONT_[A-Z]{2})\s+(0x[0-9A-Fa-f]+)', t)}
    def cont_array(name):
        body = re.search(r'kNAME\[\] = \{(.*?)\n\};'.replace('NAME', name), t, re.S).group(1)
        out = []
        for v, m in re.findall(r'\{ *"([^"]+)", *([A-Z_|]+) *\}', body):
            out.append([v.lower(), sum(masks[x] for x in m.split('|'))])
        return out
    def plain_array(name):
        body = re.search(r'kNAME\[\] = \{(.*?)\n\};'.replace('NAME', name), t, re.S).group(1)
        return [v.lower() for v in re.findall(r'"([^"]+)"', body)]
    return {
        'NAME': cont_array('Names'), 'QTH': cont_array('Qths'),
        'SOTA': cont_array('SotaRefs'), 'POTA': cont_array('PotaRefs'),
        'RIG': plain_array('Rigs'), 'ANT': plain_array('Ants'), 'WX': plain_array('Wxs'), 'AGE': plain_array('Ages'),
    }, {k.replace('CONT_', '').lower(): v for k, v in masks.items()}

def read_prefixes():
    """callsign_prefixes.h -> [[prefix (lower case), continent mask, CQ zone, weight]], in table order
    (the generator's weighted pick walks the table in this order)."""
    t = (FW / 'callsign_prefixes.h').read_text(encoding='utf-8')
    rows = re.findall(r'\{"([A-Z0-9/_]+)", (0x[0-9A-Fa-f]+), +(\d+), +(\d+)\}', t)
    n = int(re.search(r'#define PREFIX_COUNT (\d+)', t).group(1))
    if len(rows) != n:
        sys.exit('build_corpus.py: read %d prefixes, callsign_prefixes.h says %d' % (len(rows), n))
    return [[p.lower(), int(c, 16), int(z), int(w)] for p, c, z, w in rows]

COMMON_WEIGHT = 81           # getRandomCall()'s "Common" filter: minWeight = 81

def call_possible(active, prefixes):
    """Can the generator (common prefixes, any continent) make a call from these characters only?"""
    digits = [c for c in active if c.isdigit()]
    letters = [c for c in active if 'a' <= c <= 'z']
    if not letters:
        return False
    for p, cont, zone, w in prefixes:
        if w < COMMON_WEIGHT or not set(p) <= active:
            continue
        needs_digit = not p[-1].isdigit() or len(p) <= 2
        if not needs_digit or digits:
            return True
    return False

def read_sequences():
    t = (FW / 'MorsePreferences.h').read_text(encoding='utf-8')
    names = [('M32', 'morserinoKochChars'), ('LCWO', 'lcwoKochChars'), ('CW Academy', 'cwacKochChars'), ('LICW', 'licwAllKochChars')]
    return [(label, re.search(var + r'\s*=\s*"([^"]+)"', t).group(1)) for label, var in names]

# ---- authored sources ------------------------------------------------------------------

def read_lines(path):
    """-> [(line number, tag, text)]. '## tag' sets the tag for the lines below it; '#' comments."""
    out, tag = [], ''
    for n, raw in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = raw.strip()
        if line.startswith('##'):
            tag = line[2:].strip(); continue
        if not line or line.startswith('#'):
            continue
        out.append((n, tag, line))
    return out

# ---- the repo's practice files (Practice Files/, already published for the File Player) ---------

PRACTICE = ROOT / 'Practice Files'

def read_exclude(name):
    p = HERE / name
    return {l.strip() for l in p.read_text(encoding='utf-8').splitlines()
            if l.strip() and not l.startswith('#')} if p.exists() else set()

# Tommy's files lost their apostrophes (the M32 has none to send): spell the words out
CONTRACTIONS = {
    'im': 'i am', 'ive': 'i have', 'ill': 'i will', 'its': 'it is', 'youre': 'you are', 'dont': 'do not',
    'didnt': 'did not', 'cant': 'cannot', 'wont': 'will not', 'thats': 'that is', 'lets': 'let us',
    'todays': 'today is', 'wheres': 'where is', 'writings': 'writing is',
}

def tidy_question(text):
    return re.sub(r' \?', '?', text)                    # "how are you ?" -> "how are you?"

def read_tommy(exclude):
    """phrases_2/3/4_words.txt, contributed by Tommy OZ1THC (2019) -> [(file:line, text)], dropped count."""
    out, dropped = [], []
    for f in ('phrases_2_words.txt', 'phrases_3_words.txt', 'phrases_4_words.txt'):
        for n, raw in enumerate((PRACTICE / f).read_text(encoding='latin-1').splitlines(), 1):
            t = re.sub(r'[ =]+$', '', raw.strip()).lower()
            t = re.sub(r'\s+', ' ', t).strip()
            if not t:
                continue
            t = tidy_question(' '.join(CONTRACTIONS.get(w, w) for w in t.split(' ')))
            if t in exclude:
                dropped.append(t); continue
            out.append(('%s:%d' % (f, n), t))
    return out, dropped

# The apostrophes of Willi's list, written out (the M32 has no apostrophe to send). An apostrophe
# not listed here stops the build, rather than slipping through.
APOSTROPHES = {"couldn't": 'could not', "how's": 'how is', "i'm": 'i am', "what's": 'what is'}

def read_willi():
    """tier_b_willi_en.txt, a list Willi generated with Google Gemini (2026-11-01), kept as received: '* Phrase' per line.
    -> [(file:line, text)]: lower-cased, '!' dropped, apostrophes written out."""
    out = []
    for n, raw in enumerate((HERE / 'tier_b_willi_en.txt').read_text(encoding='utf-8').splitlines(), 1):
        t = re.sub(r'^\*\s*', '', raw.strip()).lower().replace('!', '')
        t = re.sub(r'\s+', ' ', t).strip()
        if t:
            out.append(('tier_b_willi_en.txt:%d' % n, ' '.join(APOSTROPHES.get(w, w) for w in t.split(' '))))
    return out

# A fragment that ends on one of these was cut off before its value ("name is pat qth")
DANGLING = set('qth name is es rig ant wx op de and the a to my ur hr nr of in with wid fer for at on pwr tcvr '
               'temp rst rprt call sig sigs from by via is are was hre here'.split())

def read_realqsos(exclude):
    """RealQSOs.TXT (G0ELZ and G3DVW, used with Bill G0ELZ's permission) cut at '=' into 2-8-word
    fragments. -> [fragment], {reason: count}."""
    text = (PRACTICE / 'RealQSOs.TXT').read_text(encoding='latin-1').replace('\r', '')
    kept, why = [], {}
    def drop(reason):
        why[reason] = why.get(reason, 0) + 1
    for qso in text.split('\n'):
        for frag in qso.split('='):
            t = re.sub(r'\s+', ' ', frag.lower()).strip()
            if not t:
                continue
            t = t.replace('<ar>>kn', '<ar><kn>').replace('><', '> <')
            t = re.sub(r'<ar>sk\b', '<ar> <sk>', t)
            t = t.replace('<ar>', '+').replace('<bt>', '=')
            t = tidy_question(t)
            ws = t.split(' ')
            if not MIN_WORDS <= len(ws) <= MAX_WORDS:
                drop('not 2-8 words'); continue
            if any(tk not in PLAIN and tk not in PROSIGN_CODE for tk in tokens(t) if tk != ' '):
                drop('character the M32 does not teach'); continue
            if ws[-1] in DANGLING:
                drop('cut off (ends on "%s")' % 'qth/name/is/...'); continue
            if any(len(w) > 14 for w in ws):
                drop('a word over 14 characters'); continue
            if t in exclude:
                drop('in realqsos_exclude.txt'); continue
            kept.append(t)
    return kept, why

def check_text(where, text, problems, slots=None):
    fixed = re.sub(r'\[[A-Z0-9]+\]', ' ', text) if slots is not None else text
    if fixed != fixed.lower():
        problems.append('%s: write it in lower case (prosigns as <sk>): %r' % (where, text))
    if '  ' in text or text != text.strip():
        problems.append('%s: stray spaces: %r' % (where, text))
    for t in tokens(fixed):
        if t == ' ' or t in PLAIN or t in PROSIGN_CODE:
            continue
        problems.append('%s: %r is not a character the M32 teaches: %r' % (where, t, text))
    n = len(text.split(' '))
    if not MIN_WORDS <= n <= MAX_WORDS:
        problems.append('%s: %d words, must be %d-%d: %r' % (where, n, MIN_WORDS, MAX_WORDS, text))
    if slots is not None:
        for s in re.findall(r'\[([A-Z0-9]+)\]', text):
            if s not in slots:
                problems.append('%s: unknown slot [%s]: %r' % (where, s, text))

def main():
    check_only = '--check' in sys.argv
    problems = []
    words = read_words()
    pools, continents = read_pools()
    seqs = read_sequences()

    # tier C: @pool lines first (pools the firmware has none for), then the templates
    extra = {}
    for n, tag, line in read_lines(HERE / 'tier_c_en.txt'):
        if line.startswith('@pool '):
            parts = line.split()
            extra[parts[1]] = parts[2:]
    all_pools = dict(pools); all_pools.update(extra)
    slots = set(all_pools) | {'CALL', 'CALL2'}           # calls come from the call sign generator

    phrases, seen = [], {}
    for n, tag, line in read_lines(HERE / 'tier_b_en.txt'):
        where = 'tier_b_en.txt:%d' % n
        check_text(where, line, problems)
        if line in seen: problems.append('%s: duplicate of line %d' % (where, seen[line]))
        seen[line] = n
        phrases.append({'id': item_id('b', line), 'tier': 'B', 'src': 'authored', 'text': line, 'tags': []})   # B's ## lines are writing bands only
    tommy, tommy_dropped = read_tommy(read_exclude('tier_b_exclude.txt'))
    tommy_dupes = 0
    for where, line in tommy:
        check_text(where, line, problems)
        if line in seen:
            tommy_dupes += 1; continue
        seen[line] = where
        phrases.append({'id': item_id('b', line), 'tier': 'B', 'src': 'oz1thc', 'text': line, 'tags': []})
    willi = read_willi()
    willi_dupes = []
    for where, line in willi:
        check_text(where, line, problems)
        if line in seen:
            willi_dupes.append(line); continue
        seen[line] = where
        phrases.append({'id': item_id('b', line), 'tier': 'B', 'src': 'willi', 'text': line, 'tags': []})
    templates = []
    for n, tag, line in read_lines(HERE / 'tier_c_en.txt'):
        if line.startswith('@pool '):
            continue
        where = 'tier_c_en.txt:%d' % n
        check_text(where, line, problems, slots)
        if line in seen: problems.append('%s: duplicate' % where)
        seen[line] = n
        templates.append({'id': item_id('c', line), 'tier': 'C', 'src': 'authored', 'text': line, 'tags': [tag] if tag else []})
    real, real_why = read_realqsos(read_exclude('realqsos_exclude.txt'))
    real_kept = 0
    for line in real:
        if line in seen:
            real_why['duplicate'] = real_why.get('duplicate', 0) + 1; continue
        seen[line] = 'RealQSOs'
        real_kept += 1
        phrases.append({'id': item_id('c', line), 'tier': 'C', 'src': 'realqsos', 'text': line, 'tags': []})
    for name, vals in all_pools.items():
        for v in vals:
            v = v[0] if isinstance(v, list) else v
            for t in tokens(v):
                if t not in PLAIN and t not in PROSIGN_CODE:
                    problems.append('pool %s: %r has %r' % (name, v, t))
    # longest expansion must fit cw/play
    longest = max((len(x) for p in all_pools.values() for x in [v[0] if isinstance(v, list) else v for v in p]), default=0)
    for t in templates:
        worst = len(re.sub(r'\[[A-Z0-9]+\]', 'x' * max(longest, 12), t['text']))
        if worst > CW_PLAY_MAX:
            problems.append('%s: up to %d bytes filled, over %d' % (t['text'], worst, CW_PLAY_MAX))
    if problems:
        die(problems)

    corpus = {
        'schema': SCHEMA, 'lang': 'en',
        'about': 'M32 Trainer corpus, built by Software/tools/trainer-corpus/build_corpus.py -- do not edit; '
                 'edit tier_b_en.txt / tier_c_en.txt and rebuild.',
        'words': words,
        'wordsSource': 'english_words.h (most of the Oxford 5000; frequency order from hermitdave/FrequencyWords)',
        'sources': {
            'authored': 'written for the M32 Trainer (Software/tools/trainer-corpus/)',
            'oz1thc': 'phrases contributed to the Morserino-32 project by Tommy, OZ1THC',
            'willi': 'a phrase list generated with Google Gemini, supplied by Willi, OE1WKL',
            'realqsos': 'from RealQSOs.TXT by Bill, G0ELZ, and Gordon, G3DVW (SK), used with Bill\'s permission',
        },
        'continents': continents,
        'pools': all_pools,
        'prefixes': read_prefixes(),
        'prefixesSource': 'callsign_prefixes.h (prefix, continent mask, CQ zone, weight), for the trainer\'s copy of getRandomCall()',
        'phrases': phrases,
        'templates': templates,
    }
    stats = {'tommy': len(tommy), 'tommyDropped': tommy_dropped, 'tommyDupes': tommy_dupes, 'williDupes': willi_dupes,
             'realKept': real_kept, 'realWhy': real_why}
    review = review_doc(corpus, seqs, stats)
    if check_only:
        print('ok: %d words, %d phrases, %d tier C templates' % (len(words), len(phrases), len(templates)))
        return
    OUT_JSON.write_text(json.dumps(corpus, ensure_ascii=False, separators=(',', ':')) + '\n', encoding='utf-8')
    OUT_REVIEW.write_text(review, encoding='utf-8')
    print('%s: %d words, %d phrases, %d tier C templates, %d KB' %
          (OUT_JSON.name, len(words), len(phrases), len(templates), OUT_JSON.stat().st_size // 1024))
    print(OUT_REVIEW.name + ' written')

# ---- coverage and the review document ------------------------------------------------------

def first_lesson(text, seq):
    """The lesson (1-based, in sequence `seq`) from which every character of `text` is taught; None if never."""
    need = codes(text)
    if not need <= set(seq):
        return None
    return max(seq.index(c) for c in need) + 1 if need else 1

def slot_ok(slot, active, pools, prefixes):
    if slot in ('CALL', 'CALL2'):
        return call_possible(active, prefixes)
    vals = [v[0] if isinstance(v, list) else v for v in pools[slot]]
    return any(codes(v) <= active for v in vals)

def template_ok(text, active, pools, prefixes):
    fixed = re.sub(r'\[[A-Z0-9]+\]', ' ', text)
    return codes(fixed) <= active and all(slot_ok(s, active, pools, prefixes) for s in re.findall(r'\[([A-Z0-9]+)\]', text))

def review_doc(corpus, seqs, stats):
    words, phrases, templates, pools = corpus['words'], corpus['phrases'], corpus['templates'], corpus['pools']
    tierB = [p for p in phrases if p['tier'] == 'B']
    ours = [p for p in tierB if p['src'] == 'authored']
    tommy = [p for p in tierB if p['src'] == 'oz1thc']
    willi = [p for p in tierB if p['src'] == 'willi']
    real = [p for p in phrases if p['src'] == 'realqsos']
    m32 = seqs[0][1]
    L = []
    L.append('# M32 Trainer corpus — draft for review\n')
    L.append('*Generated by `Software/tools/trainer-corpus/build_corpus.py` (edit the sources, not this file). '
             'For Willi\'s review (decision 12c); tier C also gets a second operator\'s look.*\n')
    L.append('**What to look for:** correct CW idiom and abbreviations, prosign use, natural plain language, nothing '
             'odd or offensive, and whether the lesson bands make sense.\n')
    L.append('| Tier | Source | Items |')
    L.append('|---|---|---|')
    L.append('| A — words, word chains | `english_words.h` (Oxford-based list) | %d words |' % len(words))
    L.append('| B — plain language | written for the trainer (`tier_b_en.txt`) | %d |' % len(ours))
    L.append('| B — plain language | Tommy OZ1THC\'s `phrases_2/3/4_words.txt` | %d |' % len(tommy))
    L.append('| B — plain language | Willi\'s list (`tier_b_willi_en.txt`) | %d |' % len(willi))
    L.append('| C — QSO | fragments of `RealQSOs.TXT` (G0ELZ, G3DVW) | %d |' % len(real))
    L.append('| C — QSO | templates with slots (`tier_c_en.txt`) | %d |' % len(templates))
    L.append('')
    L.append('## Coverage — usable items per lesson\n')
    L.append('Counted with the strict Koch filter (decision 13a): every character of the item is in the first *n* '
             'characters of the sequence. *Words* = tier A (single words; word chains are drawn from the same words). '
             'A template counts when its fixed text and at least one value for each slot are usable (a call sign: when the '
             'generator can make one from a common prefix with the characters learned so far). A tier is offered at a lesson '
             'once it has 10 usable items (decision 13b).\n')
    marks = [5, 10, 12, 14, 16, 18, 20, 22, 25, 28, 30, 32, 35, 38, 40, 42, 45, 51]
    for label, seq in seqs:
        L.append('### %s sequence `%s`\n' % (label, seq))
        L.append('| Lesson | ' + ' | '.join(str(m) for m in marks) + ' |')
        L.append('|---|' + '---|' * len(marks))
        rows = [('Words (A)', lambda a: sum(codes(w) <= a for w in words)),
                ('Plain (B)', lambda a: sum(codes(p['text']) <= a for p in tierB)),
                ('QSO, real (C)', lambda a: sum(codes(p['text']) <= a for p in real)),
                ('QSO, templates (C)', lambda a: sum(template_ok(t['text'], a, pools, corpus['prefixes']) for t in templates))]
        for name, f in rows:
            L.append('| %s | %s |' % (name, ' | '.join(str(f(set(seq[:m]))) for m in marks)))
        L.append('')

    def by_lesson(items, title):
        by = {}
        for p in items:
            by.setdefault(first_lesson(p['text'], m32), []).append(p)
        L.append(title)
        for les in sorted(by, key=lambda x: (x is None, x or 0)):
            L.append('**From lesson %s** (%s)\n' % (les if les else '— (never in the M32 sequence)', m32[les - 1] if les else ''))
            for p in sorted(by[les], key=lambda p: (len(p['text'].split()), p['text'])):
                L.append('- `%s`' % p['text'])
            L.append('')

    L.append('## Tier B — plain-language phrases\n')
    by_lesson(ours, '### Written for the trainer (%d)\n\nGrouped by the first M32-sequence lesson that can sound them.\n' % len(ours))
    L.append('### Tommy OZ1THC\'s phrases (%d)\n' % len(tommy))
    L.append('From `Practice Files/phrases_2/3/4_words.txt` (contributed in 2019). Lower-cased, the trailing `=` removed, '
             'the apostrophe-less spellings written out (`im` → `i am`, `dont` → `do not`, ... — the M32 has no apostrophe '
             'to send), `?` joined to its word. %d were already in the list above; **%d are left out** by '
             '`tier_b_exclude.txt` — my suggestion, see there; delete a line to let a phrase back in:\n'
             % (stats['tommyDupes'], len(stats['tommyDropped'])))
    for t in stats['tommyDropped']:
        L.append('- ~~`%s`~~' % t)
    L.append('')
    by_lesson(tommy, '')

    L.append('### Willi\'s list (%d)\n' % len(willi))
    L.append('From `tier_b_willi_en.txt`, generated with Google Gemini (kept as received). Lower-cased, `!` dropped, apostrophes written out '
             '(`couldn\'t` → `could not`, `i\'m` → `i am`, `how\'s`/`what\'s` → `how is`/`what is`). %d were already '
             'in a list above: %s.\n' % (len(stats['williDupes']), ', '.join('`%s`' % t for t in stats['williDupes']) or '—'))
    by_lesson(willi, '')

    L.append('## Tier C — QSO lines\n')
    L.append('### Fragments of RealQSOs.TXT (%d)\n' % len(real))
    L.append('Each QSO cut at `=` (BT); kept when 2–8 words. `<AR>` became `+`, run-together prosigns were split '
             '(`<AR><KN>` → `+ <kn>`, `<AR>SK` → `+ <sk>`), `?` joined to its word. Dropped:\n')
    for k, v in sorted(stats['realWhy'].items(), key=lambda kv: -kv[1]):
        L.append('- %s: %d' % (k, v))
    L.append('\nToo many to read one by one: **here is a fixed random sample of 100** (the same each build). '
             'A bad fragment goes into `realqsos_exclude.txt`, exactly as shown.\n')
    import random
    rnd = random.Random(1955)
    sample = rnd.sample(sorted(p['text'] for p in real), min(100, len(real)))
    for t in sorted(sample, key=lambda t: (first_lesson(t, m32) or 99, t)):
        L.append('- `%s` — from lesson %s' % (t, first_lesson(t, m32) or '—'))
    L.append('')
    L.append('### Templates with slots (%d)\n' % len(templates))
    L.append('`[SLOT]`s are filled at practice time (one value per slot, picked to suit the call sign\'s continent '
             'where the pool has one). The slots and their values are listed after the lines.\n')
    tags = {}
    for t in templates:
        tags.setdefault(t['tags'][0] if t['tags'] else '', []).append(t)
    for tag, items in tags.items():
        L.append('**%s**\n' % (tag or 'untagged'))
        for t in items:
            fl = first_lesson(re.sub(r'\[[A-Z0-9]+\]', ' ', t['text']), m32)
            L.append('- `%s`  — fixed text from lesson %s' % (t['text'], fl if fl else '—'))
        L.append('')
    L.append('#### Slots\n')
    L.append('| Slot | Values |')
    L.append('|---|---|')
    L.append('| `[CALL]`, `[CALL2]` | generated like the M32 does (`getRandomCall()`), continent-tagged |')
    for k in sorted(pools):
        vals = [v[0] if isinstance(v, list) else v for v in pools[k]]
        L.append('| `[%s]` | %s |' % (k, ' '.join(vals)))
    L.append('')
    return '\n'.join(L)

if __name__ == '__main__':
    main()
