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
const VERSION = '2026-11-01';
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
// order (the last one is the newest), already reduced to what the M32 can play.
class Scheduler {
  constructor(opts) {
    this.rng = opts.rng || Math.random;
    this.mastery = opts.mastery;
    this.chars = opts.chars.slice();
    this.size = opts.size || 'mixed';          // 'mixed' or a pinned 1..5
    this.retryGap = opts.retryGap != null ? opts.retryGap : 4;
    this.retries = [];                         // {text, wait}
  }

  setChars(chars) { this.chars = chars.slice(); this.retries = this.retries.filter((r) => tokenize(r.text).every((t) => chars.indexOf(t) >= 0)); }

  // Draw weight = 6 - box; the newest character counts double until it reaches box 2.
  weight(ch) {
    let w = this.mastery.rules.boxes + 1 - this.mastery.box(ch);
    if (ch === this.chars[this.chars.length - 1] && this.mastery.box(ch) < 2) w *= 2;
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

  // -> {kind: 'group', text, retry}
  next() {
    for (const r of this.retries) r.wait--;
    const due = this.retries.findIndex((r) => r.wait <= 0);
    if (due >= 0) return { kind: 'group', text: this.retries.splice(due, 1)[0].text, retry: true };
    if (!this.chars.length) throw new Error('No characters to practise');
    const n = this.groupSize();
    let text = '';
    for (let i = 0; i < n; i++) text += this.pickChar();
    return { kind: 'group', text, retry: false };
  }

  // A missed item comes back, scored normally, after about `retryGap` other items.
  missed(item) {
    if (!this.retries.some((r) => r.text === item.text)) this.retries.push({ text: item.text, wait: this.retryGap + 1 });
  }
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
// Events (on(name, fn)): state, item, result, notice.
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
    if (['idle', 'feedback', 'feedback-playing'].indexOf(this.state) < 0) return;
    const pick = this.scheduler.next();
    this.item = { kind: pick.kind, text: pick.text, retry: pick.retry, doneAt: null, keys: [],
                  replays: 0, helped: false, stops: 0 };
    this._emit('item', this.item);
    this._play(false);
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
        if (unscored) { this._setState('feedback'); return; }
        it.doneAt = d.t;
        this._setState('answering');
      } else if (d.reason === 'stopped' && !unscored && it.stops < 3) {
        it.stops++;                       // a paddle touch (R2): play it again, it costs nothing
        this._emit('notice', 'stopped');
        this._play(false);
      } else if (d.reason === 'stopped') {
        this._setState(unscored ? 'feedback' : 'answering');
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
  async submit(answer, t) {
    const it = this.item;
    if (!it || this.state !== 'answering') return null;
    t = t != null ? t : this.clock.now();
    const s = score(it.text, answer);
    const ctx = this.context() || {};
    const first = it.keys.length ? it.keys[0].t : null;
    const attempt = {
      schema: SCHEMA, id: newId(), t: this.clock.wall(),
      device: ctx.device || null, lesson: ctx.lesson != null ? ctx.lesson : null, wpm: ctx.wpm != null ? ctx.wpm : null,
      farnsworth: ctx.farnsworth != null ? ctx.farnsworth : null,
      kind: it.kind, tier: null, source: 'typed', retry: it.retry,
      expected: s.expected, answer: s.answer, perfect: s.perfect,
      perWord: s.words, perChar: s.chars, extra: s.extra,
      latency: first != null && it.doneAt != null ? Math.round(first - it.doneAt) : null,
      total: it.doneAt != null ? Math.round(t - it.doneAt) : null,
      keystrokes: it.keys.map((k) => ({ k: k.k, t: it.doneAt != null ? Math.round(k.t - it.doneAt) : null })),
      corrections: 0, replays: it.replays, helped: it.helped
    };
    await this.store.add(attempt);
    this.mastery.apply(attempt);
    this.tally.items++;
    if (s.perfect) this.tally.perfect++;
    this.tally.chars += s.charCount;
    this.tally.charsRight += s.charsRight;
    this._emit('result', { attempt, score: s });
    if (s.perfect) {
      this._setState('idle');
    } else {
      this.scheduler.missed(it);       // decision 4c: it comes back after ~4 others, scored
      this._play(true);                // ... and is played once more now, unscored
    }
    return attempt;
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
      req.onsuccess = () => resolve(new IdbStore(req.result));
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
function lessonChars(kochlesson) {
  if (!kochlesson || !Array.isArray(kochlesson.characters)) return [];
  return kochlesson.characters.slice(0, Math.max(0, kochlesson.value | 0)).map((c) => String(c).toLowerCase());
}


return {
  VERSION, SCHEMA, RULES, KEYSTROKE_DAYS,
  tokenize, normalize, splitWords, isProsign, alignTokens, alignWords, score,
  Mastery, movesBoxes, median, mulberry32, Scheduler, Session, newId,
  MemoryStore, IdbStore, exportFile, parseExport, lessonChars
};
}));
