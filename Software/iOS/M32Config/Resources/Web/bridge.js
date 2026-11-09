// bridge.js — teaches m32_config_tool.html to speak over CoreBluetooth.
//
// Injected by WebToolView into the PAGE content world at document-end, i.e.
// after the tool's own inline scripts have run. The tool itself is NOT patched:
// it stays byte-for-byte the file that is served from the web, so the browser
// version and this app never drift apart.
//
// How little has to change: the tool's whole USB surface is four globals.
//
//   openTransport()  -> opens the link             ... the one we replace
//   sendLine(t)      -> writer.write(t + '\n')     ... so we swap in a writer
//   doDisconnect()   -> closes writer/reader/port  ... so our writer needs close()
//   readLoop()       -> appends to readBuffer      ... so we append to it ourselves
//
// Plus two we wrap without changing what they do: waitForResponse() and
// sendAndParse() get longer timeouts (BLE is paced), and doDisconnect(linkGone)
// gets the native disconnect after it. Re-check all six names whenever the tool
// changes; the 2026-11 review (app 1.2) is in devdocs/ios-app/RELEASE_1.2.md.
//
// openTransport() is the tool's deliberate transport seam. We override it and
// nothing else about connecting, so every later change to doConnect() -- the
// handshake, the 1.4 capability query, whatever comes next -- reaches this app
// on its own. Do NOT go back to replacing doConnect(): that copy drifted once
// already and lost `await loadCapabilities()` without a symptom.

(function () {
  'use strict';

  if (window.__m32BridgeInstalled) return;
  if (typeof sendLine !== 'function' || typeof waitForResponse !== 'function'
      || typeof openTransport !== 'function') {
    console.error('bridge.js: this is not the M32 config tool, or it predates the '
                + 'openTransport() seam. Re-run sync-webtool.sh.');
    return;
  }
  window.__m32BridgeInstalled = true;

  var native = window.webkit.messageHandlers.m32;

  // Every reply from the device is paced by the firmware: at most 2 notification
  // chunks per poll of loop(), fewer than 4 in flight. A big GET (get configs,
  // get stats/log, a file download) therefore takes noticeably longer than over
  // USB, and the tool's built-in timeouts were measured on USB. Stretch them
  // rather than editing timeout numbers all over the tool.
  var BLE_TIMEOUT_FACTOR = 3;

  // ---------------------------------------------------------------- native RPC

  var nextCallId = 1;
  var pendingCalls = {};

  function callNative(cmd, args) {
    return new Promise(function (resolve, reject) {
      var id = nextCallId++;
      pendingCalls[id] = { resolve: resolve, reject: reject };
      var message = { id: id, cmd: cmd };
      if (args) for (var k in args) message[k] = args[k];
      native.postMessage(message);
    });
  }

  var decoder = new TextDecoder('utf-8');

  window.__m32Native = {
    reply: function (id, ok, payload) {
      var call = pendingCalls[id];
      if (!call) return;
      delete pendingCalls[id];
      if (ok) call.resolve(payload); else call.reject(new Error(payload));
    },

    // Bytes straight off the TX characteristic, base64-encoded. A notification
    // can end mid-UTF-8-sequence, and {stream:true} holds that tail back until
    // the next chunk completes it.
    rx: function (b64) {
      var binary = atob(b64);
      var bytes = new Uint8Array(binary.length);
      for (var i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
      var text = decoder.decode(bytes, { stream: true });
      if (!text) return;
      noteLinkEndNotice(text);
      readBuffer += text;
      if (typeof ttyProcessChunk === 'function') ttyProcessChunk(text);
    },

    log: function (message) { log(message); },

    // The link dropped underneath us (out of range, device switched off or
    // rebooted). Native only calls this for a drop we did not ask for.
    //
    // This is the Bluetooth counterpart of the tool's linkEnded(), which its USB
    // readLoop() calls and which never runs here (openTransport() is ours, so
    // that readLoop() never starts). linkEnded() itself is not reused because
    // its text is about a cable. doDisconnect(true) is the tool's "link already
    // gone" teardown: nothing is sent. The writer is cleared first anyway, so a
    // tool without that argument stays quiet too.
    //
    // linkClosing is the tool's "we expect the link to drop" flag (a factory
    // reset reboots the device and says so in its dialog): no notice then.
    linkLost: async function () {
      log('Bluetooth link lost.');
      var expected = (typeof linkClosing !== 'undefined') && linkClosing === true;
      var why = LINK_END_TEXT[linkEndReason] || LINK_END_TEXT.lost;
      linkEndReason = '';
      writer = null;
      await doDisconnect(true);
      if (!expected && typeof showLinkNotice === 'function') {
        showLinkNotice(why);
        log(why);
      }
    }
  };

  // Why the device is about to drop us, when it says so. Before it stops BLE
  // Serial the firmware sends one {"message":...} (MorseBleSerial::stopWithNotice):
  // "BLE serial off" when Bluetooth Use is changed away from BLE Serial — which
  // the Preferences tab can now do from this very app, since the tool lists
  // Bluetooth Use in its General group — and "BLE serial suspended: wireless
  // mode" when a WiFi mode takes the radio. Without this the drop looked like
  // "out of range". Watched on a short rolling tail, because a notification can
  // split the text, and because the tool may consume the object from readBuffer.
  var LINK_END_TEXT = {
    lost:     'Bluetooth connection lost. Check that the Morserino is switched on and nearby, '
            + 'then tap Connect again.',
    off:      'The Morserino turned BLE Serial off, so the connection ended: "Bluetooth Use" '
            + 'is no longer set to "BLE Serial". Set it back on the device to connect again.',
    wireless: 'The Morserino paused BLE Serial for a WiFi mode. Leave that mode on the device, '
            + 'then tap Connect again.'
  };
  var linkEndReason = '';
  var rxTail = '';
  function noteLinkEndNotice(text) {
    rxTail = (rxTail + text).slice(-160);
    if (rxTail.indexOf('BLE serial off') >= 0) linkEndReason = 'off';
    else if (rxTail.indexOf('BLE serial suspended') >= 0) linkEndReason = 'wireless';
    else return;
    rxTail = '';
  }

  // ------------------------------------------------------------------ transport

  function installWriter() {
    writer = {
      write: function (text) { return callNative('write', { text: text }); },
      close: function () { return Promise.resolve(); }
    };
  }

  // Two layers, stretched exactly once each. sendAndParse() (since the snapshot
  // fix of 2026-11-09) keeps its OWN deadline, Date.now() + timeout, and calls
  // waitForResponse() again with what is left whenever it skips a stray reply
  // ({expect:key}). Stretching only waitForResponse() would leave that outer
  // deadline at USB length: a stray reply arriving after it would leave the real
  // answer 150 ms. So sendAndParse() gets the stretched timeout, and the calls
  // it makes itself pass through unchanged; every other caller of
  // waitForResponse() (WiFi, factory reset, time set ...) is stretched as before.
  // A counter, not a flag: nothing in the tool runs two of these at once (the
  // firmware needs one command in flight), but a counter stays right if it did.
  var insideSendAndParse = 0;

  var origWaitForResponse = waitForResponse;
  waitForResponse = function (timeoutMs) {
    var t = timeoutMs || 3000;
    return origWaitForResponse(insideSendAndParse > 0 ? t : t * BLE_TIMEOUT_FACTOR);
  };

  if (typeof sendAndParse === 'function') {
    var origSendAndParse = sendAndParse;
    sendAndParse = async function (cmd, timeout, opts) {
      insideSendAndParse++;
      try {
        return await origSendAndParse(cmd, (timeout || 3000) * BLE_TIMEOUT_FACTOR, opts);
      } finally {
        insideSendAndParse--;
      }
    };
  }

  // Replaces ONLY the tool's transport-opening step. Everything after it in
  // doConnect() is the tool's own code, shared with the USB build.
  //
  // Contract (see openTransport() in the tool): leave `writer` usable by
  // sendLine(), get incoming text into readBuffer, return {label, noReplyHint}.
  // Throwing is how failure is reported -- doConnect()'s catch logs it and
  // tears down.
  openTransport = async function () {
    setStatus('Scanning...', 'working');
    var name = await callNative('connect');
    installWriter();
    readBuffer = '';
    rxTail = '';
    linkEndReason = '';
    return {
      label: 'Bluetooth link up: ' + name,
      noReplyHint: 'The device answered over Bluetooth but not the protocol handshake. '
                 + 'Check that "Bluetooth Use" is set to "BLE Serial" in the M32 '
                 + 'preferences, and that no other phone or computer is already '
                 + 'connected to it.'
    };
  };

  // Pass the arguments through: since 2026-11-04 the tool calls
  // doDisconnect(true) ("linkGone") when the link has already ended, so that
  // nothing is sent into it. Dropping the argument here turned that back into
  // an ordinary disconnect.
  var origDoDisconnect = doDisconnect;
  doDisconnect = async function () {
    try {
      await origDoDisconnect.apply(this, arguments);
    } finally {
      try { await callNative('disconnect'); } catch (e) { /* already down */ }
    }
  };

  // ------------------------------------------------------------------- saving

  // saveTextFile() builds a Blob and clicks an <a download> — every "save to
  // computer" feature in the tool (File Builder, Snapshot Export, and whatever
  // comes next) goes through this one function. WKWebView ignores that outright:
  // no file, no error, no prompt -- and the caller's own log line still says it
  // saved, which is what made this look like a mystery rather than a failure.
  // Overriding saveTextFile() itself, instead of each caller, means a future
  // caller needs no bridge.js change to work correctly here. Hand the text to
  // the app instead and let iOS put up a share sheet, which is where "save a
  // file" lives on a phone.
  if (typeof saveTextFile === 'function') {
    saveTextFile = function (name, text) {
      callNative('saveFile', { name: name, text: text })
        .then(function () {
          log('Saved "' + name + '" (' + text.length + ' bytes) — choose where to keep it');
        })
        .catch(function (e) {
          log('Could not save "' + name + '": ' + e.message);
        });
    };
    var saveBtn = document.querySelector('[onclick="fbDownload()"]');
    if (saveBtn) saveBtn.textContent = 'Save / Share…';   // there is no "computer" here
  }

  // ------------------------------------------------------------------ cosmetics

  function retitleForBluetooth() {
    // The tool disables Connect when navigator.serial is missing — which it
    // always is in a WKWebView. Undo that: our transport is not Web Serial.
    var connectBtn = document.getElementById('connectBtn');
    if (connectBtn) connectBtn.disabled = false;

    var sub = document.querySelector('.banner-sub');
    if (sub) sub.textContent = 'Morserino-32 · Device Management via Bluetooth LE';

    // "USB · Chrome/Edge/Firefox" is true of the browser build and meaningless here.
    var hint = document.querySelector('.conn-hint');
    if (hint) hint.textContent = 'Bluetooth LE';

    // The page decided at load time that this browser has no Web Serial and
    // said so in the log. Correct, and irrelevant: this app does not use it.
    // Drop the line rather than leave a "not supported" notice sitting in the
    // log of an app that works.
    var logEl = document.getElementById('log');
    if (logEl && /Web ?Serial/i.test(logEl.textContent)) {
      logEl.textContent = logEl.textContent.split('\n')
        .filter(function (line) { return !/Web ?Serial/i.test(line); }).join('\n');
    }

    var placeholder = document.getElementById('connPlaceholder');
    if (placeholder) {
      placeholder.textContent =
        'Switch the Morserino-32 on, set "Bluetooth Use" to "BLE Serial", then tap Connect. '
      + 'New to this? See the Help tab.';
    }
  }

  retitleForBluetooth();
  // Belt and braces: if anything in the page re-disables the button after us.
  setTimeout(retitleForBluetooth, 0);

  log('Bluetooth transport ready — tap Connect to scan for your Morserino-32.');
})();
