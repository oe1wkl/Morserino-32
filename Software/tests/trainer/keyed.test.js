// Tests for the keyed answer in Software/Utilities/m32_trainer_engine.js (M3c). Run:
//   node --test Software/tests/trainer/*.test.js
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('node:path');
const E = require(path.join(__dirname, '../../Utilities/m32_trainer_engine.js'));

function answer(target, opts) { const k = new E.KeyedAnswer(target, opts); k.start(0); return k; }
function keyed(target, text, opts) { const k = answer(target, opts); k.feed(text, 0); return k; }

test('reads the echo: case, prosigns, word spaces', () => {
  assert.equal(keyed('km', 'km').text, 'km');
  assert.equal(keyed('hw cpy', 'HW CPY ').text, 'hw cpy ');
  assert.equal(keyed('hw', '  hw').text, 'hw', 'no space before the first character');
  assert.equal(keyed('hw cpy', 'hw   cpy').text, 'hw cpy', 'one word space, however many arrive');
  assert.deepEqual(keyed('a<sk>', 'a<SK>').tokens, ['a', '<sk>']);
  assert.deepEqual(keyed('r+', 'r<ar>').tokens, ['r', '+'], '<ar> is the character +');
});

test('a prosign cut in two by the transport stays one token', () => {
  const k = answer('a<sk>');
  k.feed('a<s', 1);
  assert.deepEqual(k.tokens, ['a']);
  k.feed('k>', 2);
  assert.deepEqual(k.tokens, ['a', '<sk>']);
  const e = keyed('hw cpy', 'hw cpy');
  e.feed('<e', 1);
  e.feed('RR>', 2);
  assert.equal(e.text, 'hw ');
  assert.equal(e.corrections, 1);
});

test('a < that opens no prosign is a character', () => {
  assert.deepEqual(keyed('a', '<a b>').tokens, ['<', 'a', ' ', 'b', '>']);
  assert.deepEqual(keyed('a', '<abcdefg').tokens, ['<', 'a', 'b', 'c', 'd', 'e', 'f', 'g']);
});

test('the first <err> or * deletes the last word, a second one right after starts over', () => {
  const k = answer('hw cpy om');
  k.feed('hw cp', 1);
  k.feed('<err>', 2);
  assert.equal(k.text, 'hw ');
  k.feed('cpy o*', 3);
  assert.equal(k.text, 'hw cpy ');
  assert.equal(k.corrections, 2);
  k.feed('*', 4);
  assert.equal(k.text, '', 'right after: all of it');
  assert.equal(k.corrections, 3);
});

test('a word space between two corrections still counts as right after', () => {
  const k = keyed('hw cpy', 'hw cp<err> ');
  k.feed('<err>', 1);
  assert.equal(k.text, '');
});

test('a character in between makes the next correction a first one again', () => {
  const k = keyed('hw cpy', 'hw cp<err>c');
  k.feed('<err>', 1);
  assert.equal(k.text, 'hw ');
});

test('a correction after a word space deletes the word before it', () => {
  assert.equal(keyed('hw cpy', 'hw cpy <err>').text, 'hw ');
  assert.equal(keyed('hw', 'hw <err>').text, '');
});

test('= and <bt> finish, unless the target still has one to come', () => {
  for (const sign of ['=', '<bt>', '<BT>']) {
    const k = keyed('hw', 'hw' + sign);
    assert.equal(k.check(0), 'sign');
    assert.equal(k.text, 'hw', 'the sign is not part of the answer');
  }
  const q = keyed('name tom = qth wien', 'name tom =');
  assert.equal(q.check(0), null);
  assert.equal(q.text, 'name tom =');
  q.feed(' qth wien =', 1);
  assert.equal(q.check(1), 'sign', 'the second one is not in the target');
  assert.equal(keyed('hw', '=').check(0), 'sign', 'with nothing keyed too');
});

test('nothing keyed: the start timeout, not the idle one', () => {
  const k = answer('kmr');
  assert.equal(k.check(14999), null);
  assert.equal(k.check(15000), 'start');
  assert.equal(new E.KeyedAnswer('kmr').check(99999), null, 'before the sound has ended it does not run');
});

test('starting again moves the timeouts on', () => {
  const k = answer('kmr');
  k.start(15000);
  assert.equal(k.check(29999), null);
  assert.equal(k.check(30000), 'start');
});

test('an answer deleted entirely waits like one not begun', () => {
  const k = keyed('kmr', 'km<err>');
  assert.equal(k.check(14999), null);
  assert.equal(k.check(15000), 'start');
});

test('early finish counts characters, not words', () => {
  const g = keyed('kmrsu', 'km');          // a pause inside a group must not end it
  assert.equal(g.check(1500), null);
  assert.equal(g.check(3999), null);
  assert.equal(g.check(4000), 'idle');
  const all = keyed('kmr', 'kmr');
  assert.equal(all.check(1499), null);
  assert.equal(all.check(1500), 'early');
  const p = keyed('hw cpy', 'h w c');      // two "words" keyed, three of five characters
  assert.equal(p.check(1500), null);
  assert.equal(keyed('hw cpy', 'hw cpy x').check(1500), 'early', 'too many is enough too');
});

test('the word space the decoder sends after the answer starts no waiting of its own', () => {
  const k = answer('kmr');
  k.feed('kmr', 100);
  k.feed(' ', 1500);
  assert.equal(k.check(1600), 'early');
  const w = answer('kmrsu');
  w.feed('km', 100);
  w.feed(' ', 3000);
  assert.equal(w.check(4100), 'idle');
});

test('the timeouts can be set', () => {
  const k = keyed('kmrsu', 'km', { idleMs: 2000 });
  assert.equal(k.check(1999), null);
  assert.equal(k.check(2000), 'idle');
  assert.equal(keyed('km', 'km', { graceMs: 500 }).check(500), 'early');
  assert.equal(answer('km', { startMs: 900 }).check(900), 'start');
});

test('Lenient: spaces do not count in a group or a single word', () => {
  const k = keyed('kmr', 'k m r ');
  assert.equal(E.score('kmr', k.answer('lenient')).perfect, true);
  assert.equal(k.answer('lenient'), 'kmr');
});

test('Strict: a break inside a group or a word is an error', () => {
  const k = keyed('kmr', 'k mr');
  assert.equal(E.score('kmr', k.answer('strict')).perfect, false);
  assert.equal(E.score('kmr', keyed('kmr', 'kmr ').answer('strict')).perfect, true, 'a trailing space is nothing');
});

test('a phrase keeps its word breaks, Strict and Lenient', () => {
  for (const sp of ['strict', 'lenient']) {
    assert.equal(E.score('hw cpy', keyed('hw cpy', 'hw cpy ').answer(sp)).perfect, true);
    assert.equal(E.score('hw cpy', keyed('hw cpy', 'hwcpy').answer(sp)).perfect, false, 'run together');
    assert.equal(E.score('hw cpy', keyed('hw cpy', 'h w cpy').answer(sp)).perfect, false, 'a break inside a word');
  }
});

// ---- in a session ------------------------------------------------------------

function session() {
  const player = { plays: [] };
  player.play = (text) => {
    const e = { text };
    e.done = new Promise((res) => { e.finish = (reason, t) => res({ reason, t }); });
    e.started = Promise.resolve({ t: 0 });
    player.plays.push(e);
    return e;
  };
  player.stop = () => Promise.resolve();
  const store = new E.MemoryStore(), mastery = new E.Mastery();
  const scheduler = new E.Scheduler({ mastery, chars: ['k', 'm', 'r'], rng: E.mulberry32(1), size: 3 });
  const s = new E.Session({ player, store, mastery, scheduler, clock: { now: () => 0, wall: () => 0 } });
  return { s, player, store, mastery };
}
const tick = () => new Promise((r) => setImmediate(r));
async function toAnswer(c) {
  c.s.next();
  c.player.plays[c.player.plays.length - 1].finish('end', 10);
  await tick();
  assert.equal(c.s.state, 'answering');
  return c.s.item.text;
}

test('a keyed attempt says so, stores its corrections and spacing', async () => {
  const c = session();
  const text = await toAnswer(c);
  await c.s.submit(text, 100, { corrections: 2, spacing: 'lenient' });
  const [a] = await c.store.all();
  assert.equal(a.source, 'keyed');
  assert.equal(a.corrections, 2);
  assert.equal(a.spacing, 'lenient');
  assert.equal(a.perfect, true, 'corrections do not change the score');
});

test('a typed attempt is unchanged', async () => {
  const c = session();
  const text = await toAnswer(c);
  await c.s.submit(text, 100);
  const [a] = await c.store.all();
  assert.equal(a.source, 'typed');
  assert.equal(a.corrections, 0);
  assert.equal('spacing' in a, false);
});

test('keyed answers never touch the boxes or the confusions, and tally apart', async () => {
  const c = session();
  for (let i = 0; i < 12; i++) {
    const text = await toAnswer(c);
    await c.s.submit(i % 2 ? text : 'xyz', 100, { corrections: 0, spacing: 'strict' });
    if (c.s.state === 'feedback-playing') { c.player.plays[c.player.plays.length - 1].finish('end', 20); await tick(); }
  }
  for (const ch of ['k', 'm', 'r']) {
    const row = c.mastery.row(ch);
    assert.equal(row.box, 1);
    assert.equal(row.attempts, 0);
  }
  assert.equal(c.mastery.confusionList(6).length, 0);
  assert.equal(c.s.tally.items, 0);
  assert.equal(c.s.keyedTally.items, 12);
  assert.ok(['k', 'm', 'r'].some((ch) => c.mastery.row(ch).keyed.attempts > 0), 'they count in their own tally');
});

test('a missed keyed item comes back like a typed one', async () => {
  const c = session();
  const text = await toAnswer(c);
  await c.s.submit('xyz', 100, { corrections: 0, spacing: 'strict' });
  assert.equal(c.s.state, 'feedback-playing');
  assert.equal(c.s.scheduler.retries[0].text, text);
});

async function toSecondTry(c) {
  const text = await toAnswer(c);
  await c.s.submit('xyz', 100, { corrections: 0, spacing: 'strict' });
  assert.equal(c.s.state, 'feedback-playing');
  c.player.plays[c.player.plays.length - 1].finish('end', 200);
  await tick();
  assert.equal(c.s.state, 'second-try');
  return text;
}

test('a missed keyed answer offers a second try once it has sounded again; a missed typed one does not', async () => {
  await toSecondTry(session());
  const c = session();
  await toAnswer(c);
  await c.s.submit('xyz', 100);
  c.player.plays[c.player.plays.length - 1].finish('end', 200);
  await tick();
  assert.equal(c.s.state, 'feedback');
  assert.equal(c.s.secondTry('km'), null, 'no second try for a typed answer');
});

test('the second try is marked and nothing of it is kept', async () => {
  for (const right of [true, false]) {
    const c = session();
    const text = await toSecondTry(c);
    const heard = [];
    c.s.on('second', (x) => heard.push(x.score));
    const stored = (await c.store.all()).length;
    const tally = JSON.stringify(c.s.keyedTally);
    const typed = JSON.stringify(c.s.tally);
    const rows = JSON.stringify(c.mastery.row('k'));
    const retries = c.s.scheduler.retries.length;
    const s = c.s.secondTry(right ? text : 'xyz');
    assert.equal(s.perfect, right);
    assert.equal(heard.length, 1);
    assert.equal(heard[0].perfect, right);
    assert.equal(c.s.state, 'feedback');
    assert.equal((await c.store.all()).length, stored, 'not logged');
    assert.equal(JSON.stringify(c.s.keyedTally), tally, 'not in the keyed tally');
    assert.equal(JSON.stringify(c.s.tally), typed, 'not in the typed tally');
    assert.equal(JSON.stringify(c.mastery.row('k')), rows, 'no box moves');
    assert.equal(c.s.scheduler.retries.length, retries, 'the scored attempt still comes back, right or wrong');
    assert.equal(retries, 1);
  }
});

test('no try made: no mark, and it goes on', async () => {
  const c = session();
  await toSecondTry(c);
  const heard = [];
  c.s.on('second', (x) => heard.push(x));
  assert.equal(c.s.secondTry(null), null);
  assert.equal(heard.length, 0);
  assert.equal(c.s.state, 'feedback');
});

test('the second try is taken only once, and only when it is offered', async () => {
  const c = session();
  await toSecondTry(c);
  c.s.secondTry('xyz');
  assert.equal(c.s.secondTry('xyz'), null);
  const d = session();
  await toAnswer(d);
  assert.equal(d.s.secondTry('xyz'), null, 'not while answering');
  assert.equal(d.s.state, 'answering');
});

test('Enter can skip the second try: the next item starts', async () => {
  const c = session();
  await toSecondTry(c);
  const first = c.s.item;
  c.s.next();
  assert.notEqual(c.s.item, first);
  assert.equal(c.s.state, 'playing');
  assert.equal(c.s.item.secondTry, undefined, 'the next item starts clean');
});

test('a paddle touch during the replay plays it again, three times at most, then the second try', async () => {
  const c = session();
  c.s.next();
  // stops used up on the prompt do not count against the replay
  for (let i = 0; i < 3; i++) { c.player.plays[c.player.plays.length - 1].finish('stopped', 1); await tick(); }
  c.player.plays[c.player.plays.length - 1].finish('end', 10);
  await tick();
  await c.s.submit('xyz', 100, { corrections: 0, spacing: 'strict' });
  const before = c.player.plays.length;
  for (let i = 0; i < 3; i++) {
    c.player.plays[c.player.plays.length - 1].finish('stopped', 200);
    await tick();
    assert.equal(c.s.state, 'feedback-playing');
  }
  assert.equal(c.player.plays.length, before + 3);
  c.player.plays[c.player.plays.length - 1].finish('stopped', 300);
  await tick();
  assert.equal(c.s.state, 'second-try');
  assert.equal(c.player.plays.length, before + 3);
});

test('a paddle touch during a typed replay still ends it without playing again', async () => {
  const c = session();
  await toAnswer(c);
  await c.s.submit('xyz', 100);
  const before = c.player.plays.length;
  c.player.plays[c.player.plays.length - 1].finish('stopped', 200);
  await tick();
  assert.equal(c.s.state, 'feedback');
  assert.equal(c.player.plays.length, before);
});

test('playAgain sounds it once more, and that is not help', async () => {
  const c = session();
  await toAnswer(c);
  assert.equal(c.s.playAgain(), true);
  assert.equal(c.s.state, 'playing');
  c.player.plays[c.player.plays.length - 1].finish('end', 30);
  await tick();
  assert.equal(c.s.state, 'answering');
  assert.equal(c.s.item.helped, false);
  assert.equal(c.s.item.replays, 0);
  c.s.pause();
  assert.equal(c.s.playAgain(), false);
});
