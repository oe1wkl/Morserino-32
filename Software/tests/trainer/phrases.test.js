// M3b: the corpus layer, words and phrases in the scheduler, template filling, the ladder offer.
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('path');
const U = path.join(__dirname, '..', '..', 'Utilities');
const E = require(path.join(U, 'm32_trainer_engine.js'));
const real = require(path.join(U, 'm32_trainer_corpus.json'));

// A small corpus: every expectation below can be checked by eye
const tiny = {
  schema: 1, lang: 'en',
  words: ['tea', 'eat', 'ate', 'sea', 'set', 'east', 'seat', 'tease', 'state', 'taste', 'stem', 'meat'],
  pools: { NAME: [['tom', 0x02], ['otto', 0x01]], RIG: ['k3', 'ft817'] },
  prefixes: [['k', 0x02, 5, 200, 1, '1111111111'], ['dl', 0x01, 14, 200, 1, '1111111111']],
  phrases: [
    { tier: 'B', src: 'authored', text: 'eat tea' }, { tier: 'B', src: 'authored', text: 'set a seat' },
    { tier: 'C', src: 'realqsos', text: 'tnx fer call' }, { tier: 'B', src: 'authored', text: 'the end' }
  ],
  templates: [{ tier: 'C', src: 'authored', text: 'name [NAME]' }, { tier: 'C', src: 'authored', text: '[CALL] de [CALL2]' },
              { tier: 'C', src: 'authored', text: 'rig [RIG] es ant' }, { tier: 'C', src: 'authored', text: '[CALL] name [NAME]' }]
};
const chars = (s) => s.split('');

test('the strict filter: only items made of the active characters, counted per length', () => {
  const c = new E.Corpus(tiny);
  const av = c.available(chars('teasm'), 2);
  assert.deepEqual(av.words, ['tea', 'eat', 'ate', 'sea', 'set', 'east', 'seat', 'tease', 'state', 'taste', 'stem', 'meat']);
  assert.deepEqual(av.items.B.map((x) => x.text), ['eat tea']);       // 'the end' needs h, n, d
  assert.equal(av.count.C, 0);                                         // name [NAME] needs n
  assert.deepEqual(av.eligible, ['A']);                                // B has 1 < 10
  assert.deepEqual(c.available(chars('teasm'), 3).items.B.map((x) => x.text), ['set a seat']);
  // a template is usable when its fixed text is and every slot has a usable value
  const n = c.available(chars('teasmnoft817rigk3'), 2);
  assert.deepEqual(n.items.C.map((x) => x.text), ['name [NAME]']);
  // 3 words: the two templates with calls (prefixes k, dl; digits 8 1 7 3); tnx fer call needs x
  assert.deepEqual(c.available(chars('teasmnoft817rigk3dlc'), 3).items.C.map((x) => x.text).sort(),
                   ['[CALL] de [CALL2]', '[CALL] name [NAME]']);
  assert.deepEqual(c.available(chars('teasmnoft817rigk3dlc'), 4).items.C.map((x) => x.text), ['rig [RIG] es ant']);
});

test('templates are filled with values of the active characters, names suiting the call\'s continent', () => {
  const c = new E.Corpus(tiny);
  const act = chars('abcdefghijklmnopqrstuvwxyz0123456789');
  const call = c.items.find((x) => x.text === '[CALL] de [CALL2]');
  for (let i = 0; i < 20; i++) assert.match(c.fill(call, act, E.mulberry32(i)), /^(k|dl)[0-9][a-z]{1,3}(\/[mp])? de (k|dl)[0-9][a-z]{1,3}(\/[mp])?$/);
  const name = c.items.find((x) => x.text === 'name [NAME]');
  assert.match(c.fill(name, act, E.mulberry32(1)), /^name (tom|otto)$/);        // no call: any name
  // with a call, the name suits its continent: k (NA) -> tom, dl (EU) -> otto
  const both = c.items.find((x) => x.text === '[CALL] name [NAME]');
  for (let i = 0; i < 30; i++) {
    const t = c.fill(both, act, E.mulberry32(100 + i));
    assert.ok(/^k[0-9]/.test(t) ? /name tom$/.test(t) : /^dl[0-9].* name otto$/.test(t), t);
  }
});

test('words and phrases from the scheduler: right kind, tier and length, only active characters', () => {
  const c = new E.Corpus(real);
  const seq = 'mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-='.split('').concat(['<sk>', '+', '<as>', '<kn>', '<ka>', '<ve>', '<bk>']);
  for (const L of [16, 28, 45]) {
    const act = seq.slice(0, L);
    const s = new E.Scheduler({ mastery: new E.Mastery(), chars: act, rng: E.mulberry32(L), corpus: c, mode: 'words' });
    for (let i = 0; i < 30; i++) {
      const it = s.next();
      assert.equal(it.kind, 'word');
      assert.equal(E.splitWords(it.text).length, 1);
      for (const t of E.tokenize(it.text)) assert.ok(act.includes(t), it.text);
    }
    s.mode = 'phrases';
    for (const n of [2, 3, 5]) {
      s.length = n;
      for (let i = 0; i < 30; i++) {
        const it = s.next();
        assert.equal(it.kind, 'phrase');
        assert.ok(s.available().eligible.includes(it.tier), it.tier);
        assert.equal(E.splitWords(it.text).length, n, it.text);
        for (const t of E.tokenize(it.text)) if (t !== ' ') assert.ok(act.includes(t), it.text + ' at lesson ' + L);
      }
    }
  }
});

test('a pinned tier is used alone; nothing usable throws NoItemsError', () => {
  const c = new E.Corpus(real);
  const act = 'mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x'.split('');
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars: act, rng: E.mulberry32(3), corpus: c, mode: 'phrases', length: 3, tier: 'C' });
  for (let i = 0; i < 20; i++) assert.equal(s.next().tier, 'C');
  const early = new E.Scheduler({ mastery: new E.Mastery(), chars: ['m', 'k'], rng: E.mulberry32(3), corpus: c, mode: 'phrases' });
  assert.throws(() => early.next(), (e) => e.name === 'NoItemsError' && e.what === 'phrases');
  early.mode = 'words';
  assert.throws(() => early.next(), (e) => e.name === 'NoItemsError' && e.what === 'words');
});

test('recently drawn items are skipped while there is anything else; weak characters weigh more', () => {
  const c = new E.Corpus(tiny);
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars: chars('teasm'), rng: E.mulberry32(9), corpus: c, mode: 'words' });
  const seen = [];
  for (let i = 0; i < 12; i++) seen.push(s.next().text);
  assert.equal(new Set(seen).size, 12);                                // 12 words, none twice in a row of 12
  const m = new E.Mastery();
  for (const ch of 'teas') { const st = m._get(ch); st.box = 5; }      // m is the only weak character
  const w = new E.Scheduler({ mastery: m, chars: chars('teasm'), rng: E.mulberry32(9), corpus: c });
  assert.ok(w.itemWeight('meat') > w.itemWeight('east'));
  assert.equal(w.itemWeight('name [NAME]'), w.itemWeight('name'));       // a slot counts as nothing
});

test('a missed word or phrase comes back with its kind and tier', () => {
  const c = new E.Corpus(tiny);
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars: chars('teasm'), rng: E.mulberry32(4), corpus: c, mode: 'phrases', length: 2, tier: 'A' });
  const first = s.next();
  s.missed(first);
  const later = [];
  for (let i = 0; i < 5; i++) later.push(s.next());
  const back = later.find((x) => x.retry);
  assert.deepEqual([back.text, back.kind, back.tier], [first.text, 'phrase', 'A']);
});

test('the learner\'s own lines: 2-8 words kept, comments skipped, filtered like the rest', () => {
  const c = new E.Corpus(tiny);
  const r = c.setUser('# my club\neat tea\nseat\nset   a  SEAT\nthe end of the long day is here at last now\n\n');
  assert.deepEqual(r, { lines: 4, kept: 2 });
  const av = c.available(chars('teasm'), 2);
  assert.deepEqual(av.items.U.map((x) => x.text), ['eat tea']);
  assert.deepEqual(c.available(chars('teasm'), 3).items.U.map((x) => x.text), ['set a seat']);
});

test('the ladder: offered after 10 phrases of a length, up at >= 90 % of the words, down below 60 %', () => {
  const at = (t, n, right) => ({ kind: 'phrase', t, source: 'typed', perWord: Array.from({ length: n }, (_, i) => ({ ok: i < right })) });
  const nine = Array.from({ length: 9 }, (_, i) => at(i, 3, 3));
  assert.equal(E.ladderOffer(nine, 3), null);                                    // only 9
  assert.equal(E.ladderOffer(nine.concat([at(9, 3, 3)]), 3), 'up');
  assert.equal(E.ladderOffer(nine.concat([at(9, 3, 3)]), 4), null);              // other length
  const low = Array.from({ length: 10 }, (_, i) => at(i, 4, 2));
  assert.equal(E.ladderOffer(low, 4), 'down');
  assert.equal(E.ladderOffer(Array.from({ length: 10 }, (_, i) => at(i, 2, 1)), 2), null);   // already at 2
  assert.equal(E.ladderOffer(Array.from({ length: 10 }, (_, i) => at(i, 8, 8)), 8), null);   // already at 8
  const mixed = Array.from({ length: 10 }, (_, i) => at(i, 3, i < 7 ? 3 : 2));             // 70 % + 30 % x 2/3 = 90 %
  assert.equal(E.ladderOffer(mixed, 3), 'up');
  const almost = Array.from({ length: 10 }, (_, i) => at(i, 4, i < 4 ? 4 : 3));            // 40 % + 60 % x 3/4 = 85 %
  assert.equal(E.ladderOffer(almost, 4), null);
  const justLow = Array.from({ length: 10 }, (_, i) => at(i, 5, 3));                        // 60 %: no step down
  assert.equal(E.ladderOffer(justLow, 5), null);
  const keyed = nine.concat([Object.assign(at(9, 3, 3), { source: 'keyed' })]);
  assert.equal(E.ladderOffer(keyed, 3), null);                                   // keyed ones do not count
});

test('a missed item waits until it fits the settings', () => {
  const c = new E.Corpus(tiny);
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars: chars('teasm'), rng: E.mulberry32(5), corpus: c, mode: 'groups', size: 5 });
  const long = s.next();
  assert.equal(long.text.length, 5);
  s.missed(long);
  s.size = 1;
  for (let i = 0; i < 12; i++) assert.equal(s.next().text.length, 1, 'a pinned size holds');
  s.size = 'mixed';                       // overdue by now, and mixed takes any group
  const back = s.next();
  assert.equal(back.text, long.text);
  assert.equal(back.retry, true);
  s.size = 5;
  s.missed(back);
  s.mode = 'words';
  for (let i = 0; i < 8; i++) assert.equal(s.next().kind, 'word', 'a group stays out of Words');
  s.mode = 'groups';
  assert.equal(s.next().text, long.text, 'and returns with Groups at its size');
});

test('a missed phrase waits for its length and its kind', () => {
  const act = chars('abcdefghijklmnopqrstuvwxyz0123456789');
  const s = new E.Scheduler({ mastery: new E.Mastery(), chars: act, rng: E.mulberry32(6), corpus: new E.Corpus(real), mode: 'phrases', length: 3, tier: 'C' });
  const p = s.next();
  assert.equal(p.n, 3);
  s.missed(p);
  s.length = 4;
  for (let i = 0; i < 8; i++) assert.notEqual(s.next().text, p.text, 'another length: it waits');
  s.length = 3;
  s.tier = 'B';
  for (let i = 0; i < 8; i++) assert.equal(s.next().tier, 'B', 'another kind pinned: it waits');
  s.tier = 'mixed';
  const back = s.next();
  assert.equal(back.text, p.text);
  assert.equal(back.n, 3);
  s.missed(back);
  s.tier = 'C';
  for (const r of s.retries) r.wait = 0;
  assert.equal(s.next().text, p.text, 'a retry keeps its length and kind');
});
