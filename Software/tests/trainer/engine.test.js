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

test('only the given characters are drawn; prosigns are not playable yet', () => {
  const chars = E.lessonChars({ value: 45, characters: 'mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-='.split('').concat(['<sk>', '+', '<as>']) });
  assert.equal(chars.length, 45);
  assert.equal(chars[42], '<sk>');
  const play = chars.filter(E.playable);
  assert.equal(play.length, 43);
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars: play, rng: E.mulberry32(2), size: 5 });
  for (let i = 0; i < 200; i++) for (const t of E.tokenize(s.next().text)) assert.ok(play.indexOf(t) >= 0);
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
