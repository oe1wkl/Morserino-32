// Tests for Software/Utilities/m32_trainer_engine.js (M3). Run:
//   node --test Software/tests/trainer/*.test.js
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const E = require(path.join(__dirname, '../../Utilities/m32_trainer_engine.js'));

// ---- helpers ---------------------------------------------------------------

function attempt(perChar, extra) {
  return Object.assign({ id: E.newId(), t: 0, source: 'typed', perChar }, extra || {});
}
const right = (ch) => ({ expected: ch, got: ch, ok: true });
const wrong = (ch, got) => ({ expected: ch, got: got === undefined ? 'x' : got, ok: false });

// A player whose plays end when the test says so.
function fakePlayer() {
  const p = { plays: [], stops: 0 };
  p.play = (text) => {
    const e = { text };
    e.done = new Promise((res) => { e.finish = (reason, t) => res({ reason, t }); });
    e.started = Promise.resolve({ t: 0 });
    p.plays.push(e);
    return e;
  };
  p.stop = () => { p.stops++; return Promise.resolve(); };
  p.last = () => p.plays[p.plays.length - 1];
  return p;
}

const tick = () => new Promise((r) => setImmediate(r));

function session(opts) {
  opts = opts || {};
  const player = fakePlayer();
  const store = new E.MemoryStore();
  const mastery = new E.Mastery();
  const scheduler = new E.Scheduler({ rng: E.mulberry32(7), mastery, chars: opts.chars || ['m', 'k', 'r'], size: opts.size || 3 });
  let now = 1000;
  const clock = { now: () => now, wall: () => 1.7e12 + now };
  const s = new E.Session({ player, store, mastery, scheduler, clock, helpLimit: opts.helpLimit,
                            context: () => ({ device: { protocol: '1.6' }, lesson: 3, wpm: 20 }) });
  return { s, player, store, mastery, scheduler, set: (t) => { now = t; } };
}

// ---- text ------------------------------------------------------------------

test('tokenize keeps prosigns whole, lower-cases, collapses whitespace', () => {
  assert.deepEqual(E.tokenize('  CQ<SK>  de\tK '), ['c', 'q', '<sk>', ' ', 'd', 'e', ' ', 'k']);
  assert.deepEqual(E.tokenize('a<b'), ['a', '<', 'b']);
  assert.deepEqual(E.tokenize('5nn <err> ä'), ['5', 'n', 'n', ' ', '<err>', ' ', 'ä']);
  // the other ways the M32 reads a prosign; <ar> and <bt> are the lesson's + and =
  assert.deepEqual(E.tokenize('[SK]\\kn<ar>[bt]'), ['<sk>', '<kn>', '+', '=']);
  assert.deepEqual(E.tokenize('\\x [s'), ['\\', 'x', ' ', '[', 's']);
  assert.equal(E.normalize(' Ab  C '), 'ab c');
  assert.deepEqual(E.splitWords('kmr  st'), [['k', 'm', 'r'], ['s', 't']]);
});

// ---- scoring ---------------------------------------------------------------

test('a perfect answer, case and spacing ignored', () => {
  const s = E.score('kmr', ' KMR ');
  assert.equal(s.perfect, true);
  assert.equal(s.charsRight, 3);
  assert.deepEqual(s.chars.map((c) => c.ok), [true, true, true]);
});

test('substitution, deletion and insertion inside a word', () => {
  let s = E.score('kmr', 'kmt');
  assert.deepEqual(s.chars, [right('k'), right('m'), wrong('r', 't')]);
  assert.equal(s.perfect, false);
  s = E.score('kmr', 'kr');
  assert.deepEqual(s.chars, [right('k'), wrong('m', null), right('r')]);
  s = E.score('kmr', 'kmmr');
  assert.deepEqual(s.chars.map((c) => c.ok), [true, true, true]);
  assert.equal(s.extra, 1);
  assert.equal(s.perfect, false, 'an extra character is not perfect');
});

test('an empty answer misses every character', () => {
  const s = E.score('ab', '');
  assert.deepEqual(s.chars, [wrong('a', null), wrong('b', null)]);
  assert.equal(s.wordsRight, 0);
});

test('word alignment pairs similar words and handles a dropped word', () => {
  const s = E.score('cq de oe1wkl', 'cq oe1wkk');
  assert.deepEqual(s.words.map((w) => [w.expected, w.got, w.ok]),
                   [['cq', 'cq', true], ['de', null, false], ['oe1wkl', 'oe1wkk', false]]);
  assert.equal(s.charCount, 2 + 2 + 6, 'one entry per expected character');
  assert.deepEqual(s.chars.slice(2, 4), [wrong('d', null), wrong('e', null)]);
  assert.deepEqual(s.chars[9], wrong('l', 'k'));
});

test('an inserted word counts as extra, not as a character attempt', () => {
  const s = E.score('tu 73', 'tu ee 73');
  assert.equal(s.wordsRight, 2);
  assert.equal(s.charCount, 4);
  assert.equal(s.extra, 2);
  assert.equal(s.perfect, false);
});

test('a missing space costs one word, the characters survive', () => {
  const s = E.score('kmr st', 'kmrst');
  assert.equal(s.charCount, 5);
  assert.equal(s.wordsRight, 0);
  // kmr paired with kmrst (3 right, 2 extra), st dropped -- or the reverse; either way 5 entries
  assert.ok(s.charsRight >= 2);
});

test('a long word with several errors is still paired, not dropped and re-added', () => {
  // three wrong of six: 0.5 against 1 + 1 for dropping it and adding the answer's word
  const s = E.score('bcdefg', 'xcdyfz');
  assert.deepEqual(s.words.map((w) => [w.expected, w.got]), [['bcdefg', 'xcdyfz']]);
  assert.equal(s.charsRight, 3);
  assert.equal(s.extra, 0);
});

test('prosigns score as one character', () => {
  const s = E.score('tu <sk>', 'tu <kn>');
  assert.deepEqual(s.chars[2], wrong('<sk>', '<kn>'));
  assert.equal(s.charCount, 3);
});

test('a prosign typed in any M32 spelling is right', () => {
  for (const a of ['m<sk>k', 'M[SK]K', 'm\\skk']) assert.ok(E.score('m<sk>k', a).perfect, a);
  assert.ok(E.score('k+', 'k<ar>').perfect);
  assert.ok(!E.score('m<sk>k', 'mskk').perfect);
});

// ---- mastery ---------------------------------------------------------------

test('promotion needs 3 attempts since the change and 90 % of the last 10', () => {
  const m = new E.Mastery();
  m.apply(attempt([right('k')]));
  m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 1);
  m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 2, 'third right attempt promotes');
  m.apply(attempt([right('k')])); m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 2);
  m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 3);
});

test('a miss drops two boxes, not below 1, and blocks promotion while accuracy is low', () => {
  const m = new E.Mastery();
  for (let i = 0; i < 9; i++) m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 4);
  m.apply(attempt([wrong('k')]));
  assert.equal(m.box('k'), 2);
  m.apply(attempt([wrong('k')]));
  assert.equal(m.box('k'), 1);
  m.apply(attempt([wrong('k')]));
  assert.equal(m.box('k'), 1);
  // the last 10 hold misses: 3 rights since the change are not enough
  for (let i = 0; i < 3; i++) m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 1, '7/10 is under 90 %');
  for (let i = 0; i < 5; i++) m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 1, 'still a miss and 8 rights in the window');
  m.apply(attempt([right('k')]));
  assert.equal(m.box('k'), 2, 'one miss and nine rights: 90 %');
});

test('box 5 is the top', () => {
  const m = new E.Mastery();
  for (let i = 0; i < 30; i++) m.apply(attempt([right('a')]));
  assert.equal(m.box('a'), 5);
});

test('helped, keyed, corrected and unscored attempts move no box', () => {
  const m = new E.Mastery();
  for (let i = 0; i < 5; i++) m.apply(attempt([right('k')], { helped: true }));
  assert.equal(m.box('k'), 1);
  assert.equal(m.row('k').attempts, 5, 'but helped ones are in the stats');
  for (let i = 0; i < 5; i++) m.apply(attempt([right('m')], { source: 'keyed' }));
  assert.equal(m.box('m'), 1);
  assert.equal(m.row('m').attempts, 0, 'keyed attempts have their own tally');
  assert.deepEqual(m.row('m').keyed, { attempts: 5, correct: 5 });
  for (let i = 0; i < 5; i++) m.apply(attempt([right('r')], { corrections: 1 }));
  assert.equal(m.box('r'), 1);
  for (let i = 0; i < 5; i++) m.apply(attempt([right('s')], { scored: false }));
  assert.equal(m.row('s').attempts, 0);
  // a helped miss does not drop
  for (let i = 0; i < 3; i++) m.apply(attempt([right('u')]));
  m.apply(attempt([wrong('u')], { helped: true }));
  assert.equal(m.box('u'), 2);
});

test('the model is a replay of the log, in time order', () => {
  const log = [];
  for (let i = 0; i < 9; i++) log.push(attempt([right('k')], { t: 10 + i }));
  const m1 = attempt([wrong('k')], { t: 5 }), m2 = attempt([wrong('k')], { t: 6 });
  log.push(m1, m2);                             // stored last, but older than the rest
  assert.equal(E.Mastery.fromAttempts(log).box('k'), 2, 'two misses, then nine rights: 9/10');
  m1.t = 100; m2.t = 101;
  assert.equal(E.Mastery.fromAttempts(log).box('k'), 1, 'nine rights (box 4), then two misses');
});

test('confusions, accuracy, median latency', () => {
  const m = new E.Mastery();
  m.apply(attempt([wrong('k', 'r')], { latency: 300 }));
  m.apply(attempt([wrong('k', 'r')], { latency: 500 }));
  m.apply(attempt([wrong('k', null), right('m')], { latency: 400 }));
  assert.deepEqual(m.confusionList(), [{ expected: 'k', got: 'r', count: 2 }], 'a missing character is no confusion');
  assert.equal(m.row('k').accuracy, 0);
  assert.equal(m.row('k').medianLatency, 400);
  assert.equal(m.row('z').attempts, 0);
});

test('lesson advance is offered when every character is in box 3 or more', () => {
  const m = new E.Mastery();
  for (let i = 0; i < 6; i++) m.apply(attempt([right('k'), right('m')]));
  assert.equal(m.readyToAdvance(['k', 'm']), true);
  assert.equal(m.readyToAdvance(['k', 'm', 'r']), false);
  m.apply(attempt([wrong('k')]));
  assert.equal(m.readyToAdvance(['k', 'm']), false, 'withdrawn when one drops back');
  const n = new E.Mastery();
  for (let i = 0; i < 3; i++) n.apply(attempt([right('k')]));
  assert.equal(n.box('k'), 2);
  assert.equal(n.readyToAdvance(['k']), false, 'box 2 is not enough');
});

// ---- scheduling ------------------------------------------------------------

test('draw weight is 6 - box, the newest character doubled until box 2', () => {
  const m = new E.Mastery();
  const s = new E.Scheduler({ mastery: m, chars: ['m', 'k', 'r'], rng: E.mulberry32(1) });
  assert.equal(s.weight('m'), 5);
  assert.equal(s.weight('r'), 10);
  for (let i = 0; i < 3; i++) m.apply(attempt([right('r')]));
  assert.equal(s.weight('r'), 4);
  for (let i = 0; i < 12; i++) m.apply(attempt([right('m')]));
  assert.equal(s.weight('m'), 1);
});

test('drawn characters follow the weights', () => {
  const m = new E.Mastery();
  for (let i = 0; i < 12; i++) m.apply(attempt([right('m')]));      // box 5, weight 1
  const s = new E.Scheduler({ mastery: m, chars: ['m', 'k'], rng: E.mulberry32(3), size: 1 });
  const n = { m: 0, k: 0 };
  for (let i = 0; i < 6000; i++) n[s.pickChar()]++;
  // k is the newest, box 1: 5 * 2 = 10 against 1
  assert.ok(Math.abs(n.k / n.m - 10) < 1.5, JSON.stringify(n));
});

test('mixed group size grows with the average box', () => {
  const m = new E.Mastery();
  const s = new E.Scheduler({ mastery: m, chars: ['m', 'k'], rng: E.mulberry32(5) });
  for (let i = 0; i < 20; i++) assert.equal(s.groupSize(), 1);
  for (let i = 0; i < 12; i++) m.apply(attempt([right('m'), right('k')]));
  const sizes = new Set();
  for (let i = 0; i < 50; i++) sizes.add(s.groupSize());
  assert.deepEqual([...sizes].sort(), [4, 5]);
  s.size = 2;
  assert.equal(s.groupSize(), 2, 'a pinned size wins');
  assert.equal(s.next().text.length, 2);
});

test('a missed item returns after about four others', () => {
  const m = new E.Mastery();
  const s = new E.Scheduler({ mastery: m, chars: ['m', 'k', 'r'], rng: E.mulberry32(9), size: 3 });
  s.missed({ text: 'zzz' });
  const seq = [];
  for (let i = 0; i < 7; i++) seq.push(s.next());
  assert.equal(seq.findIndex((x) => x.text === 'zzz'), 4);
  assert.equal(seq[4].retry, true);
  assert.equal(seq.filter((x) => x.text === 'zzz').length, 1);
});

test('only the given characters are drawn, prosigns included and kept whole', () => {
  const chars = E.lessonChars({ value: 45, characters: 'mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-='.split('').concat(['<sk>', '+', '<as>']) });
  assert.equal(chars.length, 45);
  assert.equal(chars[42], '<sk>');
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars, rng: E.mulberry32(2), size: 5 });
  let prosigns = 0;
  for (let i = 0; i < 200; i++) {
    const toks = E.tokenize(s.next().text);
    assert.equal(toks.length, 5);                       // a prosign is one of the five
    for (const t of toks) { assert.ok(chars.indexOf(t) >= 0); if (E.isProsign(t)) prosigns++; }
  }
  assert.ok(prosigns > 0);
  assert.deepEqual(E.lessonChars({ value: 2, characters: ['M', 'K', 'R'] }), ['m', 'k']);
  assert.deepEqual(E.lessonChars(null), []);
});

// ---- the session -------------------------------------------------------------

test('play, answer right, log it: latency and total from done', async () => {
  const { s, player, store, set } = session();
  const states = [];
  s.on('state', (x) => states.push(x));
  s.next();
  assert.equal(s.state, 'playing');
  const text = player.last().text;
  assert.equal(text.length, 3);
  set(1500); s.key(text[0]);                      // typing while it sounds is fine
  player.last().finish('end', 1600);
  await tick();
  assert.equal(s.state, 'answering');
  set(1700); s.key(text[1]); s.key(text[2]);
  set(1900);
  const a = await s.submit(text.toUpperCase());
  assert.equal(a.perfect, true);
  assert.equal(a.latency, -100, 'first key before done: negative, kept as is');
  assert.equal(a.total, 300);
  assert.deepEqual(a.keystrokes.map((k) => k.t), [-100, 100, 100]);
  assert.equal(a.lesson, 3);
  assert.equal(a.wpm, 20);
  assert.equal(s.state, 'idle');
  assert.equal((await store.all()).length, 1);
  assert.deepEqual(states, ['playing', 'answering', 'idle']);
  assert.deepEqual(s.tally, { items: 1, perfect: 1, chars: 3, charsRight: 3 });
});

test('submit before done is refused', async () => {
  const { s } = session();
  s.next();
  assert.equal(await s.submit('abc'), null);
});

test('a miss replays the item unscored and schedules it again', async () => {
  const { s, player, mastery } = session();
  s.next();
  const text = player.last().text;
  player.last().finish('end', 1100);
  await tick();
  const a = await s.submit('');
  assert.equal(a.perfect, false);
  assert.equal(s.state, 'feedback-playing');
  assert.equal(player.plays.length, 2);
  assert.equal(player.last().text, text);
  player.last().finish('end', 2000);
  await tick();
  assert.equal(s.state, 'feedback');
  assert.equal(mastery.row(text[0]).attempts >= 1, true);
  for (let i = 0; i < 4; i++) {
    s.next(); player.last().finish('end', 3000 + i); await tick();
    await s.submit(player.last().text);
  }
  s.next();
  assert.equal(player.last().text, text, 'back after four others');
  assert.equal(s.item.retry, true);
});

test('next during the feedback replay skips to the new item', async () => {
  const { s, player } = session();
  s.next(); player.last().finish('end', 1); await tick();
  await s.submit('');
  assert.equal(s.state, 'feedback-playing');
  s.next();
  assert.equal(s.state, 'playing');
  player.plays[1].finish('replaced', 5);           // the old replay's done arrives late
  await tick();
  assert.equal(s.state, 'playing');
  player.last().finish('end', 6); await tick();
  assert.equal(s.state, 'answering');
});

test('next is ignored while answering', async () => {
  const { s, player } = session();
  s.next(); player.last().finish('end', 1); await tick();
  s.next();
  assert.equal(player.plays.length, 1);
});

test('a help replay is counted, limited, and marks the attempt helped', async () => {
  const { s, player, mastery } = session({ helpLimit: 1 });
  s.next();
  const text = player.last().text;
  player.last().finish('end', 1); await tick();
  assert.equal(s.replay(), true);
  assert.equal(s.state, 'playing');
  player.last().finish('end', 2); await tick();
  assert.equal(s.replay(), false, 'limit reached');
  const a = await s.submit(text);
  assert.equal(a.helped, true);
  assert.equal(a.replays, 1);
  assert.equal(mastery.box(text[0]), 1);
});

test('a paddle stop plays the item again; done of the last play counts', async () => {
  const { s, player } = session();
  const notes = [];
  s.on('notice', (n) => notes.push(n));
  s.next();
  player.last().finish('stopped', 1); await tick();
  assert.deepEqual(notes, ['stopped']);
  assert.equal(player.plays.length, 2);
  assert.equal(s.state, 'playing');
  player.last().finish('end', 50); await tick();
  assert.equal(s.item.doneAt, 50);
  assert.equal(s.state, 'answering');
});

test('after three paddle stops the learner may answer anyway', async () => {
  const { s, player } = session();
  s.next();
  for (let i = 0; i < 4; i++) { player.last().finish('stopped', i); await tick(); }
  assert.equal(player.plays.length, 4);
  assert.equal(s.state, 'answering');
});

test('a lost link pauses; resume goes back to idle', async () => {
  const { s, player } = session();
  const notes = [];
  s.on('notice', (n) => notes.push(n));
  s.next();
  player.last().finish('lost', 1); await tick();
  assert.equal(s.state, 'paused');
  assert.deepEqual(notes, ['lost']);
  s.resume();
  assert.equal(s.state, 'idle');
  s.next();
  assert.equal(s.state, 'playing');
});

test('pause stops the play and ignores its done', async () => {
  const { s, player } = session();
  s.next();
  s.pause();
  assert.equal(player.stops, 1);
  player.last().finish('stopped', 1); await tick();
  assert.equal(s.state, 'paused');
  assert.equal(player.plays.length, 1);
});

// ---- storage ---------------------------------------------------------------

test('export and import merge by id', async () => {
  const a = new E.MemoryStore(), b = new E.MemoryStore();
  const x = attempt([right('k')], { t: 1 }), y = attempt([right('m')], { t: 2 });
  await a.add(x); await a.add(y);
  await b.add(y);
  const file = JSON.stringify(E.exportFile(await a.all(), Date.UTC(2026, 10, 1)));
  const { attempts, rejected } = E.parseExport(file);
  assert.equal(rejected, 0);
  assert.deepEqual(await b.importMany(attempts), { added: 1, skipped: 1 });
  assert.deepEqual((await b.all()).map((q) => q.id), [x.id, y.id]);
});

test('parseExport refuses foreign and newer files and drops broken records', () => {
  assert.throws(() => E.parseExport('nope'), /not JSON/);
  assert.throws(() => E.parseExport('{"attempts":[]}'), /not an M32 Trainer export/);
  assert.throws(() => E.parseExport({ app: 'M32 Trainer', schema: 99, attempts: [] }), /newer/);
  const r = E.parseExport({ app: 'M32 Trainer', schema: 1, attempts: [{ id: 'a', t: 1, perChar: [] }, { id: 2 }] });
  assert.equal(r.attempts.length, 1);
  assert.equal(r.rejected, 1);
});

test('raw keystrokes are pruned before a date, the rest stays', async () => {
  const st = new E.MemoryStore();
  await st.add(attempt([right('k')], { t: 1, keystrokes: [{ k: 'k', t: 5 }], latency: 5 }));
  await st.add(attempt([right('k')], { t: 100, keystrokes: [{ k: 'k', t: 5 }] }));
  assert.equal(await st.pruneKeystrokes(50), 1);
  const all = await st.all();
  assert.equal(all[0].keystrokes, undefined);
  assert.equal(all[0].latency, 5);
  assert.equal(all[1].keystrokes.length, 1);
});

test('M4: the Practice Set reply becomes tokens as GET kochlesson shows them, once each', () => {
  assert.deepEqual(E.practiceChars({ characters: 'kmrSéHkAU' }), ['k', 'm', 'r', '<as>', 'é', 'ch', '<ka>']);   // * has no Morse code
  assert.deepEqual(E.practiceChars({ characters: '' }), []);
  assert.deepEqual(E.practiceChars(null), []);
  assert.deepEqual(E.practiceChars({ characters: 'NKEBR' }), ['<kn>', '<sk>', '<ve>', '<bk>']);         // nor has <err> (not in CWchars[])
});

test('M4: Farnsworth spacing stretches character and word spaces alike, within the M32\'s ranges', () => {
  // 20/12: 52.3 dits of spacing in PARIS -> character spaces of 8 (3/19 of it), the word space 20 (the rest)
  assert.deepEqual(E.farnsworthSpacing(20, 12), { charSpace: 8, wordSpace: 20, effective: 50 * 20 / (31 + 32 + 20) });
  for (let wpm = 5; wpm <= 40; wpm++)
    for (let eff = 4; eff < wpm; eff++) {
      const s = E.farnsworthSpacing(wpm, eff);
      assert.ok(s.charSpace >= 3 && s.charSpace <= 45, wpm + '/' + eff);
      assert.ok(s.wordSpace >= s.charSpace + 4 && s.wordSpace <= 105, wpm + '/' + eff);
      assert.ok(s.effective <= wpm + 1e-9);
      // whole dits only: PARIS is off by half a dit at most (unless a range limit is reached)
      if (s.charSpace < 45) assert.ok(Math.abs(31 + 4 * s.charSpace + s.wordSpace - 50 * wpm / eff) <= 0.5, wpm + '/' + eff + ' -> ' + s.effective);
    }
  assert.equal(E.farnsworthSpacing(20, 20), null);           // not below: no Farnsworth
  assert.equal(E.farnsworthSpacing(20, 25), null);
  assert.equal(E.farnsworthSpacing(20, 0), null);
  assert.equal(E.farnsworthSpacing(null, 10), null);
  assert.ok(E.farnsworthSpacing(40, 4).effective > 6);        // at the limit: faster than asked, the page says so
});

test('M4: without an order (Practice Set) no character counts double', () => {
  const m = new E.Mastery();
  const ordered = new E.Scheduler({ mastery: m, chars: ['k', 'm', 'r'] });
  const plain = new E.Scheduler({ mastery: m, chars: ['k', 'm', 'r'], newest: false });
  assert.equal(ordered.weight('r'), 2 * ordered.weight('k'));
  assert.equal(plain.weight('r'), plain.weight('k'));
  ordered.setChars(['k', 'm', 'r'], false);
  assert.equal(ordered.weight('r'), ordered.weight('k'));
  ordered.setChars(['k', 'm', 'r']);                         // newest left as it was
  assert.equal(ordered.newest, false);
});

test('M4: LICW (BC2 = 44 characters) from GET kochlesson: every character is one token the trainer can use', () => {
  // licwAllKochChars (MorsePreferences.h), as GET kochlesson lists it (cleanUpProSigns())
  const raw = 'reatinpgslcdhofuwbkmy59,qxv73?+K=16.zj/28B40-ASNE@:';
  const shown = { K: '<sk>', B: '<bk>', A: '<ka>', S: '<as>', N: '<kn>', E: '<ve>' };
  const characters = [...raw].map((c) => shown[c] || c);
  const act = E.lessonChars({ value: 44, minimum: 1, maximum: characters.length, characters });
  assert.equal(act.length, 44);
  assert.equal(new Set(act).size, 44);
  for (const t of act) assert.deepEqual(E.tokenize(t), [t], t);       // a prosign stays one character
  assert.ok(act.includes('<sk>') && act.includes('<bk>') && act.includes('+') && act.includes('='));
  // groups drawn from it use only those
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars: act, rng: E.mulberry32(3) });
  for (let i = 0; i < 50; i++) for (const t of E.tokenize(s.next().text)) assert.ok(act.includes(t), t);
});

// Bench 2026-11-06: "\sk" at the end of player.txt put a "\" into Custom Chars (fixed in getWord());
// the M32 skips a character it has no code for, so the trainer must not ask for it either.
test('M4: characters with no Morse code are left out of the set, and reported', () => {
  const dropped = [];
  const set = E.lessonChars({ value: 6, characters: ['g', 'r', 'ü', 's', 'Å', '\\', 'x'] }, dropped);
  assert.deepEqual(set, ['g', 'r', 'ü', 's', 'å']);
  assert.deepEqual(dropped, ['\\']);
  const d2 = [];
  assert.deepEqual(E.practiceChars({ characters: 'k\\U(ø' }, d2), ['k', '(', 'ø']);
  assert.deepEqual(d2, ['\\', '*']);
  for (const t of ['a', '9', '@', '<sk>', '<bk>', 'ä', 'ch', 'ñ', '+', '=']) assert.ok(E.hasMorse(t), t);
  for (const t of ['\\', '*', '<err>', '!', '"', 'ß']) assert.ok(!E.hasMorse(t), t);
});

// ---- M4b: call signs and prefixes --------------------------------------------

const corpusJson = require(path.join(__dirname, '../../Utilities/m32_trainer_corpus.json'));

test('M4b: Call signs and Prefixes come from the firmware generators with the page settings, all characters', () => {
  const corpus = new E.Corpus(corpusJson);
  const sch = new E.Scheduler({ rng: E.mulberry32(3), mastery: new E.Mastery(), chars: ['m', 'k'], corpus, mode: 'calls',
                                callOpts: { maxLength: 2, region: 1, commonOnly: true } });
  for (let i = 0; i < 30; i++) {
    const it = sch.next();
    assert.equal(it.kind, 'call');
    assert.match(it.text, /^[a-z0-9]{1,2}[0-9][a-z]{1,3}$/, it.text);   // prefix <= 2 letters/digits + area digit
    assert.ok(it.text.length <= 6, it.text);
  }
  sch.callOpts = { maxLength: 0, region: 7, commonOnly: true };
  for (let i = 0; i < 30; i++) assert.match(sch.next().text, /^(vk|zl)[0-9][a-z]{2,3}$/);
  sch.mode = 'prefixes';
  sch.callOpts = { maxLength: 1, region: 1, commonOnly: true };          // Length Calls does not apply to prefixes
  for (let i = 0; i < 30; i++) {
    const it = sch.next();
    assert.equal(it.kind, 'prefix');
    assert.match(it.text, /^[a-z0-9]+$/, it.text);
  }
  // no lesson characters at all: calls still come
  sch.setChars([]);
  sch.mode = 'calls';
  assert.ok(sch.next().text.length >= 3);
});

test('M4b: a missed call comes back, and survives a lesson change; it never mixes with groups', () => {
  const corpus = new E.Corpus(corpusJson);
  const sch = new E.Scheduler({ rng: E.mulberry32(9), mastery: new E.Mastery(), chars: ['m', 'k', 'r'], corpus, mode: 'calls', retryGap: 2 });
  const miss = sch.next();
  sch.missed(miss);
  sch.setChars(['m', 'k']);                       // the call's characters are not in the lesson: kept anyway
  assert.equal(sch.retries.length, 1);
  sch.mode = 'groups'; sch.size = 2;
  for (let i = 0; i < 5; i++) assert.equal(sch.next().kind, 'group');
  sch.mode = 'calls';
  const back = sch.next();
  assert.equal(back.text, miss.text);
  assert.equal(back.retry, true);
  assert.equal(back.kind, 'call');
});

test('M4b: calls and prefixes move no box and are tallied apart (also when replayed from the log)', () => {
  const m = new E.Mastery();
  m.apply(attempt([right('d'), right('l'), right('1'), wrong('a', 'e'), right('b')], { kind: 'call', expected: 'dl1ab', perfect: false }));
  m.apply(attempt([right('o'), right('e'), right('3'), right('x'), right('y'), right('z')], { kind: 'call', expected: 'oe3xyz', perfect: true }));
  m.apply(attempt([right('o'), right('e'), right('3')], { kind: 'prefix', expected: 'oe3', perfect: true, source: 'keyed' }));
  assert.equal(m.chars.size, 0, 'no character model touched');
  assert.equal(m.confusionList().length, 0);
  const c = E.callSummary(m.calls.call);
  assert.equal(c.items, 2); assert.equal(c.perfect, 1);
  assert.deepEqual(c.byLength, [{ length: 5, items: 1, perfect: 0 }, { length: 6, items: 1, perfect: 1 }]);
  assert.deepEqual(c.missed, [{ ch: 'a', missed: 1, attempts: 1 }]);
  assert.equal(E.callSummary(m.calls.prefix).items, 1);
});

test('M4b: the session counts calls in their own tally', async () => {
  const player = fakePlayer();
  const store = new E.MemoryStore();
  const mastery = new E.Mastery();
  const scheduler = new E.Scheduler({ rng: E.mulberry32(4), mastery, chars: ['m'], corpus: new E.Corpus(corpusJson), mode: 'calls' });
  let now = 1000;
  const s = new E.Session({ player, store, mastery, scheduler, clock: { now: () => now, wall: () => now }, context: () => ({}) });
  s.next();
  const text = player.last().text;
  player.last().finish('end', 1100);
  await tick();
  now = 1500;
  const a = await s.submit(text);
  assert.equal(a.kind, 'call');
  assert.equal(s.callTally.items, 1);
  assert.equal(s.callTally.perfect, 1);
  assert.equal(s.tally.items, 0);
  assert.equal(s.keyedTally.items, 0);
  assert.equal(mastery.chars.size, 0);
});

test('"All characters" is the M32\'s full standard set, in its Koch order (morserinoKochChars)', () => {
  const fs = require('node:fs');
  const h = fs.readFileSync(path.join(__dirname, '../../src/Version 6 and newer/MorsePreferences.h'), 'utf8');
  const raw = /morserinoKochChars\s*=\s*"([^"]+)"/.exec(h)[1];
  const PROS = { K: '<sk>', S: '<as>', N: '<kn>', A: '<ka>', E: '<ve>', B: '<bk>' };
  assert.deepEqual(E.ALL_CHARS, [...raw].map((c) => PROS[c] || c));
  assert.equal(E.ALL_CHARS.length, 51);
  assert.ok(E.ALL_CHARS.every(E.hasMorse));
});

// ---- M5: the Echo Trainer log -------------------------------------------------------

test('M5: an Echo result becomes an attempt; only mode "echo" (not learn / preview)', () => {
  const a = E.echoAttempt({ event: 'result', mode: 'echo', prompt: 'cq', answer: 'cq', ok: true, repeat: 0, wpm: 18, koch: 12,
                            latency: 820, duration: 2100 }, { t: 5 });
  assert.equal(a.kind, 'echo'); assert.equal(a.source, 'echo');
  assert.equal(a.ok, true); assert.equal(a.perfect, true);
  assert.equal(a.lesson, 12); assert.equal(a.wpm, 18); assert.equal(a.latency, 820); assert.equal(a.duration, 2100);
  assert.equal(a.t, 5);
  assert.deepEqual(a.perChar.map((c) => c.ok), [true, true]);
  assert.equal(E.echoAttempt({ event: 'result', mode: 'learn', prompt: 'k', answer: 'k', ok: true }), null);
  assert.equal(E.echoAttempt({ event: 'result', mode: 'preview', prompt: 'k', answer: 'k', ok: true }), null);
  assert.equal(E.echoAttempt({ event: 'done', mode: 'echo' }), null);
  const t = E.echoAttempt({ event: 'result', mode: 'echo', prompt: '<sk>', answer: '', ok: false, repeat: 1, wpm: 18, timeout: true });
  assert.equal(t.timeout, true); assert.equal(t.latency, null); assert.deepEqual(t.perChar, []);
  assert.equal(t.expected, '<sk>');
});

test('M5: Echo results have their own tally and move no box', () => {
  const m = new E.Mastery();
  const ev = (prompt, answer, o) => E.echoAttempt(Object.assign({ event: 'result', mode: 'echo', prompt, answer, ok: prompt === answer, repeat: 0, wpm: 20 }, o));
  m.apply(ev('cq', 'cq', { latency: 600, duration: 1000 }));
  m.apply(ev('test', 'tist', { latency: 1000, duration: 2000 }));
  m.apply(ev('test', 'test', { repeat: 1, latency: 800, duration: 3000 }));
  m.apply(ev('de', '', { timeout: true }));
  assert.equal(m.chars.size, 0, 'no character model touched');
  assert.equal(m.confusionList().length, 0);
  const e = E.echoSummary(m.echo);
  assert.equal(e.items, 4); assert.equal(e.ok, 2); assert.equal(e.timeouts, 1);
  assert.equal(e.firstTry, 3); assert.equal(e.firstOk, 1);
  assert.equal(e.medianLatency, 800);                 // 600, 1000, 800 (the timeout has none)
  assert.equal(e.medianDuration, 2000);               // only words keyed right: 1000, 3000
  assert.deepEqual(e.missed, [{ ch: 'e', missed: 1, attempts: 2 }]);   // the timeout counts no character
  // replayed from the log the same way
  const again = E.Mastery.fromAttempts([ev('cq', 'cq', { latency: 1 })].map((a, i) => Object.assign(a, { t: i })));
  assert.equal(again.echo.items, 1);
});
