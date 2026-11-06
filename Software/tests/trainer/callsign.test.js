// The engine's randomCall() is a port of the firmware's getRandomCall(). The fixture is written by
// the firmware's own code compiled on the host with the engine's random generator
// (Software/tests/callsign_gen/, `make run`), so without a lesson filter the port must make exactly
// the same calls, continents and CQ zones.
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const path = require('path');
const E = require(path.join(__dirname, '..', '..', 'Utilities', 'm32_trainer_engine.js'));
const corpus = require(path.join(__dirname, '..', '..', 'Utilities', 'm32_trainer_corpus.json'));
const fixture = require('./fixtures/random_calls.json');
const MASKS = [0x7F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20];      // getContinentMask(), prefs 0..6

test('randomCall() makes the firmware\'s calls for the same random numbers', () => {
  assert.ok(fixture.length >= 40);
  for (const c of fixture) {
    const rng = E.mulberry32(c.seed);
    const opts = { rng, maxLength: c.maxLength, continents: MASKS[c.continentPref], commonOnly: c.common === 1 };
    const got = c.calls.map(() => { const r = E.randomCall(corpus.prefixes, opts); return [r.call, r.continent, r.zone]; });
    assert.deepEqual(got, c.calls, JSON.stringify({ maxLength: c.maxLength, cont: c.continentPref, common: c.common, seed: c.seed }));
  }
});

test('with the learner\'s characters, a call uses only those, or there is none', () => {
  const seq = 'mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-='.split('');
  const rng = E.mulberry32(5);
  let made = 0;
  for (let n = 1; n <= seq.length; n++) {
    const active = seq.slice(0, n);
    for (let i = 0; i < 50; i++) {
      const r = E.randomCall(corpus.prefixes, { rng, active });
      if (!r) continue;
      made++;
      for (const ch of r.call) assert.ok(active.includes(ch), r.call + ' at lesson ' + n);
      assert.match(r.call, /[0-9]/, r.call);
    }
  }
  assert.ok(made > 1000);
  assert.equal(E.randomCall(corpus.prefixes, { active: ['m', 'k', 'r', 's'] }), null);   // no digit yet
  assert.equal(E.randomCall(corpus.prefixes, { active: ['0', '1'] }), null);             // no letters
});

test('the call-area digit follows the table\'s weights, and only learned digits are used', () => {
  // OE: digits 1-9 in use, 3 far more often than 7; 3B8 takes no digit
  const table = [['oe', 0x01, 15, 100, 1, '011f111110'], ['3b8', 0x08, 39, 100, 1, '0000000000']];
  const rng = E.mulberry32(9);
  const n = {};
  for (let i = 0; i < 4000; i++) {
    const r = E.randomCall(table, { rng, maxLength: 3 });
    if (r.call.startsWith('oe')) n[r.call[2]] = (n[r.call[2]] || 0) + 1;
    else assert.match(r.call, /^3b8[a-z]+$/, r.call);          // no digit added after 3b8
  }
  assert.equal(n['0'], undefined);                              // weight 0: never
  assert.ok(n['3'] > 5 * n['7'], JSON.stringify(n));
  // only 7 learned: OE takes 7 (its only learned digit in use)
  const act = ['o', 'e', 'a', 'b', '7'];
  for (let i = 0; i < 50; i++) {
    const r = E.randomCall(table, { rng, active: act });
    assert.match(r.call, /^oe7[oeab]+$/, r.call);
  }
  // 0 learned only: OE never uses 0 and 3B8 needs 3 and 8, so no prefix fits: the firmware's
  // fallback, a letter and a digit, then the suffix
  const r = E.randomCall(table, { rng, active: ['o', 'e', 'a', '0'] });
  assert.equal(r.continent, 0x7F);
  assert.match(r.call, /^[oea]0[oea]+$/, r.call);
});
