/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2026  Willi Kraml, OE1WKL                                                                            ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

// K1EL WinKeyer emulation - see MorseWinKeyer.h and devdocs/winkeyer/DESIGN.md.
//
// Three parts:
//   - the byte parser (feed()), which consumes the parameters of EVERY WK2/WK3 command, also
//     the ones we ignore - a parameter read as a command desynchronises the stream for good;
//   - the input buffer, holding host text and buffered commands in arrival order;
//   - the sender (tick()), a non-blocking state machine driven from loop(), so serial input,
//     paddle break-in, the buttons and the TOT keep running while it sends (no delay() anywhere).
// Host CW goes through keyOut(), and from there through MorseOutput::pwmTone (hard rule 1).

#include "MorseWinKeyer.h"

#ifdef CONFIG_WINKEYER

#include "MorsePreferences.h"
#include "MorseOutput.h"
#include "MorseJSON.h"
#include "M32ProtocolOut.h"
#include "MorseVoice.h"
#include "ClickButton.h"     // HeldLineFilter
#ifdef REMOTE_WINKEYER
#include "MorseKipKeyer.h"   // Remote WinKeyer: the link that carries the keying (DESIGN.md §9.1)
#endif

// m32_v6.ino
extern morserinoMode morseState;
extern encoderMode encoderState;
extern boolean m32protocol;
extern uint16_t volt;
extern const byte pool[][2];      // the M32 Morse table (element patterns), indexed like CWchars[]
extern void keyOut(boolean on, boolean fromHere, int f, int volume);
extern void displayGeneratedMorse(FONT_ATTRIB style, const String& s);
extern void displayCWspeed();
extern void updateTopLine();
extern String cleanUpText(String text);
extern String encodeProSigns(String &input);
extern String codesToUtf8(const String& s);
extern int16_t batteryVoltage();
extern void armForeignUsbRxDetect();

namespace MorseWinKeyer {
  volatile bool portOwned = false;
#ifdef WK_NATIVE_USB
  bool topMenuArmed = false;
#endif
}

using namespace MorseWinKeyer;

///////////////////////////// state

static bool inMode = false;             // the WinKeyer menu mode is running
static bool sessionOpen = false;               // a host session is open (Host Open .. Host Close)
static bool autoEntered = false;        // Pocket: the session opened at the top menu (D1a)
static bool closeToMenu = false;        // ... and was closed by the host: go back to the menu
static bool autoOpenPending = false;    // Host Open seen at the top menu, mode not entered yet
static bool highBaud = false;           // classic: 9600 instead of 1200 (Set High Baud)
static bool inPrefs = false;            // the preferences menu is up: it can store a snapshot, so the
                                        // M32's own speed must stay in MorsePreferences::wpm meanwhile

// Remote WinKeyer (DESIGN.md §9.1): the keying goes to a remote rig over M32KIP. Host CW plays only while
// the link is up. Until the link has come up the host's text waits in the buffer (R6, so XOFF works) and is
// dropped with a break-in when the start ends; while the link is down, host text is dropped, and the moment
// it goes down counts as a break-in (R4).
static bool remoteMode = false;
enum RemoteLink : uint8_t { RL_START, RL_UP, RL_DOWN };
static RemoteLink remoteLink = RL_START;
static unsigned long hostKeyUpAt = 0;   // R5: the host's last key-up, for the BUSY hold
static unsigned long longMarkAt = 0;    // R3a: when a Tune / Key Buffered mark went down

// host-owned sending-side settings (D7) - session values, never written to NVS. The initial values
// are resetSessionSettings()'s: Get Values / Dump EEPROM can be asked for before any Host Open.
static uint8_t hostMode;                // mode register (0x0E); key mode, swap and autospace are ignored
static uint8_t hostWpm;                 // 0 = speed pot (= the M32's encoder speed, E1)
static uint8_t bufWpm;                  // buffered speed change in force, 0 = none
static uint8_t sidetoneByte;            // as sent; only the paddle-only bit (WK2) is used
static uint8_t weight = 50, ratio = 50, farnsWpm, firstExt, keyComp, lead, tail, setpoint = 50;
static uint8_t potMin = 5, potRange = 30;
static bool    potWindowSet;            // the host has sent Setup Speed Pot
static uint8_t pinCfg = 0x06, x1Mode, x2Mode;
static bool    wk3Mode;                 // Set WK3 Mode: sidetone formula, X1MODE layout, Load Defaults 9/15
static bool    wk2Status;               // Set WK2 Mode: status bit 3 is 0 (pushbutton tag) instead of KEYDOWN

static uint8_t potWpm;                  // the M32's own speed: the encoder, reported as the speed pot

// input buffer: text and buffered commands. A parallel tag byte keeps Backspace from eating
// half of a buffered command.
static const uint16_t BUF_SIZE = 128;   // as a real WinKeyer; XOFF above 2/3
enum : uint8_t { T_TEXT, T_CMD, T_PARAM };
static uint8_t  buf[BUF_SIZE], tag[BUF_SIZE];
static uint16_t bufHead = 0, bufCount = 0;

// output queue to the host (status, pot, echo, replies) - drained without blocking
static const uint16_t OUT_SIZE = 320;   // room for the 256-byte EEPROM dump
static uint8_t  outBuf[OUT_SIZE];
static uint16_t outHead = 0, outCount = 0;

// parser
static const uint16_t NO_CMD = 0xFFFF;
static uint16_t pCmd = NO_CMD;
static bool     pIsAdmin = false, pAdminNext = false;
static uint16_t pNeed = 0, pHave = 0;
static uint8_t  pPar[16];

// sender
enum SendState : uint8_t { S_IDLE, S_MARK, S_SPACE, S_TIMED };
static SendState sState = S_IDLE;
static uint32_t  sBits = 0;              // elements of the character in progress, MSB first, 1 = dah
static uint8_t   sLen = 0, sPos = 0;
static unsigned long sDeadline = 0;
static unsigned long sSpaceAfter = 0;    // the gap that follows the mark in progress
static bool      hostKeying = false;     // the sender holds the key down (sidetone/TX decisions)
static unsigned long lastKeyUp = 0;      // for the 1st extension
static uint8_t   pendEcho[2]; static uint8_t pendEchoN = 0;
static String    pendDisp;
static bool      paused = false;
static bool      tuneOn = false;   static unsigned long tuneDeadline = 0;
static bool      waitOn = false;         // Wait (WAIT status bit) - Key Buffered uses S_TIMED with the key down
static bool      breakin = false;  static unsigned long lastPaddle = 0;
static uint16_t  paddleElems = 0;  static KEYERSTATES prevKs = IDLE_STATE;
static uint8_t   lastStatus = 0;
static bool      paddleWordOpen = false; // paddle echo: a letter went out since the last space
static bool      tickServiced = false;   // tick() polled the port in this loop() pass - see serialEvent()

// filterFn(): FN held by a serial line rather than a finger (HeldLineFilter, ClickButton.h)
static HeldLineFilter fnFilter;
static unsigned long lastConnectMs = 0;  // last Echo Test / Host Open: a host is (re)connecting
static const unsigned long CONNECT_WINDOW_MS = 2000;

static const unsigned long TUNE_WATCHDOG_MS = 100000UL;   // K1EL: hard-coded, cannot be disabled
// keyOut() blocks KEYOUT_EDGE_MS (m32_v6.ino) on both edges before the key line moves. As in
// generateCW(): a mark's deadline is set BEFORE the key goes down, so the waits cancel over the
// mark, and every gap that ends in a key-down is shortened by it.
extern const int KEYOUT_EDGE_MS;

///////////////////////////// small helpers

static void outPut(uint8_t b) {
  if (outCount >= OUT_SIZE) return;                 // a host that does not read loses bytes, never us
  outBuf[(outHead + outCount) % OUT_SIZE] = b;
  ++outCount;
}

#ifdef WK_NATIVE_USB
// The Pocket's HWCDC sometimes leaves written bytes in its TX ring with the interrupt that drains it switched
// off, until the host next sends something. Replies are flushed (replyNow()), but a logger that sent nothing
// never saw the unsolicited ones - above all the idle status at the end of a message: 7 of 40 withheld on the
// bench with WiFi running (Remote WinKeyer), until the next host write. flush() cures it, but always waits at
// least a millisecond, which is not to be spent on every byte of the keying path. So it is spent only on bytes
// that have stayed in the ring for STUCK_MS: an empty ring costs one availableForWrite() per pass.
static const unsigned long STUCK_MS = 10;
static int txIdleFree = 0;                  // free ring space with nothing pending: the largest value seen
static unsigned long txPendingSince = 0;    // 0 = the ring had drained when last looked at

static void unstickTx() {
  int f = Serial.availableForWrite();
  if (f > txIdleFree) txIdleFree = f;
  if (f >= txIdleFree) { txPendingSince = 0; return; }
  unsigned long now = millis();
  if (!txPendingSince) { txPendingSince = now ? now : 1; return; }
  if (now - txPendingSince >= STUCK_MS) {
    Serial.flush();                         // re-arms the drain interrupt
    txPendingSince = 0;
  }
}
#endif

static void pump() {
  while (outCount) {
    if (Serial.availableForWrite() <= 0)            // never block the keying path on a slow port
      break;
    Serial.write(outBuf[outHead]);
    outHead = (outHead + 1) % OUT_SIZE;
    --outCount;
  }
#ifdef WK_NATIVE_USB
  unstickTx();
#endif
}

// A host that waits for a reply (CQRLOG gives the revision byte 50 ms) gets it now. On the
// Pocket's HWCDC a written byte can sit in the TX ring until the next traffic unless flushed
// (devdocs: HWCDC withheld replies); on the classic's UART flush() would wait out the 1200-baud
// transmission, so there it is left to the UART.
static void replyNow() {
  pump();
#ifdef WK_NATIVE_USB
  Serial.flush();
#endif
}

static uint8_t clampWpm(int w) {
  return (uint8_t) constrain(w, (int) MorsePreferences::wpmMin, (int) MorsePreferences::wpmMax);
}

static uint8_t potTop() { return potMin + _min(potRange, 31); }

// the speed the paddle and the screen follow: the host's, or the speed pot (= encoder) in pot mode
static uint8_t baseWpm() {
  if (hostWpm)
    return clampWpm(hostWpm);
  if (sessionOpen && potWindowSet)
    return clampWpm(constrain((int) potWpm, (int) potMin, (int) potTop()));
  return potWpm;
}

static uint8_t sendWpm() { return bufWpm ? clampWpm(bufWpm) : baseWpm(); }

static void announce(const char* s) {
#ifdef CONFIG_AUDIO_A11Y
  MorseVoice::announce(s);
#else
  (void) s;
#endif
}

// Make MorsePreferences::wpm (paddle keyer timing, top-bar WpM) follow the session's speed.
// It is put back to the M32's own speed before anything can persist it (beforePreferences(),
// closeSession(), leave()), and the keyer's 12-character speed write is off in this mode.
static void applySpeed() {
  if (inPrefs) return;                    // host bytes still arrive (serialEvent() in the menu's loop)
  uint8_t w = baseWpm();
  if (w != MorsePreferences::wpm) {
    MorsePreferences::wpm = w;
    updateTimings();
    if (inMode)
      displayCWspeed();
  }
}

static uint8_t potByte() {
  int v = (int) potWpm - (int) potMin;
  return 0x80 | (uint8_t) constrain(v, 0, (int) _min(potRange, 31));
}

///////////////////////////// buffer

static bool bufPut(uint8_t b, uint8_t t) {
  if (remoteMode && remoteLink == RL_DOWN)
    return true;                                    // R4: no link, nothing to send it on - taken and dropped
  if (bufCount >= BUF_SIZE) return false;
  buf[(bufHead + bufCount) % BUF_SIZE] = b;
  tag[(bufHead + bufCount) % BUF_SIZE] = t;
  ++bufCount;
  return true;
}

static uint8_t bufGet() {
  uint8_t b = buf[bufHead];
  bufHead = (bufHead + 1) % BUF_SIZE;
  --bufCount;
  return b;
}

static void bufClear() { bufHead = bufCount = 0; }

///////////////////////////// sender

// longMark: Tune or Key Buffered - a mark of seconds, not an element (Remote WinKeyer R3a)
static void keyDown(bool longMark = false) {
#ifdef CONFIG_AUDIO_A11Y
  if (MorseVoice::isSpeaking())          // CW on the air wins over speech, as for the paddle
    MorseVoice::stop();
#endif
#ifdef REMOTE_WINKEYER
  if (remoteMode) {
    MorseKipKeyer::setLongMark(longMark);   // before the edge is queued: the rig's straight-key limit applies
    if (longMark) longMarkAt = millis();
  }
#else
  (void) longMark;
#endif
  hostKeying = true;
  keyOut(true, true, MorseOutput::notes[MorsePreferences::pliste[posPitch].value], MorsePreferences::sidetoneVolume);
}

static void keyUp() {
  if (hostKeying) {
    keyOut(false, true, 0, 0);
    lastKeyUp = hostKeyUpAt = millis();
#ifdef REMOTE_WINKEYER
    if (remoteMode) MorseKipKeyer::setLongMark(false);
#endif
  }
  hostKeying = false;
}

// R3a: the rig cuts a mark at its key-down limit; end a long mark here too once the rig must have, so the
// host's status matches what is on the air. The limit arrives with the rig's STATS (0 = not known yet).
static bool longMarkCutByRig(unsigned long now) {
#ifdef REMOTE_WINKEYER
  if (!remoteMode || !hostKeying) return false;
  uint16_t limit = MorseKipKeyer::rigMaxKeydownMs();
  return limit && now - longMarkAt > (unsigned long) limit + MorseKipKeyer::playoutMs();
#else
  (void) now;
  return false;
#endif
}

// Cut whatever the host had going: the character in progress, a wait, a timed key-down, tune.
static void abortSending() {
  keyUp();
  sState = S_IDLE;
  sLen = sPos = 0;
  pendEchoN = 0;
  pendDisp = "";
  tuneOn = waitOn = false;
}

static void restoreBufferedSpeed() { bufWpm = 0; }

static void clearBuffer() {             // 0x0A and the M32 abort paths
  bufClear();
  abortSending();
  paused = false;
  restoreBufferedSpeed();
}

// The M32 Morse table supplies the element patterns (pool[] in m32_v6.ino); only three of
// K1EL's prosign mappings have no M32 character and are spelled out here.
static int poolSize() { static const int n = strlen(CWchars); return n; }   // one pool[] entry per CWchars byte

static bool patternOfPoolIndex(int idx, uint32_t& bits, uint8_t& n) {
  if (idx < 0 || idx >= poolSize()) return false;
  uint8_t len = pool[idx][1], mask = pool[idx][0];
  for (uint8_t j = 0; j < len && n < 31; ++j) {
    bits = (bits << 1) | ((mask & 0x80) ? 1 : 0);
    mask <<= 1;
    ++n;
  }
  return true;
}

static bool patternOfText(const char* els, uint32_t& bits, uint8_t& n) {
  for (; *els && n < 31; ++els, ++n)
    bits = (bits << 1) | (*els == '-' ? 1 : 0);
  return true;
}

static int poolIndex(char c) {
  const char* e = strchr(CWchars, c);
  return (e && c) ? (int)(e - CWchars) : -1;
}

// WinKeyer byte -> elements, appended to bits/n. K1EL's ASCII table (WK3 manual, table 17).
static bool appendPattern(uint8_t b, uint32_t& bits, uint8_t& n) {
  if (b >= 0x80)                                         // internal: an M32 code from a keyer memory
    return patternOfPoolIndex(b & 0x7F, bits, n);
  if (b >= 'A' && b <= 'Z') b += 32;                     // CWchars has lower-case letters
  if (wk3Mode && (x2Mode & 0x10) && b == '0') b = 't';   // X2MODE cut numbers
  if (wk3Mode && (x2Mode & 0x20) && b == '9') b = 'n';
  char c;
  switch (b) {
    case '<':  c = '+';    break;                        // AR
    case '>':  c = 'K';    break;                        // SK (M32 prosign code)
    case '[':  c = 'S';    break;                        // AS
    case '(': case ':': case ']':
               c = 'N';    break;                        // KN
    case '\\': c = '/';    break;                        // DN
    case ';':  c = '\xE4'; break;                        // AA - same code as the M32's ä
    case '"':  return patternOfText(".-..-.", bits, n);  // RR
    case '$':  return patternOfText("...-..-", bits, n); // SX
    case '\'': return patternOfText(".----.", bits, n);  // WG
    default:
      if ((b >= 'a' && b <= 'z') || (b >= '0' && b <= '9') || strchr(".,?/=-@+)", b))
        c = (char) b;
      else
        return false;                                    // ! # % & * and the rest: ignored
  }
  return patternOfPoolIndex(poolIndex(c), bits, n);
}

// What the scroll area shows for a host byte - the M32's own conventions (hard rule 4).
static String displayOf(uint8_t b) {
  if (b >= 0x80)
    return (b & 0x7F) < poolSize() ? codesToUtf8(cleanUpProSigns(String(CWchars[b & 0x7F]))) : String();
  switch (b) {
    case '<':  return "+";
    case '>':  return "<sk>";
    case '[':  return "<as>";
    case '(': case ':': case ']':
               return "<kn>";
    case '\\': return "/";
    case ';':  return "<aa>";
  }
  if (b >= 'A' && b <= 'Z') b += 32;
  return String((char) b);
}

struct WkTiming { long dit, dah, weightAdj, letterGap, wordExtra; };

static void timing(WkTiming& t) {
  uint8_t s = sendWpm();
  uint8_t c = s;
  bool farns = farnsWpm >= 10 && farnsWpm > s;           // Farnsworth is off at or above its speed
  if (farns) c = farnsWpm;
  t.dit = 1200L / c;
  t.dah = t.dit * 3L * ratio / 50L;                      // DAH/DIT = 3 * (ratio / 50)
  t.weightAdj = t.dit * ((long) weight - 50L) / 50L;     // added to every mark, taken from the gap after it
  long letter, word;
  if (farns) {                                           // PARIS: 31 units at character speed, 19 of spacing
    long ts = 60000L / s - 31L * t.dit;
    if (ts < 19L * t.dit) ts = 19L * t.dit;
    letter = ts * 3L / 19L;
    word   = ts * ((hostMode & 0x01) ? 6L : 7L) / 19L;    // contest spacing: one unit less
  } else {
    letter = 3L * t.dit;
    word   = ((hostMode & 0x01) ? 6L : 7L) * t.dit;
  }
  // Letterspace (X1MODE): 0..15 steps of 2 %, read here as a lengthening of the letter space.
  uint8_t ls = wk3Mode ? (x1Mode & 0x1F) : (x1Mode >> 4);
  if (ls > 15) ls = 15;
  letter += letter * 2L * ls / 100L;
  t.letterGap = letter;
  t.wordExtra = word > letter ? word - letter : 0;
}

static void startMark() {
  WkTiming t; timing(t);
  bool dah = (sBits >> (sLen - 1 - sPos)) & 1;
  long mark = (dah ? t.dah : t.dit) + t.weightAdj + keyComp;
  // 1st extension: the first element after the key has been up longer than the PTT tail time
  // (3 dits + tail x 10 ms) - K1EL uses the tail timer to guess the rig is back on receive.
  if (sPos == 0 && firstExt && millis() - lastKeyUp > (unsigned long)(3L * t.dit + 10L * tail))
    mark += firstExt;
  if (mark < 1) mark = 1;
  ++sPos;
  long gap = (sPos < sLen ? t.dit : t.letterGap) - t.weightAdj - keyComp - KEYOUT_EDGE_MS;
  sSpaceAfter = gap > 0 ? gap : 0;
  sDeadline = millis() + mark;
  sState = S_MARK;
  keyDown();
}

static void startChar(uint32_t bits, uint8_t n, const uint8_t* echo, uint8_t echoN, const String& disp) {
  sBits = bits; sLen = n; sPos = 0;
  pendEchoN = echoN;
  for (uint8_t i = 0; i < echoN; ++i) pendEcho[i] = echo[i];
  pendDisp = disp;
  startMark();
}

static void space(long ms) {
  sDeadline = millis() + (ms > 0 ? ms : 0);
  sState = S_SPACE;
}

static void charDone() {               // end of the last mark: echo and show the character
  if (sessionOpen && (hostMode & 0x04))       // serial echo
    for (uint8_t i = 0; i < pendEchoN; ++i)
      outPut(pendEcho[i]);
  pendEchoN = 0;
  if (pendDisp.length())
    displayGeneratedMorse(REGULAR, pendDisp);   // own transmission: REGULAR (UX_CONVENTIONS §5)
  pendDisp = "";
}

// Take the next item out of the buffer and start it.
static void nextItem() {
  while (bufCount && sState == S_IDLE) {
    uint8_t t = tag[bufHead];
    uint8_t b = bufGet();
    if (t == T_PARAM) continue;                 // orphan (cannot happen; robustness)
    if (t == T_TEXT) {
      WkTiming tm; timing(tm);
      if (b == ' ') {
        if (sessionOpen && (hostMode & 0x04)) outPut(' ');
        displayGeneratedMorse(REGULAR, " ");
        space(tm.wordExtra);
        return;
      }
      if (b == '|') {                           // half-dit pad
        space(tm.dit / 2);
        return;
      }
      uint32_t bits = 0; uint8_t n = 0;
      if (!appendPattern(b, bits, n) || n == 0)
        continue;                               // unmapped: ignored
      uint8_t echo = (b >= 0x80) ? 0 : b;
      startChar(bits, n, &echo, echo ? 1 : 0, displayOf(b));
      return;
    }
    // a buffered command; it was put in whole, so its parameters follow
    switch (b) {
      case 0x18: bufGet(); break;                                   // PTT on/off - D3: no PTT
      case 0x19: {                                                  // key buffered, 0..99 s
                  uint8_t s = bufGet();
                  if (s) { keyDown(true); sDeadline = millis() + 1000UL * _min(s, 99); sState = S_TIMED; }
                  return;
                 }
      case 0x1A: {                                                  // wait, 0..99 s
                  uint8_t s = bufGet();
                  waitOn = true; sDeadline = millis() + 1000UL * _min(s, 99); sState = S_TIMED;
                  return;
                 }
      case 0x1B: {                                                  // merge letters
                  uint8_t a = bufGet(), c = bufGet();
                  if (a >= 0x80) a = 0;                     // host bytes: never internal M32 codes (unmapped -> ignored)
                  if (c >= 0x80) c = 0;
                  uint32_t bits = 0; uint8_t n = 0;
                  uint8_t echo[2]; uint8_t echoN = 0;
                  String d = "<";
                  if (a && appendPattern(a, bits, n)) { echo[echoN++] = a; d += displayOf(a); }
                  if (c && appendPattern(c, bits, n)) { echo[echoN++] = c; d += displayOf(c); }
                  if (!n) break;
                  startChar(bits, n, echo, echoN, d + ">");
                  return;
                 }
      case 0x1C: {                                                  // buffered speed change
                  uint8_t w = bufGet();
                  if (w) bufWpm = w;
                  break;
                 }
      case 0x1D: bufGet(); break;                                   // buffered HSCW - not supported
      case 0x1E: restoreBufferedSpeed(); break;                     // cancel buffered speed
      default:   break;                                             // 0x1F NOP
    }
  }
}

static void runSender() {
  unsigned long now = millis();
  if (tuneOn) {
    if (now - tuneDeadline < 0x80000000UL || longMarkCutByRig(now)) {    // 100 s watchdog, or the rig's limit (R3a)
      tuneOn = false;
      keyUp();
    }
    return;
  }
  switch (sState) {
    case S_MARK:
      if ((long)(now - sDeadline) < 0) return;
      keyUp();
      if (sPos >= sLen)
        charDone();
      space(sSpaceAfter);
      return;
    case S_SPACE:
      if ((long)(now - sDeadline) < 0) return;
      if (sPos < sLen) { startMark(); return; }
      sState = S_IDLE;
      break;
    case S_TIMED:
      if ((long)(now - sDeadline) < 0 && !longMarkCutByRig(now)) return;
      keyUp();
      waitOn = false;
      sState = S_IDLE;
      break;
    case S_IDLE:
      break;
  }
  if (sState == S_IDLE && !paused && !breakin)
    nextItem();
}

///////////////////////////// status

static uint8_t status() {
  uint8_t st = 0xC0;
  if (waitOn)                                     st |= 0x10;
  if (!wk2Status && (tuneOn || (hostKeying && sState == S_TIMED)))
                                                  st |= 0x08;     // KEYDOWN (WK1 format)
  if (bufCount || sState != S_IDLE || tuneOn)     st |= 0x04;     // BUSY
#ifdef REMOTE_WINKEYER
  // R5: the rig plays the host's CW out one playout delay after it is sent here. BUSY stays up until it
  // has, so a logger that drops PTT over CAT when BUSY clears does not clip the end on the remote rig.
  else if (remoteMode && remoteLink == RL_UP && hostKeyUpAt
           && millis() - hostKeyUpAt < (unsigned long) MorseKipKeyer::playoutMs() + 20)
                                                  st |= 0x04;
#endif
  if (breakin)                                    st |= 0x02;
  if (bufCount > BUF_SIZE * 2 / 3)                st |= 0x01;     // XOFF
  return st;
}

static void statusUpdate() {
  uint8_t st = status();
  if (sessionOpen && st != lastStatus)
    outPut(st);
  lastStatus = st;
}

///////////////////////////// session

static uint8_t m32SidetoneCode() {
  int p = MorseOutput::notes[MorsePreferences::pliste[posPitch].value];
  if (wk3Mode)
    return (uint8_t) constrain(62500 / p, 1, 255);
  return (uint8_t) constrain((4000 + p / 2) / p, 1, 10) | (sidetoneByte & 0x80);
}

// The mode register as the M32 actually behaves: the host's echo/contest/watchdog bits, the
// M32's own keyer mode, paddle swap and autospace (D7).
static uint8_t effectiveModeReg() {
  uint8_t m = hostMode & 0xC5;
  switch (MorsePreferences::pliste[posCurtisMode].value) {
    case IAMBICA:     m |= 0x10; break;
    case ULTIMATIC:   m |= 0x20; break;
    case STRAIGHTKEY: m |= 0x30; break;
    default:          break;                           // Iambic B, Non-Squeeze: 00
  }
  if (MorsePreferences::pliste[posPolarity].value == 0) m |= 0x08;   // doPaddleIambic() swaps on 0
  if (MorsePreferences::pliste[posACS].value)          m |= 0x02;
  return m;
}

// Load Defaults order (WK2 v23 / WK3)
static void currentValues(uint8_t v[15]) {
  v[0]  = effectiveModeReg();
  v[1]  = hostWpm ? hostWpm : MorsePreferences::wpm;
  v[2]  = m32SidetoneCode();
  v[3]  = weight;
  v[4]  = lead;
  v[5]  = tail;
  v[6]  = potMin;
  v[7]  = potRange;
  v[8]  = wk3Mode ? x2Mode : firstExt;
  v[9]  = keyComp;
  v[10] = farnsWpm;
  v[11] = setpoint;
  v[12] = ratio;
  v[13] = pinCfg;
  v[14] = x1Mode;
}

static void resetSessionSettings() {
  hostMode = 0; hostWpm = 0; bufWpm = 0;
  sidetoneByte = 0; weight = 50; ratio = 50; farnsWpm = 0; firstExt = 0; keyComp = 0;
  lead = tail = 0; setpoint = 50;
  potMin = 5; potRange = 30; potWindowSet = false;
  pinCfg = 0x06;                          // sidetone + key output
  x1Mode = x2Mode = 0;
  wk3Mode = false; wk2Status = false;
  paused = false;
  breakin = false;
}

static void openSession() {
  if (!sessionOpen)
    potWpm = MorsePreferences::wpm;       // the M32's own speed becomes the speed pot (E1)
  clearBuffer();
  resetSessionSettings();
  sessionOpen = true;
  paddleWordOpen = false;
  lastConnectMs = millis();
  m32protocol = false;                    // whoever spoke the M32 protocol on this port is gone
  closeToMenu = false;
  outPut(REVISION);
  replyNow();
  lastStatus = status();
  applySpeed();
  if (inMode) {
    // E1: while a host is connected the encoder is the speed control. Put it back into that role:
    // on the classic a logger's DTR/RTS can hold FN (GPIO0) for the whole session, so an FN click
    // that its connect produced (bench: N1MM toggled the encoder to volume) could not be undone.
    if (encoderState == volumeSettingMode) {
      MorsePreferences::writeVolume();          // as the FN toggle back to speed does
      encoderState = speedSettingMode;
    } else if (encoderState == scrollMode) {
      encoderState = speedSettingMode;
      MorseOutput::relPos = MorseOutput::maxPos;
      MorseOutput::refreshScrollArea(MorseOutput::relPos);
      MorseOutput::displayScrollBar(false);
    }
    updateTopLine();
    announce("Host connected");
  }
}

static void setBaud(bool high) {
#ifndef WK_NATIVE_USB
  if (high == highBaud) return;
  Serial.flush();
  Serial.updateBaudRate(high ? 9600 : 1200);
#endif
  highBaud = high;
}

static void closeSession(bool fromHost) {
  if (!sessionOpen) return;
  clearBuffer();
  sessionOpen = false;
  resetSessionSettings();
  if (inMode)
    setBaud(false);                       // K1EL: 1200 baud is reinstated on every close
  applySpeed();                           // back to the M32's own speed
  if (fromHost && autoEntered)
    closeToMenu = true;
  autoEntered = false;
  if (inMode && !closeToMenu) {
    updateTopLine();
    announce("Host closed");
  }
}

///////////////////////////// D8: messages and Vcc

static void queueMemory(uint8_t n) {      // admin Send Message 1..6 -> M32 keyer memory n
  if (!inMode || n < 1 || n > 6 || !(MorsePreferences::cwMemMask & (1 << (n - 1))))
    return;                               // an empty slot sends nothing
  String text = String(MorsePreferences::cwMem[n - 1]);
  int i = 0, l = text.length();
  while (i < l) {                         // word by word, cleaned up as the memory keyer's playback does
    while (i < l && isSpace(text[i])) ++i;
    int s = i;
    while (i < l && !isSpace(text[i])) ++i;
    if (i == s) break;
    String w = cleanUpText(text.substring(s, i));
    w.replace("T", "");
    if (w.indexOf('P') != -1) {           // the player's pause token: a few word spaces
      bufPut(' ', T_TEXT); bufPut(' ', T_TEXT);
    } else {
      for (unsigned int k = 0; k < w.length(); ++k) {
        int idx = poolIndex(w[k]);
        if (idx >= 0) bufPut(0x80 | idx, T_TEXT);
      }
    }
    bufPut(' ', T_TEXT);
  }
}

static uint8_t vccByte() {
  uint16_t mv;
#ifdef CONFIG_MCP73871
  mv = batteryVoltage();
#else
  mv = volt;                              // the classic reads its battery by switching Vext, which also
                                          // powers the display - so report the reading taken at boot
#endif
  if (mv == 0) return 0xFF;
  return (uint8_t) constrain(262140UL / mv, 1UL, 255UL);   // K1EL: 26214 / byte = volts x 100
}

///////////////////////////// parser

static uint16_t adminParams(uint8_t sub) {
  switch (sub) {
    case 0x00: return 1;                  // calibrate (WK1): FF
    case 0x04: return 1;                  // echo test
    case 0x0D: return 256;                // load EEPROM
    case 0x0E: return 1;                  // send message
    case 0x0F: return 1;                  // load X1MODE
    case 0x13: return 2;                  // RTTY registers
    case 0x16: return 1;                  // load X2MODE
    case 0x19: return 1;                  // sidetone volume
    default:   return 0;
  }
}

static uint16_t cmdParams(uint8_t c) {
  switch (c) {
    case 0x04: case 0x1B:          return 2;    // PTT lead/tail; merge letters
    case 0x05:                     return 3;    // speed pot setup
    case 0x0F:                     return 15;   // load defaults
    case 0x07: case 0x08: case 0x0A: case 0x13: case 0x15: case 0x1E: case 0x1F:
                                   return 0;
    default:                       return 1;    // 0x16 pointer: +1 for sub-command 3, see feed()
  }
}

static void sendValues() {
  if (wk3Mode) { outPut(0); return; }     // WK3: historical, returns 0
  uint8_t v[15]; currentValues(v);
  for (uint8_t i = 0; i < 15; ++i) outPut(v[i]);
}

static void dumpEeprom() {                // for K1EL's WK3tools (flwkey 1.2.4 never asks: BENCH.md)
  uint8_t v[15]; currentValues(v);
  outPut(0xA5);                           // WK2 EEPROM magic, then the settings block (DESIGN.md §7.1)
  for (uint8_t i = 0; i < 15; ++i) outPut(v[i]);
  for (uint16_t i = 16; i < 256; ++i) outPut(0);
}

static void execAdmin(uint8_t sub) {
  switch (sub) {
    case 0x01: closeSession(true); break;                       // reset: session state only, never a reboot
    case 0x02: openSession(); break;
    case 0x03: closeSession(true); break;
    case 0x04: outPut(pPar[0]); replyNow(); lastConnectMs = millis(); break;
    case 0x05: case 0x06: outPut(0); replyNow(); break;         // paddle / speed A2D (WK1)
    case 0x07: sendValues(); replyNow(); break;
    case 0x09: outPut(REVISION >= 30 ? REVISION : 0x18); replyNow(); break;
    case 0x0A: wk2Status = false; break;
    case 0x0B: wk2Status = true;  break;
    case 0x0C: dumpEeprom(); replyNow(); break;
    case 0x0E: queueMemory(pPar[0]); break;
    case 0x0F: x1Mode = pPar[0]; break;
    case 0x11: if (inMode) setBaud(false); break;
    case 0x12: if (inMode) setBaud(true);  break;
    case 0x14: wk3Mode = true; break;
    case 0x15: outPut(vccByte()); replyNow(); break;
    case 0x16: x2Mode = pPar[0]; break;
    case 0x17: outPut(0); replyNow(); break;                   // FW minor revision
    case 0x18: outPut(0); replyNow(); break;                   // IC type: DIP
    default:   break;                                          // calibrate, reserved, load EEPROM,
                                                               // FW update, RTTY, volume: ignored
  }
}

static void setMode(uint8_t m) { hostMode = m; restoreBufferedSpeed(); }

static void execCmd(uint8_t c) {
  uint8_t p = pPar[0];
  switch (c) {
    case 0x01: sidetoneByte = p; break;                        // pitch stays the M32's (D7)
    case 0x02: hostWpm = p; restoreBufferedSpeed(); applySpeed(); break;
    case 0x03: weight = constrain(p, 10, 90); restoreBufferedSpeed(); break;
    case 0x04: lead = pPar[0]; tail = pPar[1]; break;          // PTT: D3, tail also times the 1st extension
    case 0x05: potMin = pPar[0]; potRange = pPar[1]; potWindowSet = true; applySpeed(); break;
    case 0x06: paused = (p != 0); break;
    case 0x07: outPut(potByte()); break;
    case 0x08:                                                 // backspace: the last unsent text byte
               if (bufCount && tag[(bufHead + bufCount - 1) % BUF_SIZE] == T_TEXT) --bufCount;
               break;
    case 0x09: pinCfg = p; break;
    case 0x0A: clearBuffer(); break;
    case 0x0B: if (p && !inPrefs                              // not while the preferences menu is up: its loop
                                                              // reads serial but never runs the tune watchdog
                   && !(remoteMode && remoteLink != RL_UP)) { // Remote WinKeyer: only with the link up (R4, R6)
                 abortSending(); tuneOn = true; tuneDeadline = millis() + TUNE_WATCHDOG_MS; keyDown(true);
               }
               else if (tuneOn) { tuneOn = false; keyUp(); }
               break;
    case 0x0D: farnsWpm = p; restoreBufferedSpeed(); break;
    case 0x0E: setMode(p); break;
    case 0x0F:                                                 // load defaults - all 15 parsed
               setMode(pPar[0]);
               hostWpm = pPar[1];
               sidetoneByte = pPar[2];
               weight = constrain(pPar[3], 10, 90);
               lead = pPar[4]; tail = pPar[5];
               potMin = pPar[6]; potRange = pPar[7]; potWindowSet = true;
               if (wk3Mode) x2Mode = pPar[8]; else firstExt = pPar[8];   // byte 9 changes meaning
               keyComp = pPar[9];
               farnsWpm = pPar[10];
               setpoint = pPar[11];                            // the M32's Latency plays this role (D7)
               ratio = constrain(pPar[12], 33, 66);
               pinCfg = pPar[13];
               if (wk3Mode) x1Mode = pPar[14];                 // byte 15: WK2 "don't care"
               applySpeed();
               break;
    case 0x10: firstExt = p; break;
    case 0x11: keyComp = p; restoreBufferedSpeed(); break;     // host CW only (DESIGN.md §7.2)
    case 0x12: setpoint = p; break;
    case 0x15: outPut(status()); break;
    case 0x17: ratio = constrain(p, 33, 66); restoreBufferedSpeed(); break;
    default:   break;      // 0x0C HSCW, 0x13 null, 0x14 software paddle, 0x16 pointer: parsed, ignored
  }
}

static void finish() {
  uint16_t c = pCmd;
  pCmd = NO_CMD;
  if (pIsAdmin) {
    execAdmin((uint8_t) c);
    return;
  }
  if (c >= 0x18) {                        // buffered: into the buffer whole, unless the paddle has it
    if (breakin) return;
    uint16_t need = 1 + pNeed;
    if (BUF_SIZE - bufCount < need) return;
    bufPut((uint8_t) c, T_CMD);
    for (uint16_t i = 0; i < pNeed; ++i) bufPut(pPar[i], T_PARAM);
    return;
  }
  execCmd((uint8_t) c);
}

static void feed(uint8_t b) {
  if (pAdminNext) {                       // the admin sub-command byte
    pAdminNext = false;
    pCmd = b; pIsAdmin = true; pHave = 0;
    pNeed = adminParams(b);
    if (!pNeed) finish();
    return;
  }
  if (pCmd != NO_CMD) {                   // a parameter
    if (pHave < sizeof(pPar)) pPar[pHave] = b;
    ++pHave;
    if (!pIsAdmin && pCmd == 0x16 && pHave == 1 && b == 3)
      pNeed = 2;                          // pointer command "add nulls" has a count
    if (pHave >= pNeed) finish();
    return;
  }
  if (b == 0x00) { pAdminNext = true; return; }
  if (!sessionOpen) return;                      // closed: only admin commands are accepted
  if (b < 0x20) {
    pCmd = b; pIsAdmin = false; pHave = 0;
    pNeed = cmdParams(b);
    if (!pNeed) finish();
    return;
  }
  if (b >= 0x80 || breakin) return;       // host text meets a paddle break-in: dropped
  bufPut(b, T_TEXT);
}

static void resetParser() { pCmd = NO_CMD; pAdminNext = false; pHave = pNeed = 0; }

///////////////////////////// public

void MorseWinKeyer::enter(bool autoOpened, bool remote) {
  inMode = true;
  remoteMode = remote;
  remoteLink = RL_START;
  hostKeyUpAt = 0;
  portOwned = true;
  m32protocol = false;
  closeToMenu = false;
  autoEntered = autoOpened && sessionOpen;
#ifdef WK_NATIVE_USB
  autoOpenPending = false;
#endif
  if (!autoEntered) {
    resetParser();
    closeSession(false);
    resetSessionSettings();
    potWpm = MorsePreferences::wpm;
#ifndef WK_NATIVE_USB
    Serial.flush();
    Serial.end();
    Serial.begin(1200, SERIAL_8N2);       // WinKeyer: 1200 baud, 8 data bits, no parity, 2 stop bits
    armForeignUsbRxDetect();
    highBaud = false;
#endif
  }
  paddleElems = 0; prevKs = IDLE_STATE;
  fnFilter.reset();                       // filterFn(): judge FN afresh, an FN already down is a line
  lastStatus = status();
  if (!remoteMode)                        // Remote WinKeyer says it after the link is up (MorseMenu.cpp)
    announce(autoEntered ? "Host connected" : "Waiting for host");
}

void MorseWinKeyer::leave() {
  if (!portOwned) return;
  closeSession(false);
  abortSending();
  resetParser();
  inMode = false;
  remoteMode = false;
  remoteLink = RL_START;
  autoEntered = closeToMenu = false;
  outHead = outCount = 0;
#ifndef WK_NATIVE_USB
  Serial.flush();
  Serial.end();
  Serial.begin(115200);                  // back to the M32 protocol's speed and framing
  armForeignUsbRxDetect();
#endif
  highBaud = false;
  portOwned = false;
}

void MorseWinKeyer::serviceInput() {
  while (Serial.available()) {
    feed((uint8_t) Serial.read());
    if (sessionOpen) MorseOutput::resetTOT();
  }
  statusUpdate();
  pump();
}

// Remote WinKeyer: follow the link. Going down is a break-in (R4): what the host had going is cut, the
// buffer cleared, and BREAKIN raised - N1MM stops a repeating CQ on exactly that bit. Until it is back up,
// host text is dropped (bufPut()). The first verdict after the start drops anything that queued while the
// link was coming up, with the same break-in - it would go out late.
static void followLink() {
#ifdef REMOTE_WINKEYER
  if (!remoteMode) return;
  RemoteLink now = MorseKipKeyer::linked() ? RL_UP : RL_DOWN;
  if (now == remoteLink) return;
  RemoteLink was = remoteLink;
  remoteLink = now;
  // Text that queued while the link was coming up (4-7 s after Host Open) would go on the air that late, so it
  // is dropped as well - whether the link came up or not - and the logger is told by the same break-in
  // (Willi, 2026-10-18: BENCH_REMOTE.md, interpretation 1).
  bool queuedAtStart = (was == RL_START && bufCount > 0);
  if (now == RL_DOWN || queuedAtStart) {
    clearBuffer();
    if (sessionOpen && (was == RL_UP || queuedAtStart)) {
      breakin = true;                     // cleared one word space later, as after the paddle
      lastPaddle = millis();
    }
    statusUpdate();
  }
#endif
}

void MorseWinKeyer::tick() {
  if (!inMode) { pump(); return; }
  followLink();
  serviceInput();
  tickServiced = true;
  if (breakin && millis() - lastPaddle > 7UL * 1200UL / MorsePreferences::wpm)
    breakin = false;                      // one word space after the paddle stopped, host CW resumes
  runSender();
  if (sessionOpen)
    MorseOutput::resetTOT();              // an attached logger is activity: no shutdown under it
  statusUpdate();
  pump();
}

void MorseWinKeyer::paddleActivity(bool pressed, KEYERSTATES ks) {
  if (!inMode) return;
  if (ks == KEYED && prevKs != KEYED) ++paddleElems;
  if (ks == IDLE_STATE) paddleElems = 0;  // a character ended: not a stuck paddle
  prevKs = ks;
  if (pressed || ks != IDLE_STATE) {
    lastPaddle = millis();
    lastKeyUp = lastPaddle;               // the rig is in transmit for the paddle too (1st extension)
    if (!breakin) {                       // break-in: the paddle wins at once (WK2 v23)
      breakin = true;
      clearBuffer();
      statusUpdate();                     // BREAKIN goes out before any paddle echo
    }
  }
}

void MorseWinKeyer::paddleChar(const String& symbol) {
  if (!sessionOpen || !(hostMode & 0x40))       // paddle echo off
    return;
  String s = symbol;
  s.trim();                               // hard rule 3: no stray whitespace from the decoder
  if (s.length() == 0) {                  // the decoder's word gap (the M32's Interword Spc): one space,
    if (paddleWordOpen) outPut(' ');      // as K3NG's emulator sends - never twice, never at the start
    paddleWordOpen = false;
    return;
  }
  // The decoder's prosigns and national letters, through the same single-byte codes the CW
  // generator uses (encodeProSigns(), hard rule 4), then to what a WinKeyer echoes: K1EL's ASCII
  // stand-ins where there is one (table 17), the two letters where there is not.
  encodeProSigns(s);
  for (unsigned int k = 0; k < s.length(); ++k) {
    char c = s[k];
    const char* out = nullptr;
    char one[2] = { 0, 0 };
    switch (c) {
      case 'S': out = "[";  break;                  // <as>
      case 'N': out = "(";  break;                  // <kn>
      case 'K': out = ">";  break;                  // <sk>
      case 'A': out = "KA"; break;                  // <ka> - no K1EL character
      case 'E': out = "VE"; break;                  // <ve>
      case 'B': out = "BK"; break;                  // <bk>
      case 'H': out = "CH"; break;                  // ch
      case 'R': case 'U': break;                    // <err>, * - nothing to echo
      case '\xE4': out = ";"; break;               // ä = .-.- = AA, K1EL's ';'
      default:
        if ((uint8_t) c > 0x20 && (uint8_t) c < 0x7F) { one[0] = (char) toupper(c); out = one; }
        break;                                      // other national letters: no ASCII form
    }
    for (; out && *out; ++out) {                    // sent while BREAKIN is high: the host knows
      outPut((uint8_t) *out);                       // it is the paddle
      paddleWordOpen = true;
    }
  }
}

void MorseWinKeyer::encoderSpeed(int t) {
  potWpm = clampWpm((int) potWpm + t);
  if (sessionOpen && hostWpm == 0 && potWindowSet)
    potWpm = clampWpm(constrain((int) potWpm, (int) potMin, (int) potTop()));
  if (sessionOpen) outPut(potByte());            // E1: the encoder is the speed pot
  applySpeed();
  displayCWspeed();
  if (protocolActive())
    MorseJSON::jsonControl("speed", MorsePreferences::wpm, MorsePreferences::wpmMin, MorsePreferences::wpmMax, false);
#ifdef CONFIG_AUDIO_A11Y
  MorseVoice::announceOverriding(String(MorsePreferences::wpm));
  MorseVoice::announceMore("words per minute");
#endif
}

void MorseWinKeyer::stopSending() {
  clearBuffer();
  statusUpdate();
}

void MorseWinKeyer::beforePreferences() {
  clearBuffer();                          // the preferences menu blocks loop(): no key may stay down
  statusUpdate(); pump();
  MorsePreferences::wpm = potWpm;         // only the M32's own speed may be persisted
  updateTimings();
  inPrefs = true;
}

void MorseWinKeyer::afterPreferences() {
  inPrefs = false;
  potWpm = MorsePreferences::wpm;
  MorsePreferences::wpm = 0;              // force applySpeed() to recompute and repaint
  applySpeed();
}

int MorseWinKeyer::filterFn(int clicks, bool down) {
  // A press under way while a host connects is the host's lines, not a finger - only the start-up
  // counts (Echo Test, Host Open), so FN stays usable while the logger is sending text.
  if (down && lastConnectMs && millis() - lastConnectMs < CONNECT_WINDOW_MS)
    fnFilter.markLine();
  return fnFilter.filter(clicks, down);
}

bool MorseWinKeyer::takeTickServiced() {
  bool r = tickServiced;
  tickServiced = false;
  return r;
}

bool MorseWinKeyer::wantsMenu() {
  bool r = closeToMenu;
  closeToMenu = false;
  return r;
}

bool MorseWinKeyer::isOpen() { return sessionOpen; }

bool MorseWinKeyer::hostSending() { return sState != S_IDLE || tuneOn; }

bool MorseWinKeyer::sidetoneWanted() {
  if (!(pinCfg & 0x02)) return false;                       // pin config: sidetone off
  bool paddleOnly = wk3Mode ? (x2Mode & 0x08) : (sidetoneByte & 0x80);
  return !(hostKeying && paddleOnly);                       // paddle-only sidetone mutes host CW
}

bool MorseWinKeyer::txWanted() {
  if (!(pinCfg & 0x0C)) return false;                       // both key outputs off: sidetone only
  if (!hostKeying) {
    if (!(hostMode & 0x80) && paddleElems >= 128) return false;   // paddle watchdog: 128 elements
    if (wk3Mode && (x2Mode & 0x02)) return false;           // X2MODE paddle mute: paddle in sidetone only
  }
  return true;
}

#ifdef WK_NATIVE_USB
bool MorseWinKeyer::topMenuByte(uint8_t c) {
  if (pCmd == NO_CMD && !pAdminNext && c != 0x00 && c != 0x13)
    return false;                         // not ours: the M32 protocol's line assembly gets it
  bool wasOpen = sessionOpen;
  feed(c);                                // closed: admin commands only, 0x13 nulls fall through
  if (sessionOpen && !wasOpen)
    autoOpenPending = true;
  pump();
  return true;
}

bool MorseWinKeyer::takeAutoOpen() {
  bool r = autoOpenPending;
  autoOpenPending = false;
  if (r && !sessionOpen) r = false;
  return r;
}
#endif

#endif // CONFIG_WINKEYER
