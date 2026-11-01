// Tests for Software/Utilities/m32_trainer_link.js (M32 Trainer, milestone M2).
// No device needed:   node --test Software/tests/trainer/
//
// The objects below are the device's own: the cw/trainer events were captured on the M1
// bench (classic and Pocket, 2026-10-30/31; see Software/tests/protocol/selftest.py), the
// rest follow MorseJSON.cpp. Time is a fake clock, so timeouts and the stall rule are
// exercised exactly and the suite runs in well under a second.

'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { Framer, Link, defaultExpect } = require('../../Utilities/m32_trainer_link.js');

// ---------------------------------------------------------------- fixtures

const DEVICE = '{"device":{"hardware":"M32 Pocket","firmware":"10.0","protocol":"1.6","build":"Oct 31 2026","edition":"standard"}}';
const CAPS = '{"capabilities":{"protocol":"1.6","features":["trainer-events","games","ble-serial"]}}';
const OK = '{"ok":{"content":"OK"}}';
const GOODBYE = '{"end m32protocol":{"content":"Goodbye!"}}';
const DONE_END = '{"cw":{"event":"done","reason":"end"}}';
const DONE_STOPPED = '{"cw":{"event":"done","reason":"stopped"}}';
const RESULT = '{"trainer":{"event":"result","mode":"echo","prompt":"8k","answer":"8k","ok":true,"repeat":0,"wpm":18,"latency":857,"duration":1928}}';
const RESULT_SK = '{"trainer":{"event":"result","mode":"echo","prompt":"<sk>","answer":"<ve>","ok":false,"repeat":1,"wpm":18,"latency":430,"duration":900}}';
const KOCH = '{"kochlesson":{"value":5,"minimum":1,"maximum":51,"characters":["m","k","r","s","u"]}}';
const MENU = '{"menu":{"content":"Koch Trainer/Echo Trainer","menu number":24,"executable":true,"active":false}}';
const CONTROL = '{"control":{"name":"speed","value":19}}';
// a CW memory may hold anything, braces and quotes included (the Config Tool once framed it short)
const MEMORY = '{"CW Memory":{"number":1,"content":"cq {de} \\"oe1wkl\\" \\\\ [p] }"}}';
// an escaped quote right before a brace: a framer that ignores backslashes ends the string early
const MEMORY_ESC = '{"CW Memory":{"number":2,"content":"\\"}"}}';
const NOT_ACTIVE = '{"error":{"content":"CW/PLAY: Keyer not active"}}';
const CONFIRM = '{"message":{"content":"CONFIRM ON DEVICE"}}';

// ---------------------------------------------------------------- helpers

function rng(seed) {                           // mulberry32: reproducible "random" chunking
  return () => {
    seed |= 0; seed = seed + 0x6D2B79F5 | 0;
    let t = Math.imul(seed ^ seed >>> 15, 1 | seed);
    t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
    return ((t ^ t >>> 14) >>> 0) / 4294967296;
  };
}

// feed `chunks` to a fresh framer; return the objects (as raw text) and the text, joined
function frame(chunks, opts) {
  const f = new Framer(opts);
  const objects = [], text = [], junk = [];
  for (const c of chunks) {
    for (const it of f.feed(c, 0)) {
      if (it.kind === 'object') objects.push(it.raw);
      else if (it.kind === 'text') text.push(it.text);
      else junk.push(it);
    }
  }
  return { objects, text: text.join(''), junk, framer: f };
}

const flush = () => new Promise((r) => setImmediate(r));

function fakeClock() {
  let now = 0, seq = 0;
  const timers = new Map();
  return {
    now: () => now,
    setTimeout(fn, ms) { const id = ++seq; timers.set(id, { at: now + ms, fn, id }); return id; },
    clearTimeout(id) { timers.delete(id); },
    async advance(ms) {
      const end = now + ms;
      for (;;) {
        await flush();
        let next = null;
        for (const t of timers.values())
          if (t.at <= end && (!next || t.at < next.at || (t.at === next.at && t.id < next.id))) next = t;
        if (!next) break;
        timers.delete(next.id);
        now = next.at;
        next.fn();
      }
      now = end;
      await flush();
    }
  };
}

// A transport whose far end is a scripted M32: reply(line) returns the text the device
// answers with (undefined: the standard answer, null: silence).
function fakeM32(reply) {
  const t = {
    label: 'fake', maxLine: 1024, onData: null, onClose: null,
    lines: [], writes: [], closed: false, silent: false,
    write(s) {
      t.writes.push(s);
      for (const line of s.split('\n').filter(Boolean)) {
        t.lines.push(line);
        let r = reply ? reply(line) : undefined;
        if (r === undefined) r = standard(line);
        if (r != null && !t.silent) Promise.resolve().then(() => t.onData && t.onData(r));
      }
      return Promise.resolve();
    },
    close() { t.closed = true; return Promise.resolve(); },
    send(text) { t.onData(text); }
  };
  return t;
}

function standard(line) {
  const l = line.toLowerCase();
  if (l === 'put device/protocol/on') return DEVICE;
  if (l === 'put device/protocol/off') return GOODBYE;
  if (l === 'get capabilities') return CAPS;
  if (l === 'get kochlesson') return KOCH;
  if (l.startsWith('put ')) return OK;
  return null;
}

async function connected(reply, opts) {
  const clock = fakeClock();
  const t = fakeM32(reply);
  const link = new Link(Object.assign({ clock }, opts));
  const p = link.connect(t);
  await clock.advance(0);
  await p;
  return { clock, t, link };
}

function record(link, names) {
  const seen = [];
  for (const n of names) link.on(n, (a, b) => seen.push([n, a, b]));
  return seen;
}

// ================================================================ FRAMER

const STREAM = [DEVICE, 'cq ', DONE_END, MEMORY, MEMORY_ESC, 'de oe1wkl <sk> ', RESULT_SK, '+=?', RESULT, OK];
const STREAM_TEXT = 'cq de oe1wkl <sk> +=?';
const STREAM_OBJECTS = STREAM.filter((s) => s.startsWith('{'));

test('framer: objects and echo, whole', () => {
  const r = frame([STREAM.join('')]);
  assert.deepEqual(r.objects, STREAM_OBJECTS);
  assert.equal(r.text, STREAM_TEXT);
  assert.equal(r.junk.length, 0);
});

test('framer: split at every position', () => {
  const s = STREAM.join('');
  for (let i = 1; i < s.length; i++) {
    const r = frame([s.slice(0, i), s.slice(i)]);
    assert.deepEqual(r.objects, STREAM_OBJECTS, 'split at ' + i);
    assert.equal(r.text, STREAM_TEXT, 'split at ' + i);
  }
});

test('framer: random chunking, down to single characters', () => {
  const s = STREAM.join(''), next = rng(1);
  for (let round = 0; round < 300; round++) {
    const chunks = [];
    for (let i = 0; i < s.length;) {
      const n = 1 + Math.floor(next() * (round % 3 === 0 ? 3 : 40));
      chunks.push(s.slice(i, i + n));
      i += n;
    }
    const r = frame(chunks);
    assert.deepEqual(r.objects, STREAM_OBJECTS, 'round ' + round);
    assert.equal(r.text, STREAM_TEXT, 'round ' + round);
  }
});

test('framer: braces, quotes and backslashes inside strings do not count', () => {
  const r = frame([MEMORY]);
  assert.deepEqual(r.objects, [MEMORY]);
  assert.equal(JSON.parse(r.objects[0])['CW Memory'].content, 'cq {de} "oe1wkl" \\ [p] }');
  assert.deepEqual(frame([MEMORY_ESC]).objects, [MEMORY_ESC]);
  assert.equal(JSON.parse(MEMORY_ESC)['CW Memory'].content, '"}');
});

test('framer: text is handed on at once, an object only when it is complete', () => {
  const f = new Framer();
  assert.deepEqual(f.feed('cq', 0).map((i) => i.text), ['cq']);
  assert.deepEqual(f.feed(' {"ok":{"con', 0).map((i) => i.text), [' ']);
  assert.ok(f.open);
  const done = f.feed('tent":"OK"}}de', 7);
  assert.equal(done[0].kind, 'object');
  assert.equal(done[0].t, 7);
  assert.equal(done[1].text, 'de');
  assert.ok(!f.open);
});

test('framer: noise between objects stays text and does not disturb them', () => {
  const next = rng(7), noise = 'abc xyz 0123456789 .,?/=+-:@"\'\r\n<>[]';
  const parts = [], want = [];
  let text = '';
  for (let i = 0; i < 200; i++) {
    let n = '';
    for (let k = Math.floor(next() * 12); k > 0; k--) n += noise[Math.floor(next() * noise.length)];
    const o = STREAM_OBJECTS[i % STREAM_OBJECTS.length];
    parts.push(n, o);
    text += n;
    want.push(o);
  }
  const s = parts.join(''), chunks = [];
  for (let i = 0; i < s.length;) { const n = 1 + Math.floor(next() * 50); chunks.push(s.slice(i, i + n)); i += n; }
  const r = frame(chunks);
  assert.deepEqual(r.objects, want);
  assert.equal(r.text, text);
  assert.equal(r.junk.length, 0);
});

test('framer: prosigns and UTF-8 pass through strings and echo', () => {
  const umlaut = '{"trainer":{"event":"result","mode":"echo","prompt":"ä<ch>","answer":"ä<ch>","ok":true,"repeat":0,"wpm":18,"latency":1,"duration":2}}';
  const r = frame(['<sk> ä ', umlaut, '<ka>']);
  assert.equal(r.text, '<sk> ä <ka>');
  assert.equal(JSON.parse(r.objects[0]).trainer.prompt, 'ä<ch>');
});

test('framer: a torn object is given up after stallMs of silence, and framing resumes', () => {
  const f = new Framer({ stallMs: 1500 });
  f.feed('{"cw":{"event":"do', 100);
  assert.deepEqual(f.check(1599), []);
  const out = f.check(1600);
  assert.equal(out.length, 1);
  assert.equal(out[0].kind, 'junk');
  assert.equal(out[0].why, 'stalled');
  assert.ok(!f.open);
  const next = f.feed(OK + 'cq', 2000);
  assert.equal(next[0].raw, OK);
  assert.equal(next[1].text, 'cq');
});

test('framer: an object that arrived behind a torn one is recovered, marked late', () => {
  const f = new Framer({ stallMs: 1500 });
  // the tail of a done was dropped; the next reply, and keyed echo, arrived inside the open object
  assert.deepEqual(f.feed('{"cw":{"event":"do' + OK + 'cq de', 0), []);
  const out = f.check(1500);
  assert.equal(out[0].kind, 'junk');
  assert.equal(out[0].raw, '{"cw":{"event":"do');
  assert.equal(out[1].kind, 'object');
  assert.equal(out[1].raw, OK);
  assert.equal(out[1].late, true);
  assert.equal(out[2].text, 'cq de');
  assert.equal(out[2].late, true);
  assert.ok(!f.open);
});

test('framer: a stray "{" in debug text is junk; what follows is framed', () => {
  const r = frame(['boot {bar} x', OK]);
  assert.equal(r.text, 'boot  x');
  assert.deepEqual(r.objects, [OK]);
  assert.equal(r.junk.length, 1);
  assert.equal(r.junk[0].raw, '{bar}');
  assert.equal(r.junk[0].why, 'unparsable');
});

test('framer: an unparsable object that wraps a good one gives the good one back', () => {
  const r = frame(['{oops ' + OK + '}' + DONE_END]);
  assert.deepEqual(r.objects, [OK, DONE_END]);
  assert.equal(r.junk[0].raw, '{oops ');
  assert.equal(r.text, '}');
});

test('framer: an unparsable object followed by one still arriving keeps that one open', () => {
  const f = new Framer();
  const out = f.feed('{x}' + '{"ok":{"cont', 0);
  assert.equal(out.length, 1);
  assert.equal(out[0].kind, 'junk');
  assert.ok(f.open);
  assert.equal(f.feed('ent":"OK"}}', 1)[0].raw, OK);
});

test('framer: an object past maxObject is junk', () => {
  const f = new Framer({ maxObject: 100 });
  const out = f.feed('{"x":"' + 'a'.repeat(200), 0);
  assert.equal(out[0].kind, 'junk');
  assert.equal(out[0].why, 'oversize');
  assert.ok(!f.open);
  assert.equal(f.feed(OK, 1)[0].raw, OK);
});

test('framer: a stall inside recovered data repeats until the stream is in sync', () => {
  const f = new Framer({ stallMs: 10 });
  f.feed('{"a":{"b":"x{"c":"', 0);         // quote parity broken twice over, nothing complete inside
  const out = f.check(10);
  assert.ok(out.every((i) => i.kind === 'junk'));
  assert.ok(!f.open);
});

// ================================================================ LINK

test('link: defaultExpect knows the reply keys', () => {
  assert.deepEqual(defaultExpect('get kochlesson'), ['kochlesson']);
  assert.deepEqual(defaultExpect('GET Config/Serial Output'), ['config']);
  assert.deepEqual(defaultExpect('put control/speed/18'), ['control']);
  assert.deepEqual(defaultExpect('put cw/play/cq cq'), ['ok']);
  assert.deepEqual(defaultExpect('put device/protocol/on'), ['device']);
  assert.deepEqual(defaultExpect('put device/protocol/off'), ['end m32protocol']);
  assert.equal(defaultExpect('get somethingnew'), null);
});

test('link: connect handshakes and reads the capabilities', async () => {
  const { link, t } = await connected();
  assert.equal(link.state, 'ready');
  assert.equal(link.device.protocol, '1.6');
  assert.equal(link.hasFeature('trainer-events'), true);
  assert.equal(link.hasFeature('kip'), false);
  assert.deepEqual(t.lines, ['put device/protocol/on', 'get capabilities']);
});

test('link: firmware before 1.4 has no capabilities, and that is not an error', async () => {
  const { link } = await connected((l) => (l === 'get capabilities' ? '{"error":{"content":"UNKNOWN COMMAND"}}' : undefined));
  assert.equal(link.state, 'ready');
  assert.equal(link.hasFeature('trainer-events'), null);
});

test('link: no M32 on the port', async () => {
  const clock = fakeClock(), t = fakeM32(() => null);
  const link = new Link({ clock });
  const p = link.connect(t);
  p.catch(() => {});
  await clock.advance(2000);
  await assert.rejects(p, (e) => e.timeout && /No M32 response/.test(e.message));
  assert.equal(link.state, 'closed');
  assert.ok(t.closed);
});

test('link: BLE consent waits for the operator, then the handshake completes', async () => {
  const clock = fakeClock();
  const t = fakeM32((l) => (l === 'put device/protocol/on' ? CONFIRM : undefined));
  const link = new Link({ clock });
  const seen = record(link, ['confirm']);
  const p = link.connect(t);
  await clock.advance(10000);                 // well past the 2 s handshake timeout
  assert.equal(seen.length, 1);
  assert.equal(link.state, 'connecting');
  t.send(DEVICE);                             // the operator pressed FN
  await clock.advance(0);
  assert.equal((await p).edition, 'standard');
});

test('link: a refused BLE session rejects with the device error', async () => {
  const clock = fakeClock();
  const t = fakeM32((l) => (l === 'put device/protocol/on' ? CONFIRM : undefined));
  const link = new Link({ clock });
  const p = link.connect(t);
  p.catch(() => {});
  await clock.advance(5000);
  t.send('{"error":{"content":"CONNECTION DECLINED"}}');
  await clock.advance(0);
  await assert.rejects(p, (e) => e.deviceError && e.message === 'CONNECTION DECLINED');
  assert.equal(link.state, 'closed');
});

test('link: one command in flight; the next is written only after the reply', async () => {
  const { link, t, clock } = await connected((l) => (l === 'get kochlesson' ? null : undefined));
  t.lines.length = 0;
  const a = link.command('get kochlesson');
  const b = link.command('put config/Serial Output/1');
  await clock.advance(0);
  assert.deepEqual(t.lines, ['get kochlesson']);
  t.send(KOCH);
  assert.equal((await a).kochlesson.value, 5);
  await clock.advance(0);
  assert.deepEqual(t.lines, ['get kochlesson', 'put config/Serial Output/1']);
  assert.deepEqual(await b, JSON.parse(OK));
});

test('link: unsolicited objects during a command go to listeners, not to the command', async () => {
  const { link, t, clock } = await connected((l) => (l === 'get kochlesson' ? null : undefined));
  const seen = record(link, ['cw', 'trainer', 'menu', 'control', 'text']);
  const a = link.command('get kochlesson');
  await clock.advance(0);
  t.send(DONE_END + MENU + 'cq ' + RESULT + CONTROL + KOCH);
  assert.equal((await a).kochlesson.maximum, 51);
  assert.deepEqual(seen.map((s) => s[0]), ['cw', 'menu', 'text', 'trainer', 'control']);
  assert.deepEqual(seen[0][1], { event: 'done', reason: 'end' });
  assert.equal(seen[2][1].text, 'cq ');
  assert.equal(seen[3][1].latency, 857);
  assert.equal(seen[3][2].reply, false);
});

test('link: every object, replies included, reaches "object" with its arrival time', async () => {
  const { link, t, clock } = await connected((l) => (l.startsWith('put cw/play') ? null : undefined));
  const seen = record(link, ['object']);
  const a = link.command('put cw/play/cq');
  await clock.advance(120);
  t.send(OK);
  await a;
  assert.equal(seen[0][2].reply, true);
  assert.equal(seen[0][2].command, 'put cw/play/cq');
  assert.equal(seen[0][2].t, 120);
});

test('link: an error reply rejects with the device error', async () => {
  const { link } = await connected((l) => (l.startsWith('put kochlesson') ? '{"error":{"content":"INVALID KOCH LESSON 99"}}' : undefined));
  await assert.rejects(link.command('put kochlesson/99'),
                       (e) => e.message === 'INVALID KOCH LESSON 99' && e.deviceError && e.command === 'put kochlesson/99');
  assert.equal(link.state, 'ready');
});

test('link: commands that cannot be one line are refused before sending', async () => {
  const { link, t } = await connected();
  const before = t.lines.length;
  await assert.rejects(link.command('put cw/play/cq\nput device/reset/defaults'), /control character/);
  await assert.rejects(link.command('put cw/play/' + 'e'.repeat(1100)), /longer than 1024/);
  await assert.rejects(link.command('   '), /empty/);
  assert.equal(t.lines.length, before);
});

test('link: a timeout, then the late reply is not taken for the next command', async () => {
  const { link, t, clock } = await connected((l) => (l === 'get kochlesson' ? null : undefined));
  const a = link.command('get kochlesson', { timeout: 1000 });
  const b = link.command('get kochlesson', { timeout: 1000 });
  a.catch(() => {});
  await clock.advance(1000);
  await assert.rejects(a, (e) => e.timeout);
  t.send(KOCH);                               // a's reply, late: swallowed in the grace period
  await clock.advance(250);
  // the link re-handshakes before it sends b
  const i = t.lines.lastIndexOf('put device/protocol/on');
  assert.ok(i > t.lines.indexOf('get kochlesson'));
  await clock.advance(0);
  assert.equal(t.lines[t.lines.length - 1], 'get kochlesson');
  t.send('{"kochlesson":{"value":6,"minimum":1,"maximum":51,"characters":[]}}');
  assert.equal((await b).kochlesson.value, 6);
});

test('link: a command sent in the grace period after a timeout does not get the late reply', async () => {
  const { link, t, clock } = await connected((l) => (l === 'get kochlesson' ? null : undefined));
  const a = link.command('get kochlesson', { timeout: 1000 });
  a.catch(() => {});
  await clock.advance(1000);
  await assert.rejects(a, (e) => e.timeout);
  const b = link.command('get kochlesson');   // issued while a's reply may still be on its way
  await clock.advance(10);
  t.send(KOCH);                               // ... and here it is
  await clock.advance(240);
  await clock.advance(0);
  t.send('{"kochlesson":{"value":6,"minimum":1,"maximum":51,"characters":[]}}');
  assert.equal((await b).kochlesson.value, 6);
});

test('link: a handshake is not held up by a stray "{" left in the framer', async () => {
  const { link, t, clock } = await connected();
  t.send('rst:0x1 (POWERON_RESET),boot:0x13 {');    // boot text with a brace, never closed
  const r = link.reconnect();
  let device = null;
  r.then((d) => { device = d; });
  await clock.advance(0);
  assert.equal(device && device.protocol, '1.6');
});

test('link: a device that restarted is re-handshaken, and the page is told', async () => {
  let session = true;
  const { link, clock } = await connected((l) => {
    if (l === 'put device/protocol/on') { session = true; return DEVICE; }
    return session ? undefined : null;        // without a session the M32 ignores every command
  });
  const seen = record(link, ['resync', 'state']);
  session = false;                            // the classic restarted behind its USB bridge
  const a = link.command('get kochlesson');
  a.catch(() => {});
  await clock.advance(3000 + 250);
  await assert.rejects(a, (e) => e.timeout);
  await clock.advance(0);
  assert.deepEqual(seen.map((s) => s[0]), ['resync']);
  assert.equal(link.state, 'ready');
  assert.equal((await link.command('get kochlesson')).kochlesson.value, 5);
});

test('link: no answer to the re-handshake either: unresponsive, queue rejected, reconnect() recovers', async () => {
  const { link, t, clock } = await connected();
  t.silent = true;
  const seen = record(link, ['unresponsive', 'reconnected']);
  const a = link.command('get kochlesson'), b = link.command('get kochlesson');
  a.catch(() => {}); b.catch(() => {});
  await clock.advance(3000 + 250 + 1500);
  await assert.rejects(a, (e) => e.timeout);
  await assert.rejects(b, /not answering/);
  assert.equal(link.state, 'unresponsive');
  assert.equal(seen[0][0], 'unresponsive');
  await assert.rejects(link.command('get kochlesson'), /not connected/);
  t.silent = false;
  const r = link.reconnect();
  await clock.advance(250);                   // the grace period after the probe's own timeout
  await r;
  assert.equal(link.state, 'ready');
  assert.equal(seen[1][0], 'reconnected');
});

test('link: a failed reconnect() leaves the state it found', async () => {
  const { link, t, clock } = await connected();
  t.silent = true;
  const a = link.command('get kochlesson');
  a.catch(() => {});
  await clock.advance(5000);
  assert.equal(link.state, 'unresponsive');
  const r = link.reconnect();
  r.catch(() => {});
  await clock.advance(2000);
  await assert.rejects(r, (e) => e.timeout);
  assert.equal(link.state, 'unresponsive');
});

// ---------------------------------------------------------------- cw/play

test('play: ok, then done end, with arrival times', async () => {
  const { link, t, clock } = await connected((l) => (l.startsWith('put cw/play') ? null : undefined));
  const p = link.playCw('cq de oe1wkl');
  await clock.advance(30);
  assert.equal(t.lines[t.lines.length - 1], 'put cw/play/cq de oe1wkl');
  t.send(OK);
  assert.deepEqual(await p.started, { t: 30 });
  await clock.advance(4000);
  t.send(DONE_END);
  assert.deepEqual(await p.done, { reason: 'end', t: 4030, late: false });
});

test('play: cw/stop sends ok, then done stopped', async () => {
  const { link, t, clock } = await connected((l) => (l === 'put cw/stop' ? OK + DONE_STOPPED : undefined));
  const p = link.playCw('paris paris');
  await p.started;
  await clock.advance(500);
  await link.stopCw();
  assert.equal((await p.done).reason, 'stopped');
});

test('play: a done with nothing playing is only an event', async () => {
  const { link, t } = await connected();
  const seen = record(link, ['cw']);
  t.send(DONE_STOPPED);                       // the operator's own memory, or a client before this page
  assert.equal(seen.length, 1);
});

test('play: a second play replaces the first; one done, for the second', async () => {
  const { link, t, clock } = await connected();
  const a = link.playCw('vvv');
  await a.started;
  const b = link.playCw('test');
  await b.started;
  assert.equal((await a.done).reason, 'replaced');
  t.send(DONE_END);
  assert.equal((await b.done).reason, 'end');
});

test('play: a done that arrives before the next play is accepted belongs to the first', async () => {
  const { link, t, clock } = await connected((l) => (l === 'put cw/play/test' ? DONE_END + OK : undefined));
  const a = link.playCw('vvv');
  await a.started;
  const b = link.playCw('test');
  await b.started;
  assert.equal((await a.done).reason, 'end');
  assert.equal(b.done.settled, undefined);
  let settled = false;
  b.done.then(() => { settled = true; });
  await clock.advance(0);
  assert.equal(settled, false);
});

test('play: refused outside the Keyer; done says so and never rejects', async () => {
  const { link } = await connected((l) => (l.startsWith('put cw/play') ? NOT_ACTIVE : undefined));
  const p = link.playCw('cq');
  await assert.rejects(p.started, /Keyer not active/);
  const d = await p.done;
  assert.equal(d.reason, 'refused');
  assert.match(d.error.message, /Keyer not active/);
});

test('play: a text with a line feed is refused, nothing is sent', async () => {
  const { link, t } = await connected();
  const n = t.lines.length;
  const p = link.playCw('cq\nput device/reset/defaults');
  assert.equal((await p.done).reason, 'refused');
  assert.equal(t.lines.length, n);
});

test('play: repeat uses cw/repeat', async () => {
  const { link, t } = await connected();
  await link.playCw('vvv', { repeat: true }).started;
  assert.equal(t.lines[t.lines.length - 1], 'put cw/repeat/vvv');
});

test('play: slashes and prosigns go through as typed', async () => {
  const { link, t } = await connected();
  await link.playCw('oe1wkl/p <sk> \\p e').started;
  assert.equal(t.lines[t.lines.length - 1], 'put cw/play/oe1wkl/p <sk> \\p e');
});

// ---------------------------------------------------------------- stall, loss, goodbye

test('link: a torn object is given up on its own timer', async () => {
  const { link, t, clock } = await connected();
  const seen = record(link, ['junk', 'trainer']);
  t.send('{"trainer":{"event":"res');
  await clock.advance(1499);
  assert.equal(seen.length, 0);
  await clock.advance(1);
  assert.equal(seen[0][0], 'junk');
  t.send(RESULT);
  assert.equal(seen[1][0], 'trainer');
});

test('link: the cable goes away: everything waiting is settled, then reopen and a new session', async () => {
  const { link, t, clock } = await connected((l) => (l === 'get kochlesson' ? null : undefined));
  let back;
  t.reopen = () => new Promise((r) => { back = r; });
  const seen = record(link, ['lost', 'reconnected', 'state']);
  const play = link.playCw('cq');
  await play.started;
  const cmd = link.command('get kochlesson');
  cmd.catch(() => {});
  await clock.advance(0);
  t.onClose(new Error('NetworkError: The device has been lost.'));
  await assert.rejects(cmd, /lost/);
  assert.equal((await play.done).reason, 'lost');
  assert.equal(link.state, 'lost');
  await assert.rejects(link.command('get kochlesson'), /not connected/);
  t.lines.length = 0;
  back();                                     // the Pocket came back on USB
  await clock.advance(0);
  assert.deepEqual(t.lines, ['put device/protocol/on', 'get capabilities']);
  assert.equal(link.state, 'ready');
  assert.deepEqual(seen.filter((s) => s[0] !== 'state').map((s) => s[0]), ['lost', 'reconnected']);
});

test('link: the device ends the session itself', async () => {
  const { link, t } = await connected();
  const seen = record(link, ['goodbye']);
  t.send(GOODBYE);
  assert.equal(seen.length, 1);
  assert.equal(link.state, 'ended');
  await assert.rejects(link.command('get kochlesson'), /not connected/);
});

test('disconnect: goodbye steps, then protocol off, then the transport closes', async () => {
  const { link, t, clock } = await connected();
  let serialOut = 4;
  link.addGoodbye(() => 'put config/Serial Output/' + serialOut);
  link.addGoodbye(() => null);
  link.addGoodbye(() => { throw new Error('a broken step must not stop the goodbye'); });
  serialOut = 2;                              // read at goodbye time
  t.lines.length = 0;
  const d = link.disconnect();
  await clock.advance(0);
  await d;
  assert.deepEqual(t.lines, ['put config/Serial Output/2', 'put device/protocol/off']);
  assert.ok(t.closed);
  assert.equal(link.state, 'closed');
  assert.equal(link.device, null);
});

test('disconnect: a silent device does not hold it up for long', async () => {
  const { link, t, clock } = await connected();
  link.addGoodbye(() => ['put config/Serial Output/2', 'put config/Interchar Spc/3']);
  t.silent = true;
  let finished = false;
  link.disconnect().then(() => { finished = true; });
  await clock.advance(3 * 1000 + 3 * 250);
  assert.ok(finished);
  assert.ok(t.closed);
});

test('pagehide: the goodbye goes out in one write, nothing awaited', async () => {
  const { link, t } = await connected();
  link.addGoodbye(() => 'put config/Serial Output/2');
  const handlers = {};
  const win = { addEventListener: (n, fn) => { handlers[n] = fn; }, removeEventListener: () => {} };
  link.installPageHide(win);
  const n = t.writes.length;
  handlers.pagehide();
  assert.equal(t.writes.length, n + 1);
  assert.equal(t.writes[n], 'put config/Serial Output/2\nput device/protocol/off\n');
});

test('pagehide: nothing is written when there is no session', async () => {
  const link = new Link({ clock: fakeClock() });
  assert.equal(link.pageHideGoodbye(), '');
});

test('listeners: one that throws does not break the link', async () => {
  const { link, t } = await connected();
  const orig = console.error;
  console.error = () => {};
  try {
    link.on('cw', () => { throw new Error('boom'); });
    const seen = record(link, ['cw']);
    t.send(DONE_END);
    assert.equal(seen.length, 1);
  } finally { console.error = orig; }
  assert.equal((await link.command('get kochlesson')).kochlesson.value, 5);
});

test('listeners: on() returns an unsubscribe', async () => {
  const { link, t } = await connected();
  let n = 0;
  const off = link.on('cw', () => n++);
  t.send(DONE_END);
  off();
  t.send(DONE_END);
  assert.equal(n, 1);
});
