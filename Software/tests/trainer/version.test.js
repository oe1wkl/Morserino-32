// The trainer's page and its two scripts must carry one VERSION, and the page must load the
// scripts with that ?v= tag -- otherwise a browser can run a new page with a cached old script.
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('fs');
const path = require('path');
const U = path.join(__dirname, '..', '..', 'Utilities');
const L = require(path.join(U, 'm32_trainer_link.js'));
const E = require(path.join(U, 'm32_trainer_engine.js'));
const html = fs.readFileSync(path.join(U, 'm32_trainer.html'), 'utf8');

test('page, link and engine carry the same VERSION, and the page loads the scripts with it', () => {
  assert.match(L.VERSION, /^\d{4}-\d{2}-\d{2}[a-z]?$/);
  assert.equal(E.VERSION, L.VERSION);
  assert.match(html, new RegExp("const PAGE_VERSION = '" + L.VERSION + "'"));
  const srcs = [...html.matchAll(/<script src="(m32_trainer_[a-z]+\.js)(\?v=([^"]*))?"/g)];
  assert.deepEqual(srcs.map((m) => m[1]).sort(), ['m32_trainer_engine.js', 'm32_trainer_link.js']);
  for (const m of srcs) assert.equal(m[3], L.VERSION, m[1] + ' is loaded without ?v=' + L.VERSION);
});
