/*
 * m32_trainer_link.js -- the M32 Trainer's link to a Morserino-32 (milestone M2).
 *
 * The bottom layer of the trainer (link -> engine -> page). It knows the M32 serial
 * protocol and nothing about training: it frames the byte stream, sends one command at a
 * time, routes the unsolicited objects to listeners, follows a cw/play to its
 * {"cw":{"event":"done"}}, recovers when the device or the cable goes away, and says
 * goodbye when the page closes. No DOM, so the same file runs under Node for the tests
 * (Software/tests/trainer/) and in the page as a plain <script>, where it defines the
 * global M32Link.
 *
 * Protocol reference: Documentation/Protocol Description/M32 Protocol.md, in particular
 * "Transports" (framing, the stall rule), "Consent on BLE", "Events for practice
 * programs". The transport contract copies the Config Tool's openTransport() seam
 * (Software/Utilities/m32_config_tool.html), so a BLE host can replace openWebSerial()
 * and nothing else.
 *
 * What the stream looks like: bare concatenated JSON objects, one top-level key each,
 * interleaved with the raw character echo (Serial Output) and, on the classic, debug and
 * boot text. The echo never contains a brace (no Morse character is one), so everything
 * outside braces is text and everything inside is JSON -- but only braces outside JSON
 * strings count, and a backslash escapes the next character inside one.
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.M32Link = factory();
}(typeof self !== 'undefined' ? self : this, function () {
'use strict';

// Objects that are only ever unsolicited, so never taken as the reply to a GET whose
// reply key the link does not know.
const EVENT_ONLY = new Set(['cw', 'trainer', 'activate', 'message']);

// The reply key of each GET, by the first part of its path. A GET not listed here takes
// the first object that is not EVENT_ONLY; pass `expect` to be exact.
const GET_REPLY = {
  device: ['device'], capabilities: ['capabilities'],
  control: ['control'], controls: ['controls'], menu: ['menu'],
  config: ['config'], configs: ['configs', 'configdetails'],
  kochlesson: ['kochlesson'], customchars: ['customchars'], practicechars: ['practicechars'],
  player: ['player'], hardware: ['hardware'], battery: ['battery'],
  snapshots: ['snapshots'], snapshot: ['snapshot'], resets: ['resets']
};

const USB_MAX_LINE = 1024;        // bytes, without the line feed (protocol: "Maximum command length")
const HANDSHAKE_MS = 2000;        // as the Config Tool
const CONSENT_MS = 25000;         // BLE: a person is answering on the device (~20 s, then it refuses)
const PROBE_MS = 1500;

function noop() {}

function defaultClock() {
  const perf = typeof performance !== 'undefined' ? performance : null;
  return {
    now: perf ? () => perf.now() : () => Date.now(),
    setTimeout: (fn, ms) => setTimeout(fn, ms),
    clearTimeout: (h) => clearTimeout(h)
  };
}

function utf8Length(s) {
  return new TextEncoder().encode(s).length;
}

function parseObject(raw) {
  try {
    const v = JSON.parse(raw);
    return v && typeof v === 'object' && !Array.isArray(v) ? v : null;
  } catch (e) {
    return null;
  }
}

// Index just past the brace that closes the object opening at s[p], or -1 if it does not
// close within s.
function closeOf(s, p) {
  let depth = 0, inStr = false, esc = false;
  for (let i = p; i < s.length; i++) {
    const c = s[i];
    if (esc) { esc = false; continue; }
    if (inStr) {
      if (c === '\\') esc = true;
      else if (c === '"') inStr = false;
      continue;
    }
    if (c === '"') inStr = true;
    else if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return i + 1;
  }
  return -1;
}

// ============================================================================
// FRAMER
// ============================================================================
// feed() takes text as it arrives and returns what it completed:
//   {kind:'object', obj, raw, t, late}   one JSON object
//   {kind:'text',   text, t, late}       echo (and any other text outside braces)
//   {kind:'junk',   raw, why, t}         bytes that could not be framed
// `t` is the clock time of the chunk that completed the item. Text is handed on at once,
// not held back; an object only when its closing brace has arrived.
//
// A dropped tail leaves an object open for good (protocol, "Framing after a drop"), so an
// open object that gets no byte for stallMs is given up: check() does that and must be
// called on a timer while `open` is true (the Link does). Giving up does not throw the
// bytes away blindly -- a whole object that arrived after the torn one is inside them --
// so the framer looks for the first "{" in them from which a complete object parses and
// resumes there; only what lies before it is junk. The same happens when a closed object
// does not parse (a stray "{" in debug text) or grows past maxObject. Items recovered this
// way carry `late: true`: their `t` is when they were recovered, not when they arrived.
class Framer {
  constructor(opts) {
    opts = opts || {};
    this.stallMs = opts.stallMs != null ? opts.stallMs : 1500;
    this.maxObject = opts.maxObject || 65536;
    this.lastByteAt = 0;
    this.reset();
  }

  reset() {
    this.buf = '';
    this.depth = 0;
    this.inStr = false;
    this.esc = false;
  }

  get open() { return this.depth > 0; }

  feed(chunk, now) {
    const out = [];
    if (chunk) {
      this.lastByteAt = now;
      this._run(String(chunk), now, out, 0);
    }
    return out;
  }

  check(now) {
    const out = [];
    while (this.depth > 0 && now - this.lastByteAt >= this.stallMs) {
      const raw = this.buf;
      this.reset();
      const p = this._resumeAt(raw, false);
      out.push({ kind: 'junk', raw: p < 0 ? raw : raw.slice(0, p), why: 'stalled', t: now });
      if (p >= 0) this._run(raw.slice(p), now, out, raw.length - p);
    }
    return out;
  }

  // The first "{" after the start of `raw` from which an object can be framed: a complete
  // one that parses, or -- when more bytes may still come (allowOpen) -- one still open at
  // the end. -1 if there is none.
  _resumeAt(raw, allowOpen) {
    for (let p = raw.indexOf('{', 1); p >= 0; p = raw.indexOf('{', p + 1)) {
      const end = closeOf(raw, p);
      if (end < 0 ? allowOpen : parseObject(raw.slice(p, end))) return p;
    }
    return -1;
  }

  // s[0..late) is recovered data (see the class comment).
  _run(s, now, out, late) {
    let i = 0;
    while (i < s.length) {
      if (this.depth === 0) {
        let j = s.indexOf('{', i);
        if (j < 0) j = s.length;
        if (j > i) pushText(out, s.slice(i, j), now, i < late);
        if (j === s.length) return;
        this.buf = '{';
        this.depth = 1;
        this.inStr = false;
        this.esc = false;
        i = j + 1;
        continue;
      }
      const start = i;
      let closed = false;
      for (; i < s.length; i++) {
        const c = s[i];
        if (this.esc) { this.esc = false; continue; }
        if (this.inStr) {
          if (c === '\\') this.esc = true;
          else if (c === '"') this.inStr = false;
          continue;
        }
        if (c === '"') this.inStr = true;
        else if (c === '{') this.depth++;
        else if (c === '}' && --this.depth === 0) { closed = true; i++; break; }
      }
      this.buf += s.slice(start, i);
      let why = null;
      if (closed) {
        const obj = parseObject(this.buf);
        if (obj) {
          out.push({ kind: 'object', obj: obj, raw: this.buf, t: now, late: i <= late });
          this.reset();
          continue;
        }
        why = 'unparsable';
      } else if (this.buf.length > this.maxObject) {
        why = 'oversize';
      } else {
        return;                                 // still open; wait for more
      }
      // salvage: resume at the first frameable "{" inside what we have, then go on with the rest
      const raw = this.buf, rest = s.slice(i);
      this.reset();
      const p = this._resumeAt(raw + rest, true);
      const cut = p < 0 || p > raw.length ? raw.length : p;
      out.push({ kind: 'junk', raw: raw.slice(0, cut), why: why, t: now });
      s = raw.slice(cut) + rest;
      late = Math.max(late - i, 0) + (raw.length - cut);
      i = 0;
    }
  }
}

function pushText(out, text, t, late) {
  const last = out[out.length - 1];
  if (last && last.kind === 'text' && last.late === late) last.text += text;
  else out.push({ kind: 'text', text: text, t: t, late: late });
}

// ============================================================================
// LINK
// ============================================================================
// A transport is the seam (the Config Tool's openTransport() contract, as an object):
//   write(text) -> Promise    send bytes; the link adds the "\n"
//   close()     -> Promise    tear down; must not call onClose
//   reopen()    -> Promise    optional: resolves when the same device is back (USB
//                             re-enumeration); rejects if close() is called meanwhile
//   maxLine                   longest command in bytes (USB 1024, BLE 400)
//   label                     how to name the link to the user
// and the link sets on it:
//   onData(text)              incoming text, already UTF-8 decoded
//   onClose(err)              the link went away without close()
//
// Events (link.on(name, fn); returns an unsubscribe function):
//   'text'   (item)            echo text: {text, t, late}
//   'junk'   (item)            unframeable bytes: {raw, why, t}
//   'object' (obj, meta)       every object, replies included; meta {key, t, late, raw, reply, command}
//   '<key>'  (value, meta)     each unsolicited object by its key: 'cw', 'trainer', 'menu',
//                              'control', 'config', 'activate', 'message', 'error', ...;
//                              value is the object under the key ({event, reason} for 'cw')
//   'state'  (state, old)      closed | connecting | ready | unresponsive | lost | ended | closing
//   'confirm' (message)        BLE: the operator must allow the session on the device (FN)
//   'resync'  (device)         the session was re-handshaken after a command went unanswered;
//                              the device may have restarted, so re-read what you rely on
//   'unresponsive' (err)       ... and the re-handshake went unanswered too
//   'lost'    (err)            the transport went away (cable, Pocket restart)
//   'reconnected' (device)     ... and is back, with a new session
//   'goodbye' ()               the device ended the session by itself
//   'closed'  ()               disconnect() has finished
class Link {
  constructor(opts) {
    opts = opts || {};
    this.clock = opts.clock || defaultClock();
    this.framer = new Framer({ stallMs: opts.stallMs, maxObject: opts.maxObject });
    this.timeoutMs = opts.timeoutMs || 3000;
    this.graceMs = opts.graceMs != null ? opts.graceMs : 250;
    this.autoReconnect = opts.autoReconnect !== false;
    this.state = 'closed';
    this.transport = null;
    this.device = null;
    this.capabilities = null;
    this._listeners = new Map();
    this._queue = [];
    this._inflight = null;
    this._hold = null;          // a timed-out command whose late reply must not be taken for the next one's
    this._play = null;          // the cw/play the next {"cw":{"event":"done"}} belongs to
    this._goodbyeSteps = [];
    this._stallTimer = null;
  }

  // ---- events --------------------------------------------------------------

  on(name, fn) {
    if (!this._listeners.has(name)) this._listeners.set(name, []);
    this._listeners.get(name).push(fn);
    return () => this.off(name, fn);
  }

  off(name, fn) {
    const l = this._listeners.get(name);
    if (l) { const i = l.indexOf(fn); if (i >= 0) l.splice(i, 1); }
  }

  _emit(name, a, b) {
    const l = this._listeners.get(name);
    if (!l) return;
    for (const fn of l.slice()) {
      try { fn(a, b); }
      catch (e) { if (typeof console !== 'undefined') console.error('M32Link listener for "' + name + '":', e); }
    }
  }

  _setState(s) {
    if (s === this.state) return;
    const old = this.state;
    this.state = s;
    this._emit('state', s, old);
  }

  // ---- session -------------------------------------------------------------

  // Opens the session on an open transport; resolves with the device object. On failure
  // the transport is closed and the error says why (a device refusal carries deviceError).
  async connect(transport) {
    if (this.state !== 'closed') throw new Error('M32Link: already connected');
    this._attach(transport);
    try {
      return await this._handshake();
    } catch (e) {
      await this._close();
      if (e.timeout) {
        const err = new Error('No M32 response on this link. Is this the right port, is the M32 switched on, ' +
                              'and is no other program (a serial monitor) holding the port?');
        err.timeout = true;
        throw err;
      }
      throw e;
    }
  }

  // Re-handshake on demand, e.g. from 'unresponsive' (a button in the page).
  async reconnect() {
    if (!this.transport) throw new Error('M32Link: not connected');
    const before = this.state === 'lost' ? 'lost' : 'unresponsive';
    let device;
    try { device = await this._handshake(); }
    catch (e) {
      if (this.state === 'connecting') this._setState(before);
      throw e;
    }
    this._emit('reconnected', device);
    return device;
  }

  hasFeature(name) {
    if (!this.capabilities || !Array.isArray(this.capabilities.features)) return null;
    return this.capabilities.features.indexOf(name) >= 0;
  }

  // Ends the session: the goodbye steps (see addGoodbye) as ordinary commands, then
  // `put device/protocol/off`, then the transport is closed. Never rejects.
  async disconnect() {
    if (this.state === 'closed' || this.state === 'closing') return;
    if (this.state === 'ready' || this.state === 'unresponsive') {
      this._setState('ready');                  // let the steps through even when unresponsive
      for (const line of this._goodbyeLines())
        await this._command(line, { timeout: 1000, noRecover: true }).catch(noop);
      await this._command('put device/protocol/off',
                          { expect: ['end m32protocol'], timeout: 1000, noRecover: true }).catch(noop);
    }
    await this._close();
  }

  // fn() returns a command line or an array of them (or nothing), to be sent before the
  // session ends -- on disconnect() and, best effort, on pagehide. The trainer uses this to
  // put back the Serial Output setting it changed. Called at goodbye time, so it can send
  // what is true then. Returns a function that removes the step.
  addGoodbye(fn) {
    this._goodbyeSteps.push(fn);
    return () => { const i = this._goodbyeSteps.indexOf(fn); if (i >= 0) this._goodbyeSteps.splice(i, 1); };
  }

  // The page is going away: write the goodbye steps and `put device/protocol/off` in one
  // write, without waiting for anything (nothing may be awaited in pagehide). Best effort:
  // the bytes can still be lost. Without it the M32 keeps the session, and with it the
  // auto-power-off stays suspended until the next restart (the classic cannot see a USB
  // host go away). Returns what was written.
  pageHideGoodbye() {
    if (!this.transport || ['ready', 'unresponsive', 'connecting'].indexOf(this.state) < 0) return '';
    const text = this._goodbyeLines().concat(['put device/protocol/off']).map((l) => l + '\n').join('');
    try {
      const p = this.transport.write(text);
      if (p && p.catch) p.catch(noop);
    } catch (e) { /* best effort */ }
    return text;
  }

  installPageHide(win) {
    const handler = () => this.pageHideGoodbye();
    win.addEventListener('pagehide', handler);
    return () => win.removeEventListener('pagehide', handler);
  }

  _goodbyeLines() {
    const lines = [];
    for (const fn of this._goodbyeSteps) {
      let v;
      try { v = fn(); } catch (e) { continue; }
      for (const line of [].concat(v == null ? [] : v))
        if (!lineProblem(line, this._maxLine())) lines.push(line);
    }
    return lines;
  }

  _attach(t) {
    this.transport = t;
    t.onData = (chunk) => this._receive(t, chunk);
    t.onClose = (err) => this._transportClosed(t, err);
  }

  async _handshake() {
    this._setState('connecting');
    this.framer.reset();                        // a stray "{" before it must not swallow the reply
    const { reply } = await this._command('put device/protocol/on', {
      expect: ['device'], timeout: HANDSHAKE_MS, internal: true, noRecover: true,
      // BLE consent: the reply comes after a person has reacted on the device
      watch: (obj, entry) => {
        if (obj.message && /CONFIRM ON DEVICE/i.test(obj.message.content || '')) {
          this._emit('confirm', obj.message);
          entry.extend(CONSENT_MS);
        }
      }
    });
    this.device = reply.device;
    try {
      const { reply } = await this._command('get capabilities',
                                            { expect: ['capabilities'], internal: true, noRecover: true });
      this.capabilities = reply.capabilities;
    } catch (e) {
      this.capabilities = null;                 // firmware before protocol 1.4
    }
    this._setState('ready');
    this._pump();
    return this.device;
  }

  async _close() {
    this._setState('closing');
    this._fail(new Error('Disconnected'));
    const t = this.transport;
    this.transport = null;
    if (t) {
      t.onData = null;
      t.onClose = null;
      try { await t.close(); } catch (e) { /* already gone */ }
    }
    this.device = null;
    this.capabilities = null;
    this._setState('closed');
    this._emit('closed');
  }

  _transportClosed(t, err) {
    if (t !== this.transport || this.state === 'closing' || this.state === 'closed') return;
    this._setState('lost');
    this._fail(new Error('The link to the M32 was lost'));
    this._emit('lost', err);
    if (!this.autoReconnect || typeof t.reopen !== 'function') return;
    t.reopen()
      .then(() => this._handshake())
      .then((device) => this._emit('reconnected', device),
            () => { if (this.transport === t && this.state !== 'closing' && this.state !== 'closed') this._setState('lost'); });
  }

  // Everything waiting is rejected (or, for a cw/play, settled as 'lost').
  _fail(err) {
    const pending = (this._inflight ? [this._inflight] : []).concat(this._queue);
    this._queue = [];
    this._inflight = null;
    this._hold = null;
    for (const e of pending) { this.clock.clearTimeout(e.timer); e.reject(err); }
    this.framer.reset();
    if (this._stallTimer) { this.clock.clearTimeout(this._stallTimer); this._stallTimer = null; }
    if (this._play) { const p = this._play; this._play = null; p._settle({ reason: 'lost', t: this.clock.now() }); }
  }

  _maxLine() {
    return (this.transport && this.transport.maxLine) || USB_MAX_LINE;
  }

  // ---- commands ------------------------------------------------------------

  // Sends one command line (no "\n"); resolves with the reply object. One command is in
  // flight at a time; the rest wait in order. An {"error":...} reply rejects, with the
  // device's object in err.deviceError; no reply within opts.timeout rejects with
  // err.timeout -- and the link then re-handshakes once before it sends anything else
  // (event 'resync', or 'unresponsive' when that fails too).
  //   opts.expect   reply keys, e.g. ['kochlesson']; default from the command (GET_REPLY,
  //                 'ok' for a PUT); "error" always counts as the reply
  //   opts.timeout  ms (default 3000)
  //   opts.withMeta resolve with {reply, meta} instead, meta.t being the arrival time
  command(line, opts) {
    return this._command(line, Object.assign({}, opts, { internal: false }))
      .then((r) => (opts && opts.withMeta ? r : r.reply));
  }

  _command(line, opts) {
    opts = opts || {};
    return new Promise((resolve, reject) => {
      const problem = lineProblem(line, this._maxLine());
      if (problem) return reject(new TypeError('M32Link: ' + problem + ': ' + JSON.stringify(String(line).slice(0, 60))));
      if (!opts.internal && ['ready', 'connecting'].indexOf(this.state) < 0)
        return reject(new Error('M32Link: not connected (' + this.state + ')'));
      const entry = {
        line: line,
        expect: opts.expect || defaultExpect(line),
        timeout: opts.timeout || this.timeoutMs,
        internal: !!opts.internal,
        noRecover: !!opts.noRecover,
        watch: opts.watch || null,
        resolve: resolve, reject: reject, timer: null,
        extend: (ms) => {
          this.clock.clearTimeout(entry.timer);
          entry.timer = this.clock.setTimeout(() => this._timeout(entry), ms);
        }
      };
      if (entry.internal) this._queue.unshift(entry);
      else this._queue.push(entry);
      this._pump();
    });
  }

  _pump() {
    if (this._inflight || this._hold || !this.transport) return;
    const ready = this.state === 'ready';
    const i = this._queue.findIndex((e) => e.internal || ready);
    if (i < 0) return;
    const e = this._queue.splice(i, 1)[0];
    this._inflight = e;
    e.timer = this.clock.setTimeout(() => this._timeout(e), e.timeout);
    let p;
    try { p = this.transport.write(e.line + '\n'); }
    catch (err) { p = Promise.reject(err); }
    Promise.resolve(p).catch((err) => { if (this._inflight === e) this._settle(e, err); });
  }

  _settle(e, err, reply, meta) {
    this.clock.clearTimeout(e.timer);
    if (this._inflight === e) this._inflight = null;
    if (err) e.reject(err);
    else e.resolve({ reply: reply, meta: meta });
    this._pump();
  }

  _timeout(e) {
    if (this._inflight !== e) return;
    this._inflight = null;
    const err = new Error('M32Link: no reply to "' + e.line + '"');
    err.timeout = true;
    e.reject(err);
    // A reply that is merely late must not be taken for the next command's: hold the queue
    // for graceMs and swallow it. Then make sure there still is a session -- a device that
    // restarted (the classic does so without dropping USB) ignores every command silently
    // until it is handshaken again.
    this._hold = { expect: e.expect };
    this.clock.setTimeout(() => {
      this._hold = null;
      if (!e.noRecover && this.state === 'ready') this._probe();
      else this._pump();
    }, this.graceMs);
  }

  _probe() {
    this._command('put device/protocol/on', { expect: ['device'], timeout: PROBE_MS, internal: true, noRecover: true })
      .then(({ reply }) => {
        this.device = reply.device;
        this._emit('resync', this.device);
      }, (err) => {
        if (this.state !== 'ready') return;
        this._setState('unresponsive');
        const queued = this._queue;
        this._queue = [];
        for (const q of queued) q.reject(new Error('M32Link: the M32 is not answering'));
        if (this._play) { const p = this._play; this._play = null; p._settle({ reason: 'lost', t: this.clock.now() }); }
        this._emit('unresponsive', err);
      });
  }

  // ---- cw/play -------------------------------------------------------------

  // Has the M32 sound `text` (it must be in the CW Keyer). Returns
  //   {started: Promise<{t}>, done: Promise<{reason, t, late}>}
  // `started` resolves on the "ok" (t = its arrival) and rejects if the device refuses
  // ("CW/PLAY: Keyer not active"). `done` never rejects; reason is
  //   'end' | 'stopped'   from the device's {"cw":{"event":"done"}}, t = its arrival
  //   'replaced'          a later playCw() was accepted first (the device owes one "done", for that one)
  //   'refused'           the device did not accept this one (also has `error`)
  //   'lost'              the link went away
  // `done` has no timeout of its own: a caller that knows how long the text sounds can race it.
  // opts.repeat uses cw/repeat, which only ever ends 'stopped'.
  playCw(text, opts) {
    const play = { text: text, settled: false };
    play.done = new Promise((resolve) => {
      play._settle = (v) => { if (!play.settled) { play.settled = true; resolve(v); } };
    });
    const verb = opts && opts.repeat ? 'put cw/repeat/' : 'put cw/play/';
    play.started = this._command(verb + text, { expect: ['ok'] }).then(({ meta }) => {
      if (this._play && this._play !== play) this._play._settle({ reason: 'replaced', t: meta.t });
      this._play = play;
      return { t: meta.t };
    }, (err) => {
      play._settle({ reason: 'refused', error: err, t: this.clock.now() });
      throw err;
    });
    play.started.catch(noop);                   // a caller that only awaits `done` gets no unhandled rejection
    return { started: play.started, done: play.done };
  }

  // The running cw/play ends with {"cw":{"event":"done","reason":"stopped"}} after the "ok".
  stopCw() {
    return this.command('put cw/stop', { expect: ['ok'] });
  }

  // ---- incoming ------------------------------------------------------------

  _receive(t, chunk) {
    if (t !== this.transport || this.state === 'closing' || this.state === 'closed') return;
    this._dispatch(this.framer.feed(chunk, this.clock.now()));
    this._armStall();
  }

  _armStall() {
    if (this._stallTimer) { this.clock.clearTimeout(this._stallTimer); this._stallTimer = null; }
    if (!this.framer.open) return;
    const wait = Math.max(0, this.framer.lastByteAt + this.framer.stallMs - this.clock.now());
    this._stallTimer = this.clock.setTimeout(() => {
      this._stallTimer = null;
      this._dispatch(this.framer.check(this.clock.now()));
      this._armStall();
    }, wait);
  }

  _dispatch(items) {
    for (const it of items) {
      if (it.kind === 'text') this._emit('text', it);
      else if (it.kind === 'junk') this._emit('junk', it);
      else this._onObject(it);
    }
  }

  _onObject(it) {
    const obj = it.obj, key = Object.keys(obj)[0];
    const meta = { key: key, t: it.t, late: it.late, raw: it.raw, reply: false, command: null };
    const e = this._inflight;
    if (e && matches(e.expect, key)) {
      meta.reply = true;
      meta.command = e.line;
      this._emit('object', obj, meta);
      if (key === 'error') this._settle(e, deviceError(obj.error, e.line));
      else this._settle(e, null, obj, meta);
      return;
    }
    if (this._hold && matches(this._hold.expect, key)) {
      meta.reply = true;                         // the late reply of a command that already timed out
      meta.command = 'late';
      this._emit('object', obj, meta);
      return;
    }
    if (e && e.watch) e.watch(obj, e);
    this._emit('object', obj, meta);
    if (key === 'cw' && obj.cw && obj.cw.event === 'done' && this._play) {
      const p = this._play;
      this._play = null;
      p._settle({ reason: obj.cw.reason, t: it.t, late: it.late });
    }
    this._emit(key, obj[key], meta);
    if (key === 'end m32protocol' && (this.state === 'ready' || this.state === 'unresponsive')) {
      this._setState('ended');
      this._fail(new Error('The M32 ended the session'));
      this._emit('goodbye');
    }
  }
}

function defaultExpect(line) {
  const m = /^\s*(\S+)\s+(.*)$/.exec(line);
  if (!m) return null;
  const verb = m[1].toLowerCase(), path = m[2].toLowerCase(), obj = path.split('/')[0];
  if (verb === 'put') {
    if (obj === 'control') return ['control'];
    if (path === 'device/protocol/on') return ['device'];
    if (path === 'device/protocol/off') return ['end m32protocol'];
    return ['ok'];
  }
  return GET_REPLY[obj] || null;
}

function matches(expect, key) {
  if (key === 'error') return true;
  if (!expect) return !EVENT_ONLY.has(key);
  return expect.indexOf(key) >= 0;
}

function deviceError(error, line) {
  const msg = (error && (error.content || error.name)) || 'unknown error';
  const err = new Error(msg);
  err.deviceError = error;
  err.command = line;
  return err;
}

// Why `line` cannot be sent as one command, or null. The device ends a command at "\n"
// and a longer line than maxLine is discarded with "LINE TOO LONG".
function lineProblem(line, maxLine) {
  if (typeof line !== 'string' || !line.trim()) return 'empty command';
  if (/[\x00-\x1f\x7f]/.test(line)) return 'control character in command';
  if (utf8Length(line) > maxLine) return 'command longer than ' + maxLine + ' bytes';
  return null;
}

// ============================================================================
// WEB SERIAL TRANSPORT
// ============================================================================
// The browser's side of the seam, as the Config Tool's openTransport() does it (115200
// baud, TextDecoderStream/TextEncoderStream). opts.port skips the port picker (a port
// from navigator.serial.getPorts()); opts.serial replaces navigator.serial.
//
// reopen(): a Pocket that restarts re-enumerates on USB, and Chrome reports it back with a
// "connect" event; the same device (by USB vendor/product id) is opened again without a
// picker. A classic restarts behind its USB bridge, so its port never closes -- the link's
// re-handshake after an unanswered command covers that case.
async function openWebSerial(opts) {
  opts = opts || {};
  const serial = opts.serial || (typeof navigator !== 'undefined' ? navigator.serial : null);
  if (!serial) throw new Error('This browser has no Web Serial (use Chrome, Edge, Opera, or Firefox 151 or newer, on a desktop computer)');
  const t = { label: 'USB', maxLine: USB_MAX_LINE, onData: null, onClose: null };
  let port = null, info = {}, io = null, closing = false, waiter = null;

  async function start(p) {
    port = p;
    info = (p.getInfo && p.getInfo()) || {};
    await p.open({ baudRate: 115200 });
    const abort = new AbortController();
    const dec = new TextDecoderStream();
    const readClosed = p.readable.pipeTo(dec.writable, { signal: abort.signal }).catch(noop);
    const reader = dec.readable.getReader();
    const enc = new TextEncoderStream();
    const writeClosed = enc.readable.pipeTo(p.writable).catch(noop);
    const writer = enc.writable.getWriter();
    io = { abort, readClosed, reader, writeClosed, writer };
    readLoop(io);
  }

  async function readLoop(cur) {
    let err = null;
    try {
      for (;;) {
        const r = await cur.reader.read();
        if (r.done) break;
        if (r.value && t.onData) t.onData(r.value);
      }
    } catch (e) { err = e; }
    if (closing || io !== cur) return;
    await teardown();
    if (t.onClose) t.onClose(err);
  }

  async function teardown() {
    const cur = io, p = port;
    io = null;
    if (cur) {
      try { await cur.reader.cancel(); cur.reader.releaseLock(); } catch (e) { /* gone */ }
      cur.abort.abort();
      await cur.readClosed;
      try { await cur.writer.close(); } catch (e) { /* gone */ }
      await cur.writeClosed;
    }
    if (p) { try { await p.close(); } catch (e) { /* gone */ } }
  }

  const same = (p) => {
    const i = (p.getInfo && p.getInfo()) || {};
    return info.usbVendorId == null || (i.usbVendorId === info.usbVendorId && i.usbProductId === info.usbProductId);
  };

  t.write = (text) => (io ? io.writer.write(text) : Promise.reject(new Error('port not open')));

  t.close = async () => {
    closing = true;
    if (waiter) waiter.cancel();
    await teardown();
  };

  t.reopen = () => new Promise((resolve, reject) => {
    let done = false, busy = false;
    const stop = () => { done = true; serial.removeEventListener('connect', onConnect); waiter = null; };
    // a port from getPorts() may be the stale object of the device that went away: if it
    // will not open, keep waiting for the "connect" event
    const tryPort = (p, fromList) => {
      if (done || busy) return;
      busy = true;
      start(p).then(() => { stop(); resolve(); },
                    (err) => { busy = false; if (!fromList) { stop(); reject(err); } });
    };
    const onConnect = (ev) => { if (ev.target && same(ev.target)) tryPort(ev.target, false); };
    serial.addEventListener('connect', onConnect);
    waiter = { cancel: () => { if (!done) { stop(); reject(new Error('closed')); } } };
    serial.getPorts().then((ports) => {           // it may be back already
      const p = ports.find((q) => same(q) && q.connected !== false && !q.readable);
      if (p) tryPort(p, true);
    }, noop);
  });

  await start(opts.port || await serial.requestPort(opts.filters ? { filters: opts.filters } : undefined));
  return t;
}

return {
  Framer: Framer,
  Link: Link,
  openWebSerial: openWebSerial,
  defaultExpect: defaultExpect,
  EVENT_ONLY: EVENT_ONLY
};
}));
