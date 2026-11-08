/*
 * m32_trainer_engine.js -- the M32 Trainer's engine (milestone M3).
 *
 * The middle layer (link -> engine -> page): it decides what to practise, scores what the
 * learner answered, keeps the per-character mastery model and the attempt log. It knows
 * nothing about the serial protocol (the session gets a `player` with play()/stop(), which
 * the page builds on M32Link) and nothing about the DOM, so the whole file runs under Node
 * for the tests (Software/tests/trainer/) and in the page as a plain <script>, where it
 * defines the global M32Engine.
 *
 * Spec: devdocs/training-addon/LEAN_V1_SCOPE.md §4, §4b and decision 4 (mastery), 5 (storage).
 *
 * Text notation: the display form the protocol uses for `GET kochlesson` and the trainer
 * events -- lower-case letters, digits, punctuation, prosigns as "<sk>" -- so one prosign is
 * one character ("token") everywhere in the engine.
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.M32Engine = factory();
}(typeof self !== 'undefined' ? self : this, function () {
'use strict';

// The trainer's three files (page, link, engine) carry one VERSION and change together: bump it in
// all three on every change. The page loads the scripts as ?v=VERSION and checks they match, so a
// browser cannot mix a new page with a cached old script (Software/tests/trainer/version.test.js).
const VERSION = '2026-11-08';
const SCHEMA = 1;                 // attempt records and the export file

// ---- text ----------------------------------------------------------------

// "ab<sk> c" -> ['a', 'b', '<sk>', ' ', 'c']. Lower-cased; whitespace runs become one ' ';
// a '<' that does not open a known-looking prosign (letters then '>') stays a character.
// A prosign may also be typed the other ways the M32 reads it (utf8umlaut() in m32_v6.ino):
// [sk] and \sk; <ar> and <bt> are the characters + and =, as the Koch lessons list them.
const PROSIGN_ALIAS = { '<ar>': '+', '<bt>': '=' };
function prosignToken(letters) {
  const t = '<' + letters + '>';
  return PROSIGN_ALIAS[t] || t;
}
function tokenize(text) {
  const s = String(text == null ? '' : text).toLowerCase();
  const out = [];
  for (let i = 0; i < s.length; ) {
    const c = s[i];
    if (/\s/.test(c)) {
      if (out.length && out[out.length - 1] !== ' ') out.push(' ');
      while (i < s.length && /\s/.test(s[i])) i++;
      continue;
    }
    if (c === '<' || c === '[') {
      const m = /^[<[]([a-z]{2,3})[>\]]/.exec(s.slice(i, i + 5));
      if (m) { out.push(prosignToken(m[1])); i += m[0].length; continue; }
    }
    if (c === '\\') {
      const m = /^\\(as|ka|kn|sk|ve|bk|ar|bt)/.exec(s.slice(i, i + 3));
      if (m) { out.push(prosignToken(m[1])); i += m[0].length; continue; }
    }
    const cp = s.codePointAt(i);
    const ch = String.fromCodePoint(cp);
    out.push(ch);
    i += ch.length;
  }
  if (out[0] === ' ') out.shift();
  if (out[out.length - 1] === ' ') out.pop();
  return out;
}

function normalize(text) {
  return tokenize(text).join('');
}

// -> array of words, each an array of tokens
function splitWords(text) {
  const words = [];
  let cur = [];
  for (const t of tokenize(text)) {
    if (t === ' ') { if (cur.length) words.push(cur); cur = []; }
    else cur.push(t);
  }
  if (cur.length) words.push(cur);
  return words;
}

function isProsign(token) {
  return token.length > 1 && token[0] === '<';
}

// ---- scoring --------------------------------------------------------------

// Edit-distance table between token arrays a and b with unit costs; returns the alignment
// as a list of ops {op: 'match'|'sub'|'del'|'ins', a, b} (del = in a only, ins = in b only).
function alignTokens(a, b) {
  const n = a.length, m = b.length;
  const d = [];
  for (let i = 0; i <= n; i++) { d.push(new Array(m + 1)); d[i][0] = i; }
  for (let j = 0; j <= m; j++) d[0][j] = j;
  for (let i = 1; i <= n; i++)
    for (let j = 1; j <= m; j++)
      d[i][j] = Math.min(d[i - 1][j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1), d[i - 1][j] + 1, d[i][j - 1] + 1);
  const ops = [];
  let i = n, j = m;
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0 && d[i][j] === d[i - 1][j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1)) {
      ops.push({ op: a[i - 1] === b[j - 1] ? 'match' : 'sub', a: a[i - 1], b: b[j - 1] });
      i--; j--;
    } else if (i > 0 && d[i][j] === d[i - 1][j] + 1) {
      ops.push({ op: 'del', a: a[i - 1], b: null });
      i--;
    } else {
      ops.push({ op: 'ins', a: null, b: b[j - 1] });
      j--;
    }
  }
  return { distance: d[n][m], ops: ops.reverse() };
}

// Word alignment: substituting word x by y costs their character distance over the longer
// length (0..1), dropping or adding a word costs 1 -- so a word is paired with the answer's
// most similar word, and a missing space ("kmr st" typed "kmrst") costs one word, not two.
function alignWords(exp, got) {
  const n = exp.length, m = got.length;
  const cost = (x, y) => alignTokens(x, y).distance / Math.max(x.length, y.length);
  const d = [];
  for (let i = 0; i <= n; i++) { d.push(new Array(m + 1)); d[i][0] = i; }
  for (let j = 0; j <= m; j++) d[0][j] = j;
  const c = [];
  for (let i = 1; i <= n; i++) {
    c.push([]);
    for (let j = 1; j <= m; j++) {
      c[i - 1].push(cost(exp[i - 1], got[j - 1]));
      d[i][j] = Math.min(d[i - 1][j - 1] + c[i - 1][j - 1], d[i - 1][j] + 1, d[i][j - 1] + 1);
    }
  }
  const pairs = [];
  let i = n, j = m;
  const eq = (x, y) => Math.abs(x - y) < 1e-9;
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0 && eq(d[i][j], d[i - 1][j - 1] + c[i - 1][j - 1])) {
      pairs.push([exp[i - 1], got[j - 1]]); i--; j--;
    } else if (i > 0 && eq(d[i][j], d[i - 1][j] + 1)) {
      pairs.push([exp[i - 1], null]); i--;
    } else {
      pairs.push([null, got[j - 1]]); j--;
    }
  }
  return pairs.reverse();
}

// Scores `answer` against `expected` (LEAN_V1_SCOPE §4b "Scoring a multi-word answer"):
// word alignment first, then character alignment inside each paired word. Every expected
// character gets exactly one entry in `chars` (what the mastery model counts); characters
// typed in excess only count in `extra`.
function score(expected, answer) {
  const ew = splitWords(expected), aw = splitWords(answer);
  const words = [], chars = [];
  let extra = 0;
  for (const [e, a] of alignWords(ew, aw)) {
    if (!e) { extra += a.length; words.push({ expected: null, got: a.join(''), ok: false }); continue; }
    if (!a) {
      for (const t of e) chars.push({ expected: t, got: null, ok: false });
      words.push({ expected: e.join(''), got: null, ok: false });
      continue;
    }
    for (const op of alignTokens(e, a).ops) {
      if (op.op === 'ins') { extra++; continue; }
      chars.push({ expected: op.a, got: op.b, ok: op.op === 'match' });
    }
    words.push({ expected: e.join(''), got: a.join(''), ok: e.join('') === a.join('') });
  }
  const wordsRight = words.filter((w) => w.ok).length;
  const wordCount = ew.length;
  const charsRight = chars.filter((c) => c.ok).length;
  return {
    expected: normalize(expected), answer: normalize(answer),
    words, chars, extra,
    wordsRight, wordCount, charsRight, charCount: chars.length,
    perfect: wordsRight === wordCount && words.length === wordCount
  };
}

// ---- mastery ----------------------------------------------------------------

const RULES = {
  boxes: 5,          // Leitner boxes 1..5, everyone starts in 1
  window: 10,        // promotion looks at the last 10 attempts ...
  promoteAt: 0.9,    // ... of which >= 90 % must be right
  minSince: 3,       // and >= 3 attempts since the box last changed
  missDrop: 2,       // a miss drops two boxes (not below 1)
  advanceAt: 3       // lesson advance is offered when every character is in box >= 3
};

// Does this attempt move boxes? Typed answers do; keyed ones feed separate tallies
// (decision 11: a keyed miss may be perception, memory or fingers); an item answered with
// help (a replay before answering, a revealed letter) is stored and counted in the stats
// but never changes a box -- neither up nor down; neither does a corrected keyed answer.
function movesBoxes(a) {
  return a.scored !== false && (a.source || 'typed') === 'typed' && !a.helped && !(a.corrections > 0);
}

// The per-character model, derived by replaying the attempt log (decision 5: the log is
// the source of truth, so changed thresholds only need a replay).
class Mastery {
  constructor(rules) {
    this.rules = Object.assign({}, RULES, rules || {});
    this.chars = new Map();
    this.confusions = new Map();      // "expected\u0000got" -> count
    this.calls = { call: newCallStats(), prefix: newCallStats() };   // M4b: their own tally
    this.echo = newEchoStats();                                        // M5: the M32's Echo Trainer
  }

  static fromAttempts(attempts, rules) {
    const m = new Mastery(rules);
    for (const a of attempts.slice().sort((x, y) => x.t - y.t)) m.apply(a);
    return m;
  }

  _get(ch) {
    let s = this.chars.get(ch);
    if (!s) {
      s = { box: 1, since: 0, recent: [], attempts: 0, correct: 0, latencies: [],
            keyed: { attempts: 0, correct: 0 } };
      this.chars.set(ch, s);
    }
    return s;
  }

  apply(a) {
    if (a.scored === false || !Array.isArray(a.perChar)) return;
    if (a.kind === 'echo') { applyEcho(this.echo, a); return; }   // M5: own tally, no box (Willi, 2026-11-07)
    if (CALL_KINDS.indexOf(a.kind) >= 0) {   // M4b: call signs and prefixes move no box and are no confusion
      const st = this.calls[a.kind];
      const len = String(a.expected || '').length;
      const bl = st.byLength.get(len) || { items: 0, perfect: 0 };
      st.items++; bl.items++;
      if (a.perfect) { st.perfect++; bl.perfect++; }
      st.byLength.set(len, bl);
      for (const c of a.perChar) {
        const m = st.chars.get(c.expected) || { attempts: 0, missed: 0 };
        m.attempts++;
        if (!c.ok) m.missed++;
        st.chars.set(c.expected, m);
      }
      return;
    }
    if ((a.source || 'typed') !== 'typed') {
      for (const c of a.perChar) { const s = this._get(c.expected); s.keyed.attempts++; if (c.ok) s.keyed.correct++; }
      return;
    }
    const r = this.rules, moves = movesBoxes(a);
    for (const c of a.perChar) {
      const s = this._get(c.expected);
      s.attempts++;
      if (c.ok) s.correct++;
      if (a.latency != null) s.latencies.push(a.latency);
      if (!c.ok && c.got != null) {
        const k = c.expected + '\u0000' + c.got;
        this.confusions.set(k, (this.confusions.get(k) || 0) + 1);
      }
      if (!moves) continue;
      s.recent.push(!!c.ok);
      if (s.recent.length > r.window) s.recent.shift();
      s.since++;
      if (!c.ok) {
        const box = Math.max(1, s.box - r.missDrop);
        if (box !== s.box) { s.box = box; s.since = 0; }
      } else if (s.box < r.boxes && s.since >= r.minSince &&
                 s.recent.filter(Boolean).length / s.recent.length >= r.promoteAt) {
        s.box++;
        s.since = 0;
      }
    }
  }

  box(ch) {
    const s = this.chars.get(ch);
    return s ? s.box : 1;
  }

  // One row per character, for the stats view.
  row(ch) {
    const s = this.chars.get(ch);
    if (!s) return { ch, box: 1, attempts: 0, correct: 0, accuracy: null, medianLatency: null, keyed: { attempts: 0, correct: 0 } };
    return { ch, box: s.box, attempts: s.attempts, correct: s.correct,
             accuracy: s.attempts ? s.correct / s.attempts : null,
             medianLatency: median(s.latencies), keyed: Object.assign({}, s.keyed) };
  }

  // Most frequent confusions first: [{expected, got, count}]
  confusionList(limit) {
    const list = [];
    for (const [k, count] of this.confusions) {
      const [expected, got] = k.split('\u0000');
      list.push({ expected, got, count });
    }
    list.sort((x, y) => y.count - x.count || (x.expected < y.expected ? -1 : 1));
    return limit ? list.slice(0, limit) : list;
  }

  // Decision 4d: offered (never done) when every character of the lesson is in box >= 3.
  readyToAdvance(chars) {
    return chars.length > 0 && chars.every((ch) => this.box(ch) >= this.rules.advanceAt);
  }
}

const CALL_KINDS = ['call', 'prefix'];
function newCallStats() { return { items: 0, perfect: 0, byLength: new Map(), chars: new Map() }; }

// The call or prefix tally for the stats view: {items, perfect, byLength: [{length, items, perfect}],
// missed: [{ch, missed, attempts}] most missed first}
function callSummary(st, limit) {
  const byLength = [...st.byLength].map(([length, v]) => ({ length, items: v.items, perfect: v.perfect }))
    .sort((x, y) => x.length - y.length);
  const missed = [...st.chars].filter(([, v]) => v.missed > 0).map(([ch, v]) => ({ ch, missed: v.missed, attempts: v.attempts }))
    .sort((x, y) => y.missed - x.missed || y.missed / y.attempts - x.missed / x.attempts || (x.ch < y.ch ? -1 : 1));
  return { items: st.items, perfect: st.perfect, byLength, missed: limit ? missed.slice(0, limit) : missed };
}

// ---- the Echo log (M5) -------------------------------------------------------------

// One {"trainer":{"event":"result","mode":"echo",...}} from the M32 (protocol 1.6, E1) as an attempt
// for the log: kind 'echo', the M32's own verdict, per character from the scorer (for "most missed").
// Only mode "echo" (Echo Trainer and the Koch Trainer's echo modes; Willi, 2026-11-07: not Learn New
// Chr / Preview Char) -> null for anything else. ctx: {device, t}.
function echoAttempt(ev, ctx) {
  if (!ev || ev.event !== 'result' || ev.mode !== 'echo') return null;
  ctx = ctx || {};
  const s = score(ev.prompt || '', ev.answer || '');
  return {
    schema: SCHEMA, id: newId(), t: ctx.t != null ? ctx.t : Date.now(), device: ctx.device || null,
    kind: 'echo', source: 'echo', tier: null, set: null,
    lesson: ev.koch != null ? ev.koch : null, wpm: ev.wpm != null ? ev.wpm : null, farnsworth: null,
    expected: s.expected, answer: s.answer, perfect: !!ev.ok, ok: !!ev.ok,
    perWord: s.words, perChar: ev.timeout ? [] : s.chars, extra: s.extra,
    repeat: ev.repeat | 0, timeout: !!ev.timeout,
    latency: ev.latency != null ? ev.latency : null, duration: ev.duration != null ? ev.duration : null,
    keystrokes: [], corrections: 0, replays: 0, helped: false
  };
}

function newEchoStats() { return { items: 0, ok: 0, timeouts: 0, firstTry: 0, firstOk: 0, latencies: [], durations: [], chars: new Map() }; }
function applyEcho(st, a) {
  st.items++;
  if (a.ok) st.ok++;
  if (a.timeout) st.timeouts++;
  if (!a.repeat) { st.firstTry++; if (a.ok) st.firstOk++; }
  if (a.latency != null && !a.timeout) st.latencies.push(a.latency);
  if (a.duration != null && a.ok) st.durations.push(a.duration);
  for (const c of a.perChar || []) {
    const m = st.chars.get(c.expected) || { attempts: 0, missed: 0 };
    m.attempts++;
    if (!c.ok) m.missed++;
    st.chars.set(c.expected, m);
  }
}
// {items, ok, timeouts, firstTry, firstOk, medianLatency, medianDuration, missed: [{ch, missed, attempts}]}
function echoSummary(st, limit) {
  const missed = [...st.chars].filter(([, v]) => v.missed > 0).map(([ch, v]) => ({ ch, missed: v.missed, attempts: v.attempts }))
    .sort((x, y) => y.missed - x.missed || y.missed / y.attempts - x.missed / x.attempts || (x.ch < y.ch ? -1 : 1));
  return { items: st.items, ok: st.ok, timeouts: st.timeouts, firstTry: st.firstTry, firstOk: st.firstOk,
           medianLatency: median(st.latencies), medianDuration: median(st.durations), missed: limit ? missed.slice(0, limit) : missed };
}

function median(xs) {
  if (!xs || !xs.length) return null;
  const s = xs.slice().sort((a, b) => a - b);
  const mid = s.length >> 1;
  return s.length % 2 ? s[mid] : (s[mid - 1] + s[mid]) / 2;
}

// ---- scheduling ----------------------------------------------------------

// Deterministic generator for the tests; the page uses Math.random.
function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a = (a + 0x6D2B79F5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// Picks the next group (decision 4b, 4c). `chars` is the learner's active set in lesson
// order (the last one is the newest), already reduced to what the M32 can play. `newest: false`
// for a set without an order (the M32's Practice Set): no character counts double.
class Scheduler {
  constructor(opts) {
    this.rng = opts.rng || Math.random;
    this.mastery = opts.mastery;
    this.chars = opts.chars.slice();
    this.newest = opts.newest !== false;
    this.size = opts.size || 'mixed';          // 'mixed' or a pinned 1..5
    this.retryGap = opts.retryGap != null ? opts.retryGap : 4;
    this.retries = [];                         // {kind, tier, n, text, wait}
    // M3b: what to practise. 'groups' (above), 'words' (one real word), 'phrases' (`length`
    // words, from `tier`: 'mixed' or one of 'A' word chains, 'B' plain language, 'C' QSO, 'U' the
    // learner's own lines). Words and phrases need `corpus` (a Corpus).
    this.mode = opts.mode || 'groups';
    this.length = opts.length || 2;
    this.tier = opts.tier || 'mixed';
    this.corpus = opts.corpus || null;
    // M4b: 'calls' and 'prefixes' -- the firmware's generators with the page's Length Calls,
    // Calls Region and Call Prefixes ({maxLength, region, commonOnly}); always all characters.
    this.callOpts = opts.callOpts || { maxLength: 0, region: 0, commonOnly: true };
    this.recent = [];                          // texts of the last RECENT items, not drawn again
  }

  setChars(chars, newest) {
    if (newest != null) this.newest = newest;
    this.chars = chars.slice();
    this.retries = this.retries.filter((r) => CALL_KINDS.indexOf(r.kind) >= 0 || tokenize(r.text).every((t) => chars.indexOf(t) >= 0));
  }

  // Draw weight = 6 - box; the newest character counts double until it reaches box 2.
  weight(ch) {
    let w = this.mastery.rules.boxes + 1 - this.mastery.box(ch);
    if (this.newest && ch === this.chars[this.chars.length - 1] && this.mastery.box(ch) < 2) w *= 2;
    return w;
  }

  // 'mixed': short groups while the boxes are low, longer later -- the longest size is the
  // rounded average box, and the size is that or one less.
  groupSize() {
    if (this.size !== 'mixed') return this.size;
    const avg = this.chars.reduce((s, ch) => s + this.mastery.box(ch), 0) / this.chars.length;
    const max = Math.min(5, Math.max(1, Math.round(avg)));
    return max === 1 ? 1 : max - 1 + Math.floor(this.rng() * 2);
  }

  pickChar() {
    const ws = this.chars.map((ch) => this.weight(ch));
    let r = this.rng() * ws.reduce((a, b) => a + b, 0);
    for (let i = 0; i < ws.length; i++) { r -= ws[i]; if (r < 0) return this.chars[i]; }
    return this.chars[this.chars.length - 1];
  }

  // -> {kind: 'group'|'word'|'phrase'|'call'|'prefix', tier: null|'A'|'B'|'C'|'U', text, retry}
  next() {
    for (const r of this.retries) r.wait--;
    const due = this.retries.findIndex((r) => r.wait <= 0 && this.fits(r));
    if (due >= 0) {
      const r = this.retries.splice(due, 1)[0];
      return { kind: r.kind || 'group', tier: r.tier || null, n: r.n, text: r.text, retry: true };
    }
    let item;
    if (this.mode === 'calls' || this.mode === 'prefixes') {
      item = this._call();
    } else if (!this.chars.length) {
      throw new Error('No characters to practise');
    } else if (this.mode === 'groups') {
      const n = this.groupSize();
      let text = '';
      for (let i = 0; i < n; i++) text += this.pickChar();
      item = { kind: 'group', tier: null, text };
    } else {
      const av = this.available();
      if (this.mode === 'words') {
        if (!av.words.length) throw new NoItemsError('words');
        item = { kind: 'word', tier: 'A', text: this.pickItem(av.words, (w) => w, null, true) };
      } else {
        const tiers = this.tier === 'mixed' ? av.eligible : (av.count[this.tier] ? [this.tier] : []);
        if (!tiers.length) throw new NoItemsError('phrases');
        const tier = tiers[Math.floor(this.rng() * tiers.length)];
        item = { kind: 'phrase', tier, n: this.length, text: this._phrase(av, tier) };
      }
    }
    this.recent.push(item.text);
    if (this.recent.length > RECENT) this.recent.shift();
    return Object.assign(item, { retry: false });
  }

  // A call sign or prefix, not one of the last RECENT items if a few draws can avoid it.
  _call() {
    const prefixes = (this.corpus && this.corpus.prefixes) || [];
    if (!prefixes.length) throw new NoItemsError(this.mode);
    const opts = Object.assign({}, this.callOpts, { rng: this.rng });
    let text;
    for (let i = 0; i < 8; i++) {
      text = this.mode === 'calls' ? randomCall(prefixes, opts).call : randomPrefix(prefixes, opts).prefix;
      if (this.recent.indexOf(text) < 0) break;
    }
    return { kind: this.mode === 'calls' ? 'call' : 'prefix', tier: null, text };
  }

  // What can be drawn now: {words, count: {A, B, C, U} at this length, eligible: tiers with >= 10}
  available() {
    return this.corpus.available(this.chars, this.length);
  }

  _phrase(av, tier) {
    if (tier === 'A') {                        // a chain of `length` different words
      const ws = [];
      while (ws.length < this.length) {
        const w = this.pickItem(av.words, (x) => x, ws, true);
        if (w == null) break;
        ws.push(w);
      }
      return ws.join(' ');
    }
    const it = this.pickItem(av.items[tier], (x) => x.text);
    return it.slots ? this.corpus.fill(it, this.chars, this.rng) : it.text;
  }

  // Decision 4b for words and phrases: from a random sample of the candidates (the pools are
  // large), weighted by how weak their characters are; the last RECENT items are skipped
  // while there is anything else. `avoid` lists texts not to repeat inside one chain;
  // `byFrequency` says the list is most frequent first (the words).
  pickItem(list, textOf, avoid, byFrequency) {
    const fresh = list.filter((x) => this.recent.indexOf(textOf(x)) < 0 && !(avoid && avoid.indexOf(textOf(x)) >= 0));
    const pool = fresh.length ? fresh : list;
    if (!pool.length) return null;
    // Words come most frequent first: lean the sample toward the front (r squared), as the
    // firmware weights its Koch words by frequency -- "the" far more often than "alignment".
    const at = (r) => Math.floor((byFrequency ? r * r : r) * pool.length);
    const sample = [];
    for (let i = 0; i < Math.min(SAMPLE, pool.length); i++) sample.push(pool[at(this.rng())]);
    const ws = sample.map((x) => this.itemWeight(textOf(x)));
    let r = this.rng() * ws.reduce((a, b) => a + b, 0);
    for (let i = 0; i < ws.length; i++) { r -= ws[i]; if (r < 0) return sample[i]; }
    return sample[sample.length - 1];
  }

  // 1 + the sum, over its distinct characters, of how far each is below the top box; the
  // newest character counts double until it reaches box 2 (as for groups). Slots count as nothing.
  itemWeight(text) {
    const seen = new Set(tokenize(text.replace(/\[[A-Z0-9]+\]/g, ' ')).filter((t) => t !== ' '));
    let w = 1;
    for (const ch of seen) w += this.weight(ch) - 1;
    return w;
  }

  // A missed item comes back, scored normally, after about `retryGap` other items.
  missed(item) {
    if (!this.retries.some((r) => r.text === item.text))
      this.retries.push({ kind: item.kind, tier: item.tier || null, n: item.n, text: item.text, wait: this.retryGap + 1 });
  }

  // Does a missed item fit what is selected now? One that doesn't waits until it does:
  // the learner's settings always hold (Willi, 2026-11-02).
  fits(r) {
    const kind = r.kind || 'group';
    if (this.mode === 'groups') return kind === 'group' && (this.size === 'mixed' || tokenize(r.text).length === this.size);
    if (this.mode === 'words') return kind === 'word';
    if (this.mode === 'calls') return kind === 'call';
    if (this.mode === 'prefixes') return kind === 'prefix';
    return kind === 'phrase' && r.n === this.length && (this.tier === 'mixed' || r.tier === this.tier);
  }
}

const RECENT = 20;              // words and phrases not drawn again within this many items
const SAMPLE = 40;              // candidates looked at per draw
const TIER_MIN = 10;            // decision 13b: a tier is offered with >= 10 usable items

class NoItemsError extends Error {
  constructor(what) { super('Nothing to practise as ' + what + ' with these characters yet'); this.name = 'NoItemsError'; this.what = what; }
}

// ---- call signs ------------------------------------------------------------

// The firmware's getRandomCall() and getRandomPrefix() (m32_v6.ino, with buildRandomPrefix() and
// pickAreaDigit()), for the Call signs and Prefixes kinds (M4b) and tier C's [CALL] slots.
// `prefixes` is the table from callsign_prefixes.h as the corpus carries it:
// [prefix, continent mask, CQ zone, weight, common (0/1), call-area digit weights], in table order;
// the digit weights are ten hex digits, one per digit 0-9 (all 0: the prefix takes no digit, as 3B8
// or I1). random(a, b) is a + floor(rng() * (b - a)), drawn in the firmware's order, so without
// `active` this makes exactly the firmware's choices for the same random numbers
// (Software/tests/callsign_gen/ checks it against the C++), the VK/ZL generator included.
//   opts.maxLength   the firmware's Length Calls value: 0 = any, 1 = 3 characters, n = prefix <= n
//   opts.region      the firmware's Calls Region value: 0 all, 1 EU, 2 NA, 3 SA, 4 AF, 5 AS, 6 OC,
//                    7 VK/ZL (its own generator); ignored for maxLength 1, as the firmware does
//   opts.continents  a continent mask instead of `region` (default all)
//   opts.commonOnly  only prefixes flagged common (Call Prefixes = Common only), default true
//   opts.active      the learner's characters (an array or Set): the call uses only those --
//                    prefixes made of them, a call-area digit among them (by the same weights) and
//                    suffix letters drawn from them. Returns null when no call can be made.
// -> {call, continent, zone}; randomPrefix() -> {prefix, continent, zone}
const CONT_ALL = 0x7F, CONT_OC = 0x20;
const REGION_MASKS = [CONT_ALL, 0x01, 0x02, 0x04, 0x08, 0x10, CONT_OC];   // getContinentMask()
const REGION_VKZL = 7;
const NO_AREA_DIGIT = '0000000000';

function callContext(opts) {
  opts = opts || {};
  const rng = opts.rng || Math.random;
  const act = opts.active ? new Set(opts.active) : null;
  const maxLength = opts.maxLength | 0;
  const vkzl = opts.region === REGION_VKZL && maxLength !== 1;
  let contMask = CONT_ALL;
  if (maxLength !== 1)
    contMask = vkzl ? CONT_OC : opts.region != null ? (REGION_MASKS[opts.region] || CONT_ALL)
             : opts.continents == null ? CONT_ALL : opts.continents;
  const c = {
    random: (a, b) => a + Math.floor(rng() * (b - a)),
    act, maxLength, contMask, vkzl, commonOnly: opts.commonOnly !== false,
    digits: act ? '0123456789'.split('').filter((ch) => act.has(ch)) : null,
    letters: act ? 'abcdefghijklmnopqrstuvwxyz'.split('').filter((ch) => act.has(ch)) : null
  };
  c.digit = () => (act ? c.digits[c.random(0, c.digits.length)] : String.fromCharCode(48 + c.random(0, 10)));
  c.letter = () => (act ? c.letters[c.random(0, c.letters.length)] : String.fromCharCode(97 + c.random(0, 26)));
  return c;
}

// buildRandomPrefix(): the base prefix plus its call-area digit. -> {pfx, continent, zone} or null
function buildPrefix(prefixes, c) {
  const { random, act } = c;
  if (c.vkzl) {                             // the VK/ZL generator: 85 % VK, 15 % ZL, by area weights
    const isVK = random(0, 100) < 85;
    const r = random(0, 100);
    const area = isVK ? [1, 11, 31, 51, 66, 76, 81, 96, 99, 100].findIndex((x) => r < x)
                      : [0, 35, 70, 90, 100].findIndex((x) => r < x);
    const pfx = (isVK ? 'vk' : 'zl') + area;
    if (act && ![...pfx].every((ch) => act.has(ch))) return null;
    return { pfx, continent: CONT_OC, zone: isVK ? 29 : 32 };
  }
  // The call-area digits a prefix can take, as [digit, weight]: those in use, and with `active`
  // only the learner's. [] for a prefix that takes none, and for one whose digits are all unlearned.
  const areaDigits = (row) => {
    const out = [];
    for (let d = 0; d < 10; d++) {
      const w = parseInt(row[5][d], 16);
      if (w && (!act || act.has(String(d)))) out.push([String(d), w]);
    }
    return out;
  };
  const takesDigit = (row) => row[5] !== NO_AREA_DIGIT;
  const maxPfxLen = c.maxLength > 0 ? c.maxLength : 99;
  const ok = (row) => (row[1] & c.contMask) && (!c.commonOnly || row[4]) && row[0].length <= maxPfxLen &&
    (!act || ([...row[0]].every((ch) => act.has(ch)) && (!takesDigit(row) || areaDigits(row).length > 0)));
  let total = 0;
  for (const row of prefixes) if (ok(row)) total += row[3];
  if (total === 0) {                        // the firmware's fallback: a letter and a digit
    if (act && (!c.digits.length || !c.letters.length)) return null;
    return { pfx: c.letter() + c.digit(), continent: CONT_ALL, zone: 0 };
  }
  const pick = random(0, total);
  let cum = 0, chosen = null;
  for (const row of prefixes) {
    if (!ok(row)) continue;
    cum += row[3];
    if (cum > pick) { chosen = row; break; }
  }
  let pfx = chosen[0];
  if (takesDigit(chosen)) {                 // pickAreaDigit(): by the digit weights
    const ad = areaDigits(chosen);
    let r = random(0, ad.reduce((sum, x) => sum + x[1], 0));
    for (const [d, w] of ad) if ((r -= w) < 0) { pfx += d; break; }
  }
  return { pfx, continent: chosen[1], zone: chosen[2] };
}

function randomCall(prefixes, opts) {
  const c = callContext(opts);
  const { random, act, maxLength } = c;
  if (act && !c.letters.length) return null;
  const p = buildPrefix(prefixes, c);
  if (!p) return null;
  let call = p.pfx;
  if (c.vkzl) {                             // VK/ZL: 30 % two suffix letters, 70 % three
    let n = random(0, 10) < 3 ? 2 : 3;
    while (n-- > 0) call += c.letter();
    return { call, continent: p.continent, zone: p.zone };
  }
  const maxTotal = maxLength === 0 ? 10 : maxLength > 4 ? 6 : maxLength + 2;
  let suffLen = Math.max(1, Math.min(3, maxTotal - call.length));
  if (maxLength === 0) {
    suffLen = random(1, 4);
    if (suffLen === 1 && random(0, 3) !== 0) suffLen = random(2, 4);
  }
  while (suffLen-- > 0 && call.length < 12) call += c.letter();
  if (maxLength === 0 && random(0, 8) === 0) {
    const pm = random(0, 2) ? 'm' : 'p';
    if (!act || (act.has('/') && act.has(pm))) call += '/' + pm;
  }
  return { call, continent: p.continent, zone: p.zone };
}

// getRandomPrefix(): Calls Region and Call Prefixes apply, Length Calls does not.
function randomPrefix(prefixes, opts) {
  const c = callContext(Object.assign({}, opts, { maxLength: 0 }));
  const p = buildPrefix(prefixes, c);
  return p && { prefix: p.pfx, continent: p.continent, zone: p.zone };
}

// ---- the corpus (M3b) ----------------------------------------------------------

// m32_trainer_corpus.json (built by Software/tools/trainer-corpus/build_corpus.py), plus the
// learner's own lines (tier U). Everything is filtered strictly by the active characters
// (decision 13a): an item is usable only if every one of its characters is in the set.
class Corpus {
  constructor(json) {
    if (!json || json.schema !== 1) throw new Error('Unknown corpus format');
    this.json = json;
    this.prefixes = json.prefixes || [];
    this.pools = json.pools || {};
    const prep = (p) => ({ tier: p.tier, src: p.src, text: normalize(p.text), n: splitWords(p.text).length,
                           need: tokenSet(p.text), slots: null });
    this.items = (json.phrases || []).map(prep);
    for (const t of json.templates || []) {
      const slots = (t.text.match(/\[[A-Z0-9]+\]/g) || []).map((x) => x.slice(1, -1));
      if (!slots.length) { this.items.push(prep(Object.assign({ tier: 'C', src: 'authored' }, t))); continue; }
      this.items.push({ tier: 'C', src: t.src || 'authored', text: t.text, n: t.text.split(' ').length,
                        need: tokenSet(t.text.replace(/\[[A-Z0-9]+\]/g, ' ')), slots });
    }
    this.words = (json.words || []).map((w) => ({ text: w, need: tokenSet(w) }));
    this.user = [];
    this._cache = null;
  }

  // The learner's own lines (decision C3): one phrase per line, blank lines and #comments
  // skipped. -> {lines, kept} -- kept are the 2..8-word lines (any characters; the lesson
  // filter applies when drawing).
  setUser(text) {
    const lines = String(text || '').split(/\r?\n/).map((l) => l.trim()).filter((l) => l && l[0] !== '#');
    this.user = [];
    for (const l of lines) {
      const t = normalize(l), n = splitWords(t).length;
      if (n >= 2 && n <= 8) this.user.push({ tier: 'U', src: 'user', text: t, n, need: tokenSet(t), slots: null });
    }
    this._cache = null;
    return { lines: lines.length, kept: this.user.length };
  }

  // -> {words: [text], items: {B, C, U}: [item] at `length`, count: {A, B, C, U}, eligible: [tiers]}
  // A counts the usable words (a chain can be built from any 10 of them).
  available(chars, length) {
    const key = chars.join('\u0000') + '|' + length;
    if (this._cache && this._cache.key === key) return this._cache.value;
    const act = new Set(chars);
    const fits = (need) => { for (const t of need) if (!act.has(t)) return false; return true; };
    const words = this.words.filter((w) => fits(w.need)).map((w) => w.text);
    const callOk = randomCall(this.prefixes, { active: act }) !== null;
    const slotOk = (s) => (s === 'CALL' || s === 'CALL2') ? callOk : this._values(s, act).length > 0;
    const items = { B: [], C: [], U: [] };
    for (const it of this.items.concat(this.user)) {
      if (it.n !== length || !fits(it.need)) continue;
      if (it.slots && !it.slots.every(slotOk)) continue;
      items[it.tier].push(it);
    }
    const count = { A: words.length, B: items.B.length, C: items.C.length, U: items.U.length };
    const eligible = ['A', 'B', 'C', 'U'].filter((t) => count[t] >= TIER_MIN);
    const value = { words, items, count, eligible };
    this._cache = { key, value };
    return value;
  }

  // Pool values usable with these characters: [[value, continent mask] | value]
  _values(slot, act) {
    return (this.pools[slot] || []).filter((v) => {
      for (const t of tokenSet(Array.isArray(v) ? v[0] : v)) if (!act.has(t)) return false;
      return true;
    });
  }

  // A template's text with its slots filled: [CALL] first, and the names, QTHs and
  // summit/park references then picked to suit that call's continent (as the QSO Bot does),
  // any value if none suits. Only values made of the active characters.
  fill(item, chars, rng) {
    rng = rng || Math.random;
    const act = new Set(chars);
    const one = (xs) => xs[Math.floor(rng() * xs.length)];
    const calls = {};
    let continent = CONT_ALL;
    for (const s of ['CALL', 'CALL2']) {
      if (item.slots.indexOf(s) < 0) continue;
      const c = randomCall(this.prefixes, { rng, active: act });
      calls[s] = c.call;
      if (s === 'CALL' || continent === CONT_ALL) continent = c.continent;
    }
    return item.text.replace(/\[([A-Z0-9]+)\]/g, (m, s) => {
      if (calls[s]) return calls[s];
      const vals = this._values(s, act);
      const suits = vals.filter((v) => Array.isArray(v) && (v[1] & continent));
      const v = one(suits.length ? suits : vals);
      return Array.isArray(v) ? v[0] : v;
    });
  }
}

function tokenSet(text) {
  return new Set(tokenize(text).filter((t) => t !== ' '));
}

// Decision 13b: after the last 10 phrases of this length (typed, scored), offer a step up at
// >= 90 % of the words right, a step down below 60 % -- never automatic. -> 'up' | 'down' | null
function ladderOffer(attempts, length) {
  const at = attempts.filter((a) => a.kind === 'phrase' && a.scored !== false && (a.source || 'typed') === 'typed' &&
                                    Array.isArray(a.perWord) && a.perWord.length === length)
                     .sort((x, y) => x.t - y.t).slice(-10);
  if (at.length < 10) return null;
  // right words over all words (integers: a mean of per-phrase ratios drifts, and 60 % came out as 59.99...)
  const acc = at.reduce((s, a) => s + a.perWord.filter((w) => w.ok).length, 0) / (at.length * length);
  if (acc >= 0.9 && length < 8) return 'up';
  if (acc < 0.6 && length > 2) return 'down';
  return null;
}

// ---- the session ---------------------------------------------------------

function newId() {
  if (typeof crypto !== 'undefined' && crypto.randomUUID) return crypto.randomUUID();
  return Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 10);
}

// One practice run: play an item, take the answer, score it, log it.
//
// opts: player {play(text) -> {started: Promise, done: Promise<{reason, t}>}, stop()};
//       store (see MemoryStore); mastery; scheduler; context () -> {device, lesson, wpm};
//       clock {now() (same clock as the player's `t`), wall() (epoch ms)};
//       helpLimit (replays allowed before answering, default 1).
//
// States: idle -> playing -> answering -> (feedback | idle ...) ; 'paused' after a lost link.
// A missed keyed answer goes feedback-playing -> second-try -> feedback (see secondTry()).
// Events (on(name, fn)): state, item, result, second, notice.
class Session {
  constructor(opts) {
    this.player = opts.player;
    this.store = opts.store;
    this.mastery = opts.mastery;
    this.scheduler = opts.scheduler;
    this.context = opts.context || (() => ({}));
    this.clock = opts.clock || { now: () => Date.now(), wall: () => Date.now() };
    this.helpLimit = opts.helpLimit != null ? opts.helpLimit : 1;
    this.state = 'idle';
    this.item = null;
    this.tally = { items: 0, perfect: 0, chars: 0, charsRight: 0 };
    this.callTally = { items: 0, perfect: 0, chars: 0, charsRight: 0 };   // M4b: calls and prefixes, typed or keyed
    this.keyedTally = { items: 0, perfect: 0, chars: 0, charsRight: 0 };   // keyed answers count apart
    this._listeners = new Map();
    this._gen = 0;              // bumps whenever a play becomes stale
  }

  on(name, fn) {
    if (!this._listeners.has(name)) this._listeners.set(name, []);
    this._listeners.get(name).push(fn);
    return () => { const l = this._listeners.get(name); l.splice(l.indexOf(fn), 1); };
  }

  _emit(name, a) {
    for (const fn of (this._listeners.get(name) || []).slice()) {
      try { fn(a); } catch (e) { if (typeof console !== 'undefined') console.error('M32Engine listener for "' + name + '":', e); }
    }
  }

  _setState(s) {
    if (s === this.state) return;
    this.state = s;
    this._emit('state', s);
  }

  // Start (or continue with) the next item.
  next() {
    if (['idle', 'feedback', 'feedback-playing', 'second-try'].indexOf(this.state) < 0) return;
    const pick = this.scheduler.next();
    this.item = { kind: pick.kind, tier: pick.tier || null, n: pick.n, text: pick.text, retry: pick.retry, doneAt: null, keys: [],
                  replays: 0, helped: false, stops: 0 };
    this._emit('item', this.item);
    this._play(false);
  }

  // Nothing was keyed for a long time: sound it once more. Not help, as the learner asked for nothing.
  playAgain() {
    if (!this.item || this.state !== 'answering') return false;
    this._play(false);
    return true;
  }

  // Help: hear it again before answering (counted; the item then moves no box).
  replay() {
    const it = this.item;
    if (!it || this.state !== 'answering' || it.replays >= this.helpLimit) return false;
    it.replays++;
    it.helped = true;
    this._play(false);
    return true;
  }

  _play(unscored) {
    const it = this.item, gen = ++this._gen;
    this._setState(unscored ? 'feedback-playing' : 'playing');
    const p = this.player.play(it.text);
    p.done.then((d) => {
      if (gen !== this._gen) return;
      if (d.reason === 'end') {
        if (unscored) { this._setState(it.secondTry ? 'second-try' : 'feedback'); return; }
        it.doneAt = d.t;
        this._setState('answering');
      } else if (d.reason === 'stopped' && (!unscored || it.secondTry) && it.stops < 3) {
        it.stops++;                       // a paddle touch (R2): play it again, it costs nothing
        this._emit('notice', 'stopped');
        this._play(unscored);
      } else if (d.reason === 'stopped') {
        this._setState(!unscored ? 'answering' : it.secondTry ? 'second-try' : 'feedback');
        if (!unscored && it.doneAt == null) it.doneAt = d.t;
      } else if (d.reason === 'replaced') {
        // a later play of ours took over; its own done settles things
      } else {
        this._emit('notice', d.reason === 'lost' ? 'lost' : 'refused');
        this._setState('paused');
      }
    });
  }

  // A key the learner pressed in the answer field, with its time on the session clock.
  // Typing while the item sounds is allowed (that is how copying works).
  key(k, t) {
    const it = this.item;
    if (!it || (this.state !== 'playing' && this.state !== 'answering')) return;
    it.keys.push({ k, t: t != null ? t : this.clock.now() });
  }

  // The learner is done: score, log, update the model. Returns the attempt, or null when
  // the item is still sounding (the page says "wait").
  // keyed: {corrections, spacing} when the answer came from the paddle (see KeyedAnswer).
  async submit(answer, t, keyed) {
    const it = this.item;
    if (!it || this.state !== 'answering') return null;
    t = t != null ? t : this.clock.now();
    const s = score(it.text, answer);
    const ctx = this.context() || {};
    const first = it.keys.length ? it.keys[0].t : null;
    const attempt = {
      schema: SCHEMA, id: newId(), t: this.clock.wall(),
      device: ctx.device || null, lesson: ctx.lesson != null ? ctx.lesson : null, wpm: ctx.wpm != null ? ctx.wpm : null,
      farnsworth: ctx.farnsworth != null ? ctx.farnsworth : null, set: ctx.set || null,
      kind: it.kind, tier: it.tier || null, source: keyed ? 'keyed' : 'typed', retry: it.retry,
      expected: s.expected, answer: s.answer, perfect: s.perfect,
      perWord: s.words, perChar: s.chars, extra: s.extra,
      latency: first != null && it.doneAt != null ? Math.round(first - it.doneAt) : null,
      total: it.doneAt != null ? Math.round(t - it.doneAt) : null,
      keystrokes: it.keys.map((k) => ({ k: k.k, t: it.doneAt != null ? Math.round(k.t - it.doneAt) : null })),
      corrections: keyed ? keyed.corrections : 0, replays: it.replays, helped: it.helped
    };
    if (keyed) attempt.spacing = keyed.spacing;
    await this.store.add(attempt);
    this.mastery.apply(attempt);
    const tally = CALL_KINDS.indexOf(it.kind) >= 0 ? this.callTally : keyed ? this.keyedTally : this.tally;
    tally.items++;
    if (s.perfect) tally.perfect++;
    tally.chars += s.charCount;
    tally.charsRight += s.charsRight;
    this._emit('result', { attempt, score: s });
    if (s.perfect) {
      this._setState('idle');
    } else {
      this.scheduler.missed(it);       // decision 4c: it comes back after ~4 others, scored
      if (keyed) { it.secondTry = true; it.stops = 0; }
      this._play(true);                // ... and is played once more now, unscored
    }
    return attempt;
  }

  // A missed keyed answer, heard again: the learner may key it once more (Willi, 2026-11-04, bench 7.3:
  // a hand on the paddle keys what it has just heard). Marked right or wrong, and nothing is kept: no log,
  // no tally, no box, and the scored attempt still comes later. answer null: no try was made.
  secondTry(answer) {
    if (!this.item || this.state !== 'second-try') return null;
    const s = answer == null ? null : score(this.item.text, answer);
    if (s) this._emit('second', { score: s });
    this._setState('feedback');
    return s;
  }

  // Cable gone, page hidden, the user paused: forget the running play.
  pause() {
    this._gen++;
    if (this.state === 'playing' || this.state === 'feedback-playing') {
      try { const p = this.player.stop(); if (p && p.catch) p.catch(() => {}); } catch (e) { /* gone */ }
    }
    this._setState('paused');
  }

  resume() {
    if (this.state !== 'paused') return;
    this._setState('idle');
  }
}

// ---- a keyed answer ----------------------------------------------------------

// What the paddle sends for one item, read from the M32's character echo (PUT cw/practice).
// Decisions 11a and 11b, as changed by Willi on 2026-11-04. Knows no clock of its own: start(),
// feed() and check() get the time, and the page asks check() while the answer runs.
//   check() -> 'sign'   a keyed = or <bt> the target does not need
//              'early'  as many characters as the target has, and graceMs without another one
//              'idle'   some characters, then idleMs without another one
//              'start'  nothing at all for startMs: the page sounds the item again, then pauses
//              null     keep waiting
class KeyedAnswer {
  constructor(target, opts) {
    opts = opts || {};
    const t = tokenize(target);
    this.chars = t.filter((x) => x !== ' ');
    this.oneWord = t.indexOf(' ') < 0;
    this.idleMs = opts.idleMs != null ? opts.idleMs : 4000;
    this.graceMs = opts.graceMs != null ? opts.graceMs : 1500;
    this.startMs = opts.startMs != null ? opts.startMs : 15000;
    this.tokens = [];
    this.corrections = 0;
    this._partial = '';         // a prosign cut in two by the transport
    this._justCorrected = false;
    this._sign = false;
    this._since = null;         // the last character or correction, or the end of the sound
  }

  // The sound has ended (again): the waiting starts now.
  start(now) { this._since = now; }

  get text() { return this.tokens.join(''); }
  get count() { return this.tokens.filter((x) => x !== ' ').length; }

  // Echo text as it arrives -> the tokens in it. The M32 may send them in upper case (Output Case).
  feed(text, now) {
    const s = this._partial + String(text == null ? '' : text).toLowerCase();
    this._partial = '';
    const seen = [];
    for (let i = 0; i < s.length; ) {
      const c = s[i];
      if (c === '<') {
        const close = s.indexOf('>', i);
        if (close < 0 && s.length - i < 6) { this._partial = s.slice(i); break; }
        const m = close > 0 ? /^<([a-z]{2,3})>$/.exec(s.slice(i, close + 1)) : null;
        if (m) { this._token(m[1] === 'err' ? '<err>' : prosignToken(m[1]), now, seen); i = close + 1; continue; }
      }
      const ch = String.fromCodePoint(s.codePointAt(i));
      this._token(/\s/.test(ch) ? ' ' : ch, now, seen);
      i += ch.length;
    }
    return seen;
  }

  _token(t, now, seen) {
    seen.push(t);
    if (t === ' ') {            // a word space: not input, and the decoder also sends one after each answer
      if (this.tokens.length && this.tokens[this.tokens.length - 1] !== ' ') this.tokens.push(' ');
      return;
    }
    this._since = now;
    if (t === '<err>' || t === '*') {
      this.corrections++;
      if (this._justCorrected) this.tokens = [];
      else {
        while (this.tokens.length && this.tokens[this.tokens.length - 1] === ' ') this.tokens.pop();
        while (this.tokens.length && this.tokens[this.tokens.length - 1] !== ' ') this.tokens.pop();
      }
      this._justCorrected = true;
      return;
    }
    this._justCorrected = false;
    // = finishes, unless this target still has one to come
    if (t === '=' && this.tokens.filter((x) => x === '=').length >= this.chars.filter((x) => x === '=').length) {
      this._sign = true;
      return;
    }
    this.tokens.push(t);
  }

  check(now) {
    if (this._sign) return 'sign';
    if (this._since == null) return null;
    const quiet = now - this._since;
    const n = this.count;
    if (!n) return quiet >= this.startMs ? 'start' : null;
    if (n >= this.chars.length && quiet >= this.graceMs) return 'early';
    return quiet >= this.idleMs ? 'idle' : null;
  }

  // What the scorer gets. Strict: as it was keyed. Lenient: a group or a single word has no
  // spaces to get wrong; a phrase keeps its word breaks either way.
  answer(spacing) {
    return spacing === 'lenient' && this.oneWord ? this.tokens.filter((x) => x !== ' ').join('') : this.text;
  }
}

// ---- storage ---------------------------------------------------------------

// The store interface: add(attempt), all(), importMany(attempts) -> {added, skipped},
// getMeta(key), setMeta(key, value), pruneKeystrokes(beforeWall) -> count, clear().
class MemoryStore {
  constructor() { this.attempts = new Map(); this.meta = new Map(); }
  async add(a) { this.attempts.set(a.id, a); }
  async all() { return Array.from(this.attempts.values()).sort((x, y) => x.t - y.t); }
  async importMany(list) {
    let added = 0, skipped = 0;
    for (const a of list) { if (this.attempts.has(a.id)) skipped++; else { this.attempts.set(a.id, a); added++; } }
    return { added, skipped };
  }
  async getMeta(k) { return this.meta.has(k) ? this.meta.get(k) : null; }
  async setMeta(k, v) { this.meta.set(k, v); }
  async pruneKeystrokes(before) {
    let n = 0;
    for (const a of this.attempts.values()) if (a.t < before && a.keystrokes) { delete a.keystrokes; n++; }
    return n;
  }
  async clear() { this.attempts.clear(); this.meta.clear(); }
}

// IndexedDB, in the browser only (decision 5). Two object stores: attempts (keyPath id,
// index t) and meta (key/value).
class IdbStore {
  constructor(db) { this.db = db; }

  static open(name) {
    return new Promise((resolve, reject) => {
      const req = indexedDB.open(name || 'm32trainer', 1);
      req.onupgradeneeded = () => {
        const db = req.result;
        if (!db.objectStoreNames.contains('attempts')) db.createObjectStore('attempts', { keyPath: 'id' }).createIndex('t', 't');
        if (!db.objectStoreNames.contains('meta')) db.createObjectStore('meta');
      };
      req.onsuccess = () => {
        // Give the database up when another tab needs it (a newer version's upgrade, or a delete):
        // otherwise that tab waits for ever. This tab's store is then closed (reload to go on).
        req.result.onversionchange = () => req.result.close();
        resolve(new IdbStore(req.result));
      };
      req.onerror = () => reject(req.error);
      req.onblocked = () => reject(new Error('The trainer is open in another tab that holds an older database'));
    });
  }

  _tx(stores, mode, fn) {
    return new Promise((resolve, reject) => {
      const tx = this.db.transaction(stores, mode);
      let result;
      tx.oncomplete = () => resolve(result);
      tx.onerror = () => reject(tx.error);
      tx.onabort = () => reject(tx.error || new Error('transaction aborted'));
      result = fn(tx);
    });
  }

  static _req(r) { return new Promise((res, rej) => { r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error); }); }

  add(a) { return this._tx(['attempts'], 'readwrite', (tx) => { tx.objectStore('attempts').put(a); }); }

  async all() {
    const tx = this.db.transaction(['attempts'], 'readonly');
    return IdbStore._req(tx.objectStore('attempts').index('t').getAll());
  }

  importMany(list) {
    const counts = { added: 0, skipped: 0 };
    return this._tx(['attempts'], 'readwrite', (tx) => {
      const os = tx.objectStore('attempts');
      for (const a of list) {
        const r = os.add(a);
        r.onsuccess = () => counts.added++;
        r.onerror = (e) => { e.preventDefault(); e.stopPropagation(); counts.skipped++; };   // id exists: keep ours
      }
      return counts;
    });
  }

  async getMeta(k) {
    const v = await IdbStore._req(this.db.transaction(['meta'], 'readonly').objectStore('meta').get(k));
    return v === undefined ? null : v;
  }

  setMeta(k, v) { return this._tx(['meta'], 'readwrite', (tx) => { tx.objectStore('meta').put(v, k); }); }

  pruneKeystrokes(before) {
    let n = 0;
    return this._tx(['attempts'], 'readwrite', (tx) => {
      const os = tx.objectStore('attempts');
      os.index('t').openCursor(IDBKeyRange.upperBound(before, true)).onsuccess = (e) => {
        const cur = e.target.result;
        if (!cur) return;
        if (cur.value.keystrokes) { const v = cur.value; delete v.keystrokes; cur.update(v); n++; }
        cur.continue();
      };
      return { get count() { return n; } };
    }).then((r) => r.count);
  }

  clear() { return this._tx(['attempts', 'meta'], 'readwrite', (tx) => { tx.objectStore('attempts').clear(); tx.objectStore('meta').clear(); }); }
}

const KEYSTROKE_DAYS = 180;       // raw key times are pruned after this (decision 5); summaries stay

function exportFile(attempts, wallNow) {
  return { schema: SCHEMA, app: 'M32 Trainer', exported: new Date(wallNow).toISOString(), attempts };
}

// -> the attempts of an export file, or throws with a message for the learner
function parseExport(text) {
  let j;
  try { j = typeof text === 'string' ? JSON.parse(text) : text; }
  catch (e) { throw new Error('This file is not an M32 Trainer export (not JSON).'); }
  if (!j || j.app !== 'M32 Trainer' || !Array.isArray(j.attempts)) throw new Error('This file is not an M32 Trainer export.');
  if (j.schema > SCHEMA) throw new Error('This export comes from a newer M32 Trainer (format ' + j.schema + '). Update the page first.');
  const ok = j.attempts.filter((a) => a && typeof a.id === 'string' && typeof a.t === 'number' && Array.isArray(a.perChar));
  return { attempts: ok, rejected: j.attempts.length - ok.length };
}

// ---- the learner's character set ---------------------------------------------

// From `GET kochlesson`: the active set is the first `value` characters (also when the
// lesson indexes a custom set -- the reply then lists that set).
// What the M32 can sound: CWchars[] in m32_v6.ino, as cleanUpProSigns() shows it. Anything else
// in a set -- e.g. a "\" that reached Custom Chars from player.txt before the 2026-11-06 fix of
// getWord() -- is skipped silently by generateCWword(), so the trainer must not ask for it.
const M32_SOUNDS = 'abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+'.split('')
  .concat(['<as>', '<ka>', '<kn>', '<sk>', '<ve>', '<bk>', 'ä', 'ö', 'ü', 'ch', '(', ')', 'é', 'è', 'à', 'ç', 'ñ', 'å', 'æ', 'ø']);
// "All characters" on the page: the M32's full standard set (its Koch order, as morserinoKochChars
// in MorsePreferences.h, prosigns as GET kochlesson shows them) -- what the CW Generator's
// Random "a9?<>" draws from. No national letters.
const ALL_CHARS = 'mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-='.split('')
  .concat(['<sk>', '+', '<as>', '<kn>', '<ka>', '<ve>', '<bk>', '@', ':']);
function hasMorse(token) {
  return M32_SOUNDS.indexOf(token) >= 0;
}

// `dropped` (optional array) collects what was left out because the M32 has no Morse code for it.
function lessonChars(kochlesson, dropped) {
  if (!kochlesson || !Array.isArray(kochlesson.characters)) return [];
  return keepSounding(kochlesson.characters.slice(0, Math.max(0, kochlesson.value | 0)).map((c) => String(c).toLowerCase()), dropped);
}
function keepSounding(tokens, dropped) {
  return tokens.filter((t) => {
    if (hasMorse(t)) return true;
    if (dropped && dropped.indexOf(t) < 0) dropped.push(t);
    return false;
  });
}

// From `GET practicechars` (the M32's Practice Set): raw codes, so the prosigns are single
// upper-case letters; written as `GET kochlesson` shows them (cleanUpProSigns() in m32_v6.ino),
// once each, in the M32's order.
const PRACTICE_CODES = { S: '<as>', A: '<ka>', N: '<kn>', K: '<sk>', E: '<ve>', B: '<bk>', H: 'ch', R: '<err>', U: '*' };
function practiceChars(practicechars, dropped) {
  const out = [];
  for (const c of String((practicechars && practicechars.characters) || '')) {
    const t = PRACTICE_CODES[c] || c.toLowerCase();
    if (t.trim() && out.indexOf(t) < 0) out.push(t);
  }
  return keepSounding(out, dropped);
}

// Farnsworth spacing for `PUT cw/spacing/<char>/<word>` (in dits of the character speed): the
// characters keep `wpm`, the spaces stretch so that PARIS takes as long as at `effective` WpM --
// 31 dits of marks and gaps inside the characters, 19 of spacing (4 character spaces of 3, a word
// space of 7), all spacing stretched by the same factor. Only whole dits: the character space is
// rounded, and the word space takes what is left, so PARIS is off by half a dit at most. Kept in
// the M32's ranges (Interchar Spc 3-45, InterWord Spc 6-105; a word space is never shorter than a
// character space + 4). -> {charSpace, wordSpace, effective: the WpM that results}, or null when
// `effective` is not below `wpm` (no Farnsworth).
const SPACING_LIMITS = { charMin: 3, charMax: 45, wordMin: 6, wordMax: 105 };
function farnsworthSpacing(wpm, effective) {
  if (!(wpm > 0) || !(effective > 0) || effective >= wpm) return null;
  const L = SPACING_LIMITS;
  const spaces = 50 * wpm / effective - 31;            // dits of spacing in PARIS: 4 character spaces + 1 word space
  let charSpace = Math.min(L.charMax, Math.max(L.charMin, Math.round(3 * spaces / 19)));
  if (spaces - 4 * charSpace < charSpace + 4)          // the word space would hit its minimum: a dit less here
    charSpace = Math.max(L.charMin, Math.floor((spaces - 4) / 5));
  const wordSpace = Math.min(L.wordMax, Math.max(L.wordMin, charSpace + 4, Math.round(spaces - 4 * charSpace)));
  return { charSpace, wordSpace, effective: 50 * wpm / (31 + 4 * charSpace + wordSpace) };
}


return {
  VERSION, SCHEMA, RULES, KEYSTROKE_DAYS,
  tokenize, normalize, splitWords, isProsign, alignTokens, alignWords, score,
  Mastery, movesBoxes, callSummary, echoAttempt, echoSummary, median, mulberry32, randomCall, randomPrefix, Corpus, ladderOffer, NoItemsError, Scheduler, Session, KeyedAnswer, newId,
  MemoryStore, IdbStore, exportFile, parseExport, lessonChars, practiceChars, hasMorse, ALL_CHARS, farnsworthSpacing
};
}));
