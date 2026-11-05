// A simulated Morserino for trying m32_trainer.html without a device. Paste it into the
// browser console (or inject it) on the trainer page BEFORE pressing Connect: it replaces
// M32Link.openWebSerial() with a transport that answers like protocol-1.6 firmware in the
// CW Keyer. `window.fakeM32` then lets you poke it:
//   fakeM32.paddle()       a paddle touch: stops a running cw/play ("done" "stopped")
//   fakeM32.key('kmr ')    keyed characters arriving in the echo, e.g. 'hw <err>' (a keyed answer)
//   fakeM32.leaveKeyer()   the operator long-presses back to the menu (a "menu" event)
//   fakeM32.enterKeyer()   ... and starts the CW Keyer there again (an "activate" ON event)
//   fakeM32.goodbye()      the M32 ends the session by itself ("end m32protocol")
//   fakeM32.unplug() / fakeM32.replug()   cable loss and return (the Pocket)
//   fakeM32.restart()      a restart behind an open port (the classic): silent until handshaken
//   fakeM32.msPerChar      sounding time per character (default 250)
(function () {
  'use strict';
  const st = { session: false, keyer: false, lesson: 5, speed: 20, playing: null, plugged: true, transport: null, msPerChar: 250, log: [] };
  const KOCH = 'mkrsuaptlowi.njef0yv,g5/q9zh38b?427c1d6x-='.split('').concat(['<sk>', '+', '<as>', '<kn>', '<ka>', '<ve>', '<bk>', '@', ':']);
  function send(obj) {
    const t = st.transport;
    if (t && t.onData && st.plugged) setTimeout(() => t.onData(JSON.stringify(obj)), 5);
  }
  function menuObj() { return { menu: { content: st.keyer ? 'CW Keyer' : 'CW Keyer', 'menu number': 1, executable: true, active: st.keyer } }; }
  function finish(reason) {
    if (!st.playing) return;
    clearTimeout(st.playing.timer);
    st.playing = null;
    send({ cw: { event: 'done', reason } });
  }
  function handle(line) {
    st.log.push(line);
    const l = line.toLowerCase();
    if (l === 'put device/protocol/on') st.session = true;
    else if (!st.session) return;                        // no session: every command is ignored
    if (l === 'put device/protocol/on') return send({ device: { hardware: 'M32 Pocket (simulated)', firmware: '10.0', protocol: '1.6', build: 'Nov 1 2026', edition: 'standard' } });
    if (l === 'put device/protocol/off') return send({ 'end m32protocol': { content: 'Goodbye!' } });
    if (l === 'get capabilities') return send({ capabilities: { protocol: '1.6', features: ['configs/details', 'trainer-events', 'cw/practice'] } });
    if (l === 'get kochlesson') return send({ kochlesson: { value: st.lesson, minimum: 1, maximum: 51, characters: KOCH } });
    if (l.startsWith('put kochlesson/')) { st.lesson = +l.split('/')[1]; return send({ ok: { content: 'OK' } }); }
    if (l === 'get control/speed') return send({ control: { name: 'speed', value: st.speed, minimum: 5, maximum: 60 } });
    if (l === 'get menu') return send(menuObj());
    if (l === 'get menus') return send({ menus: [{ content: 'CW Keyer', 'menu number': 1, executable: true }, { content: 'CW Generator/..', 'menu number': 2, executable: false }] });
    if (l === 'put menu/stop') { finish('stopped'); st.keyer = false; st.practice = st.spacing = st.display = null; send({ ok: { content: 'OK' } }); return setTimeout(() => send(menuObj()), 50); }
    if (l === 'put menu/start/1') { if (st.keyer) return send({ error: { content: 'NOT AT MENU - stop the running mode first' } }); st.keyer = true; return send({ ok: { content: 'OK' } }); }
    if (l.startsWith('put cw/play/')) {
      if (!st.keyer) return send({ error: { content: 'CW/PLAY: Keyer not active' } });
      const text = line.slice('put cw/play/'.length);
      if (st.playing) clearTimeout(st.playing.timer);          // replaces; one done follows, for the new text
      send({ ok: { content: 'OK' } });
      st.playing = { text, timer: setTimeout(() => finish('end'), 30 + st.msPerChar * text.length) };
      return;
    }
    if (l.startsWith('put cw/practice/')) { st.practice = l.split('/')[2]; return send({ ok: { content: 'OK' } }); }
    if (l.startsWith('put cw/display/')) { st.display = l.split('/')[2]; return send({ ok: { content: 'OK' } }); }
    if (l.startsWith('put cw/spacing/')) { st.spacing = l.split('/').slice(2).join('/'); return send({ ok: { content: 'OK' } }); }
    if (l === 'get config/interchar spc') return send({ config: { name: 'Interchar Spc', value: st.interchar || 3 } });
    if (l === 'put cw/stop') { send({ ok: { content: 'OK' } }); return finish('stopped'); }
    send({ error: { content: 'COMMAND NOT IN THE SIMULATOR: ' + line } });
  }
  function makeTransport() {
    let buf = '';
    const t = { label: 'USB (simulated)', maxLine: 1024, onData: null, onClose: null };
    t.write = (text) => {
      if (!st.plugged) return Promise.reject(new Error('port not open'));
      buf += text;
      let i;
      while ((i = buf.indexOf('\n')) >= 0) { const line = buf.slice(0, i).trim(); buf = buf.slice(i + 1); if (line) handle(line); }
      return Promise.resolve();
    };
    t.close = async () => { st.transport = null; };
    t.reopen = () => new Promise((resolve) => { st.onReplug = () => { st.transport = t; resolve(); }; });
    return t;
  }
  M32Link.openWebSerial = async () => { const t = makeTransport(); st.transport = t; return t; };
  window.fakeM32 = {
    state: st,
    get msPerChar() { return st.msPerChar; }, set msPerChar(v) { st.msPerChar = v; },
    paddle() { finish('stopped'); },
    key(text) { const t = st.transport; if (t && t.onData && st.plugged) t.onData(text); },    // characters from the paddle, as the echo sends them
    leaveKeyer() { finish('stopped'); st.keyer = false; st.practice = st.spacing = st.display = null; send({ menu: { content: 'CW Keyer', 'menu number': 1, executable: true, active: false } }); },
    enterKeyer() { st.keyer = true; send({ activate: { state: 'ON' } }); },
    goodbye() { finish('stopped'); send({ 'end m32protocol': { content: 'Goodbye!' } }); st.session = false; },
    unplug() { const t = st.transport; st.plugged = false; finish('stopped'); st.keyer = false; if (t && t.onClose) t.onClose(new Error('device lost')); },
    restart() { if (st.playing) clearTimeout(st.playing.timer); st.playing = null; st.keyer = false; st.session = false; },
    replug() { st.plugged = true; st.keyer = false; if (st.onReplug) { const f = st.onReplug; st.onReplug = null; f(); } }
  };
  return 'fake M32 installed';
})();
