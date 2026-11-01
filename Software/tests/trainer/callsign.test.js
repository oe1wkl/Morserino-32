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
