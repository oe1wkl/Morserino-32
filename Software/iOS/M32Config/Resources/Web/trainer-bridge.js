// trainer-bridge.js — lets m32_trainer.html talk to the M32 over CoreBluetooth.
//
// Injected by WebToolView into the PAGE content world at document START, i.e.
// before the trainer's own scripts run. The trainer is NOT patched: it stays
// byte-for-byte the file served from the web. Unlike the Config Tool (whose
// globals bridge.js has to replace after the fact) the trainer has a host seam
// of its own: it looks for window.M32Host and, when it finds it, uses
//
//   openTransport()   instead of Web Serial; resolves with a transport object
//                     as m32_trainer_link.js defines it (write/close/onData/
//                     onClose, maxLine, label)
//   linkOptions       for its Link: {timeScale: 3}, because BLE is paced
//   saveFile()        instead of an <a download>, which a web view ignores
//   texts             for the [data-host] parts of the page that speak of USB
//                     cables and browsers
//   pageReady(api)    called by the page with {busy, handOver, takeLink}: the
//                     app's Bluetooth link follows the tab on screen, and these
//                     let it go and take it back (see "hand-over" below)
//
// The contract is documented at "---- host" in m32_trainer.html and at
// "A transport is the seam" in m32_trainer_link.js. Re-check both whenever the
// trainer changes.

(function () {
  'use strict';

  if (window.M32Host) return;
  var native = window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.m32;
  if (!native) return;

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

  // ----------------------------------------------------------------- transport

  // The open transport, or null. Bytes arriving with none open are dropped.
  var current = null;
  var decoder = null;

  // Why the device is about to drop us, when it says so (as in bridge.js): before
  // it stops BLE Serial the firmware sends one {"message":...}. Watched on a short
  // rolling tail, because a notification can split the text.
  var LINK_END_TEXT = {
    lost:     'The Bluetooth connection to the M32 is gone. Check that the Morserino is switched on '
            + 'and nearby, then press Connect again.',
    off:      'The Morserino turned BLE Serial off, so the connection ended: "Bluetooth Use" is no '
            + 'longer set to "BLE Serial". Set it back on the device to connect again.',
    wireless: 'The Morserino paused BLE Serial for a WiFi mode. Leave that mode on the device, then '
            + 'press Connect again.'
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

  window.__m32Native = {
    reply: function (id, ok, payload) {
      var call = pendingCalls[id];
      if (!call) return;
      delete pendingCalls[id];
      if (ok) call.resolve(payload); else call.reject(new Error(payload));
    },

    // Bytes straight off the TX characteristic, base64-encoded. {stream:true}
    // holds back a UTF-8 sequence split across two notifications.
    rx: function (b64) {
      if (!current || !decoder) return;
      var binary = atob(b64);
      var bytes = new Uint8Array(binary.length);
      for (var i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
      var text = decoder.decode(bytes, { stream: true });
      if (!text) return;
      noteLinkEndNotice(text);
      if (current.onData) current.onData(text);
    },

    log: function (message) { console.info('ble: ' + message); },

    // The link dropped underneath us (out of range, switched off, restarted).
    // Native calls this only for a drop we did not ask for. The trainer sees a
    // transport without reopen() and offers Connect again, showing err.hint.
    linkLost: function () {
      var t = current;
      current = null;
      var err = new Error('Bluetooth link lost');
      err.hint = LINK_END_TEXT[linkEndReason] || LINK_END_TEXT.lost;
      linkEndReason = '';
      if (t && t.onClose) t.onClose(err);
    }
  };

  async function openTransport() {
    var name = await callNative('connect');
    decoder = new TextDecoder('utf-8');
    rxTail = '';
    linkEndReason = '';
    var t = {
      label: 'Bluetooth',
      maxLine: 400,              // the firmware's BLE line cap (m32_v6.ino, "BLE LINE TOO LONG")
      device: name,
      onData: null,
      onClose: null,
      write: function (text) { return callNative('write', { text: text }); },
      // must not call onClose (the link's contract). In a hand-over the Bluetooth
      // link stays up for the other tab: only this page lets go of it.
      close: function () {
        if (current === t) current = null;
        if (handingOver) return Promise.resolve();
        return callNative('disconnect').catch(function () { /* already down */ });
      }
    };
    current = t;
    return t;
  }

  // ------------------------------------------------------------------ hand-over
  //
  // The app moves the one Bluetooth link to the tab on screen (WebToolView.swift,
  // LinkHandover.swift). It calls __m32Native.handover() on the tab that holds the
  // link; the page finishes what is under way and lets go, keeping the M32's
  // session open, and we report "released" (or "handoverCancelled" if the app
  // called cancelHandover() first, because the user came back). On the other tab
  // it calls attach(), and the page connects as its Connect button does.

  var page = null;               // {busy, handOver, takeLink} from the page
  var handingOver = false;
  var cancelRequested = false;

  window.__m32Native.handover = function () {
    if (handingOver || !page) {
      if (!page) callNative('released').catch(function () {});   // no page yet: nothing to let go
      return;
    }
    handingOver = true;
    cancelRequested = false;
    page.handOver(function () { return cancelRequested; })
      .catch(function (e) { console.error('hand-over:', e); return true; })
      .then(function (released) {
        handingOver = false;
        callNative(released ? 'released' : 'handoverCancelled').catch(function () {});
      });
  };
  window.__m32Native.cancelHandover = function () { cancelRequested = true; };
  window.__m32Native.attach = function () { if (page) page.takeLink(); };

  // ---------------------------------------------------------------------- host

  var NO_REPLY = 'The M32 answered over Bluetooth but not the protocol handshake. Check that '
               + '"Bluetooth Use" is set to "BLE Serial" in the M32 preferences, and that no other '
               + 'phone or computer is already connected to it.';

  window.M32Host = {
    openTransport: openTransport,
    linkOptions: { timeScale: 3 },
    saveFile: function (name, text) { return callNative('saveFile', { name: name, text: text, wait: true }); },
    pageReady: function (api) {
      page = api;
      callNative('ready').catch(function () {});
    },
    texts: {
      needs: 'You need firmware 10 or newer (serial protocol 1.6), with <b>Bluetooth Use</b> set to '
           + '<b>BLE Serial</b> on the M32. Press <b>Connect</b> and allow the connection on the M32',
      history: 'stays in this app on this phone, nowhere else. Export it now and then (to Files, or '
             + 'mail it to yourself); import it here or in the M32 Trainer on a computer.',
      historyBox: 'Everything you practise is kept in this app only. Export it now and then — '
                + 'deleting the app loses it.',
      kept: 'Kept in this app.',
      keys: '<kbd>return</kbd> start, check, next · <b>Replay</b> (counts as help) and <b>Pause</b>: '
          + 'the buttons above',
      footer: 'M32 Trainer · the M32\'s settings are in the Config tab',
      handedOver: 'The Config tab has the connection now. It comes back here by itself when you return to this tab.',
      noReply: NO_REPLY
    }
  };
})();
