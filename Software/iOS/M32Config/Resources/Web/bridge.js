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
      readBuffer += text;
      if (typeof ttyProcessChunk === 'function') ttyProcessChunk(text);
    },

    log: function (message) { log(message); },

    // The link dropped underneath us. Clear the writer first so the tool's own
    // teardown does not try to send "put device/protocol/off" into a dead link.
    linkLost: function () {
      log('Bluetooth link lost.');
      writer = null;
      doDisconnect();
    }
  };

  // ------------------------------------------------------------------ transport

  function installWriter() {
    writer = {
      write: function (text) { return callNative('write', { text: text }); },
      close: function () { return Promise.resolve(); }
    };
  }

  var origWaitForResponse = waitForResponse;
  waitForResponse = function (timeoutMs) {
    return origWaitForResponse((timeoutMs || 3000) * BLE_TIMEOUT_FACTOR);
  };

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
    return {
      label: 'Bluetooth link up: ' + name,
      noReplyHint: 'The device answered over Bluetooth but not the protocol handshake. '
                 + 'Check that "Bluetooth Use" is set to "BLE Serial" in the M32 '
                 + 'preferences, and that no other phone or computer is already '
                 + 'connected to it.'
    };
  };

  var origDoDisconnect = doDisconnect;
  doDisconnect = async function () {
    try {
      await origDoDisconnect();
    } finally {
      try { await callNative('disconnect'); } catch (e) { /* already down */ }
    }
  };

  // ------------------------------------------------------------------ cosmetics

  function retitleForBluetooth() {
    // The tool disables Connect when navigator.serial is missing — which it
    // always is in a WKWebView. Undo that: our transport is not Web Serial.
    var connectBtn = document.getElementById('connectBtn');
    if (connectBtn) connectBtn.disabled = false;

    var sub = document.querySelector('.banner-sub');
    if (sub) sub.textContent = 'Morserino-32 · Device Management via Bluetooth LE';

    var placeholder = document.getElementById('connPlaceholder');
    if (placeholder) {
      placeholder.textContent =
        'Switch the Morserino-32 on, set "Bluetooth Use" to "BLE Serial", then tap Connect';
    }
  }

  retitleForBluetooth();
  // Belt and braces: if anything in the page re-disables the button after us.
  setTimeout(retitleForBluetooth, 0);

  log('Bluetooth transport ready — tap Connect to scan for your Morserino-32.');
})();
