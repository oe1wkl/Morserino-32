#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
trainer_events.py -- protocol 1.6 trainer events: shape checks and bench test.

Protocol 1.6 adds two unsolicited objects (capability "trainer-events"):

    {"cw":{"event":"done","reason":"end"|"stopped"}}
        once per PUT cw/play, cw/repeat or cw/recall: when the text has
        finished sounding (at the end of its last element) or was stopped

    {"trainer":{"event":"result","mode":"echo","prompt":"cq","answer":"cq",
                "ok":true,"repeat":0,"wpm":18,"koch":12,
                "latency":820,"duration":2100}}
        once per evaluated Echo Trainer word; device-timed; on think-time
        expiry "answer":"" and "timeout":true instead of latency/duration

The first half of this file is pure (no device): validators that selftest.py
replays fixtures through. The second half drives a real Morserino:

    python3 trainer_events.py              # cw/done checks, about 45 s, hands off
    python3 trainer_events.py --paddle     # also: stop a cw/repeat with a paddle touch
    python3 trainer_events.py --echo 120   # also listen to the Echo Trainer
                                           # for 120 s while you key answers
    python3 trainer_events.py --soak 500   # also: 500 timed cw/play items (risk R1)

Run it on the classic M32 AND the M32 Pocket -- the Pocket's HWCDC is the
transport that once withheld replies (README), and for these events the
arrival time is the measurement.
"""

import argparse
import json
import sys
import time

# ----------------------------------------------------------------- validators

CW_DONE_REASONS = ("end", "stopped")
TRAINER_MODES = ("echo", "learn", "preview")


def check_cw_done(obj):
    """Problems with one {"cw":{...}} done event, as a list of strings ([] = fine)."""
    problems = []
    cw = obj.get("cw") if isinstance(obj, dict) else None
    if not isinstance(cw, dict):
        return ["not a cw object"]
    if cw.get("event") != "done":
        problems.append("event is %r, not 'done'" % cw.get("event"))
    if cw.get("reason") not in CW_DONE_REASONS:
        problems.append("reason %r is not one of %s" % (cw.get("reason"), CW_DONE_REASONS))
    extra = set(cw) - {"event", "reason"}
    if extra:
        problems.append("unexpected properties %s" % sorted(extra))
    return problems


def check_trainer_result(obj):
    """Problems with one {"trainer":{...}} result event, as a list of strings ([] = fine)."""
    problems = []
    r = obj.get("trainer") if isinstance(obj, dict) else None
    if not isinstance(r, dict):
        return ["not a trainer object"]
    if r.get("event") != "result":
        problems.append("event is %r, not 'result'" % r.get("event"))
    if r.get("mode") not in TRAINER_MODES:
        problems.append("mode %r is not one of %s" % (r.get("mode"), TRAINER_MODES))
    for key in ("prompt", "answer"):
        if not isinstance(r.get(key), str):
            problems.append("%s is missing or not a string" % key)
    if not r.get("prompt"):
        problems.append("prompt is empty")
    if not isinstance(r.get("ok"), bool):
        problems.append("ok is missing or not a boolean")
    elif isinstance(r.get("prompt"), str) and isinstance(r.get("answer"), str):
        if r["ok"] != (r["prompt"] == r["answer"]):
            problems.append("ok=%s disagrees with prompt %r vs answer %r" % (r["ok"], r["prompt"], r["answer"]))
    for key in ("repeat", "wpm"):
        if not isinstance(r.get(key), int) or isinstance(r.get(key), bool) or r[key] < 0:
            problems.append("%s is missing or not a non-negative integer" % key)
    if isinstance(r.get("wpm"), int) and not 5 <= r["wpm"] <= 60:
        problems.append("wpm %s is outside 5..60" % r["wpm"])
    if "koch" in r and (not isinstance(r["koch"], int) or r["koch"] < 1):
        problems.append("koch %r is present but not a lesson number" % r["koch"])
    if r.get("mode") in ("learn", "preview"):        # Koch Trainer modes: lesson always there, silence never reported
        if "koch" not in r:
            problems.append("a %s result without the koch lesson" % r["mode"])
        if r.get("timeout"):
            problems.append("a %s result marked timeout (an unanswered repetition sends nothing)" % r["mode"])
    if r.get("timeout"):
        if r.get("timeout") is not True:
            problems.append("timeout is present but not true")
        if r.get("answer") != "":
            problems.append("a timeout carries an answer %r" % r.get("answer"))
        if "latency" in r or "duration" in r:
            problems.append("a timeout carries latency/duration")
        if r.get("ok"):
            problems.append("a timeout is marked ok")
    else:
        for key in ("latency", "duration"):
            if not isinstance(r.get(key), int) or isinstance(r.get(key), bool) or r[key] < 0:
                problems.append("%s is missing or not a non-negative integer" % key)
        if isinstance(r.get("latency"), int) and r["latency"] > 120000:
            problems.append("latency %s ms is implausible (over 2 minutes)" % r["latency"])
    extra = set(r) - {"event", "mode", "prompt", "answer", "ok", "repeat", "wpm", "koch",
                      "timeout", "latency", "duration"}
    if extra:
        problems.append("unexpected properties %s" % sorted(extra))
    return problems


# Element counts (dit = 1, dah = 3) for the texts the bench plays.
MORSE = {"a": ".-", "e": ".", "i": "..", "p": ".--.", "r": ".-.", "s": "...", "t": "-"}


def sounded_dits(text, ics_dits, iws_dits):
    """Length in dits of `text` from its first element's start to its last element's end.

    ics_dits/iws_dits are the device's inter-character and inter-word spaces in dits
    (updateTimings(): interCharacterSpace = Interchar Spc x dit, interWordSpace =
    max(InterWord Spc, Interchar Spc + 4) x dit). No trailing space is counted: that is
    the whole point of where cw/done is sent (R13).
    """
    total = 0
    words = text.split()
    for wi, word in enumerate(words):
        for ci, ch in enumerate(word):
            code = MORSE[ch]
            total += sum(1 if e == "." else 3 for e in code) + (len(code) - 1)
            if ci < len(word) - 1:
                total += ics_dits
        if wi < len(words) - 1:
            total += iws_dits
    return total


# ------------------------------------------------------------------- bench

def _wait_for(m32, want, timeout):
    """Read objects until one satisfies want(obj); return (obj, arrival time, others seen)."""
    deadline = time.time() + timeout
    seen = []
    while time.time() < deadline:
        try:
            raw = m32._read_object(max(0.05, deadline - time.time()))
        except OSError:
            break
        at = time.time()
        obj = json.loads(raw)
        if want(obj):
            return obj, at, seen
        seen.append(obj)
    return None, None, seen


def _send_expect_ok(m32, text):
    """Send a command, require "ok" as the FIRST object back, return its arrival time."""
    m32._buffer = ""
    m32._serial.reset_input_buffer()
    m32._write_line(text)
    raw = m32._read_object(3.0)
    at = time.time()
    obj = json.loads(raw)
    if "ok" not in obj:
        raise AssertionError("%s: first object back was %s, not ok" % (text, raw))
    return at


def _split_stream(buf):
    """Split raw serial text into (JSON object texts, everything outside them)."""
    from m32_link import extract_json_object
    objects, outside, rest = [], [], buf
    while True:
        obj, after = extract_json_object(rest)
        if obj is None:
            outside.append(rest)
            return objects, "".join(outside)
        outside.append(rest[:rest.index(obj)])
        objects.append(obj)
        rest = after


def _play_and_capture(m32, text, timeout):
    """PUT cw/play/<text> and keep the RAW stream until "done": (objects, text outside JSON, ok time, done time).

    _read_object() would throw away the Serial Output characters between the objects - here they are the point.
    """
    m32._buffer = ""
    m32._serial.reset_input_buffer()
    m32._write_line("PUT cw/play/%s" % text)
    buf, ok_at, done_at = "", None, None
    deadline = time.time() + timeout
    while time.time() < deadline and done_at is None:
        chunk = m32._serial.read(max(1, m32._serial.in_waiting))    # not read(4096): see M32Link._read_object()
        if not chunk:
            continue
        buf += chunk.decode("utf-8", "replace")
        objects, _ = _split_stream(buf)
        if ok_at is None and any("ok" in json.loads(o) for o in objects):
            ok_at = time.time()
        if any(is_done(json.loads(o)) for o in objects):
            done_at = time.time()
    objects, outside = _split_stream(buf)
    return [json.loads(o) for o in objects], outside, ok_at, done_at


def is_done(obj):
    return isinstance(obj, dict) and isinstance(obj.get("cw"), dict) and obj["cw"].get("event") == "done"


def is_result(obj):
    return isinstance(obj, dict) and isinstance(obj.get("trainer"), dict) and obj["trainer"].get("event") == "result"


def menu_number(m32, content):
    for item in m32.command("GET menus", timeout=8.0)["menus"]:
        if item["content"] == content:
            return item["menu number"]
    raise AssertionError("menu %r not found" % content)


def start_mode(m32, content):
    m32.command("PUT menu/stop")
    for _ in range(20):                             # a mode winding down refuses the start (NOT AT MENU)
        if not m32.command("GET menu")["menu"]["active"]:
            break
        time.sleep(0.2)
    m32.command("PUT menu/start/%d" % menu_number(m32, content))
    time.sleep(0.8)


def bench(report, m32, args):
    # any paddle contact stops cw/play (and now reports done "stopped"); a finger resting on a
    # touch paddle makes the timing checks below fail at 0 ms with reason "stopped". Turning the knob
    # stretches the sounding instead: on the classic each volume step held the generator ~40 ms (2026-10-31)
    print("\n   Hands off the paddles and the knob until the script asks for them.\n")
    caps = m32.command("GET capabilities")["capabilities"]
    report.check(caps.get("protocol") == "1.6", "capabilities report protocol 1.6", "got %r" % caps.get("protocol"))
    report.check("trainer-events" in caps.get("features", []), "capabilities list trainer-events",
                 "features %s" % caps.get("features"))
    report.check(m32.device.get("protocol") == "1.6", "device object reports protocol 1.6",
                 "got %r" % m32.device.get("protocol"))

    wpm_before = m32.command("GET control/speed")["control"]["value"]
    start_mode(m32, "CW Keyer")
    try:
        m32.command("PUT control/speed/20")
        ics = m32.get_config("Interchar Spc")
        iws = max(m32.get_config("InterWord Spc"), ics + 4)
        dit_ms = 1200 / 20

        # 1. natural end, one word and two words: done "end", at the last element (R13)
        for text in ("paris", "paris paris"):
            sent = _send_expect_ok(m32, "PUT cw/play/%s" % text)
            want_ms = sounded_dits(text, ics, iws) * dit_ms
            obj, at, _ = _wait_for(m32, is_done, want_ms / 1000 + 3)
            report.check(obj is not None, "cw/play/%s: a done event arrives" % text)
            if obj is None:
                continue
            report.check(not check_cw_done(obj), "cw/play/%s: done is well-formed" % text, check_cw_done(obj))
            report.check(obj["cw"]["reason"] == "end", "cw/play/%s: reason is end" % text, obj)
            took = (at - sent) * 1000
            # early limit: it cannot be done before it has sounded; late limit: well inside one word space,
            # which is what the event would carry if it were sent where playCW is cleared
            report.check(want_ms - 80 <= took <= want_ms + min(200, iws * dit_ms * 0.6),
                         "cw/play/%s: done %.0f ms after ok, sounded length %.0f ms" % (text, took, want_ms),
                         "word space is %.0f ms" % (iws * dit_ms))
            again, _, _ = _wait_for(m32, is_done, iws * dit_ms / 1000 + 1.0)
            report.check(again is None, "cw/play/%s: exactly one done" % text, again)

        # 1b. PUT config spacing applies at once, inside the running Keyer (1.6; before, only after the next
        #     speed change or mode start) - checked through the done timing, the trainer's own yardstick
        wide = min(ics + 3, 15)
        m32.set_config("Interchar Spc", wide)
        try:
            sent = _send_expect_ok(m32, "PUT cw/play/paris")
            want_ms = sounded_dits("paris", wide, 0) * dit_ms        # one word: no inter-word space in it
            obj, at, _ = _wait_for(m32, is_done, want_ms / 1000 + 3)
            took = (at - sent) * 1000 if obj else -1
            report.check(obj is not None and want_ms - 80 <= took <= want_ms + 200,
                         "Interchar Spc %d applies at once: done %.0f ms after ok, want %.0f" % (wide, took, want_ms))
        finally:
            m32.set_config("Interchar Spc", ics)

        # 2. cw/stop: "ok" first (it is the command's reply), then done "stopped"
        _send_expect_ok(m32, "PUT cw/repeat/paris")
        time.sleep(1.0)
        _send_expect_ok(m32, "PUT cw/stop")
        obj, _, _ = _wait_for(m32, is_done, 1.5)
        report.check(obj is not None and obj["cw"]["reason"] == "stopped", "cw/stop: ok, then done stopped", obj)

        # 3. a repeat never ends by itself: no done within a few repetitions
        _send_expect_ok(m32, "PUT cw/repeat/e")
        obj, _, _ = _wait_for(m32, is_done, 2.5)
        report.check(obj is None, "cw/repeat: no done while it repeats", obj)

        # 4. a cw/play over a running repeat replaces it and ends (used to inherit the repeat): one done "end"
        _send_expect_ok(m32, "PUT cw/play/e")
        obj, _, _ = _wait_for(m32, is_done, 3.0)
        report.check(obj is not None and obj["cw"]["reason"] == "end", "cw/play over cw/repeat: done end", obj)
        again, _, _ = _wait_for(m32, is_done, 1.5)
        report.check(again is None, "cw/play over cw/repeat: exactly one done", again)

        # 5. nothing playable: done end promptly
        _send_expect_ok(m32, "PUT cw/play/~~~")
        obj, _, _ = _wait_for(m32, is_done, 2.0)
        report.check(obj is not None and obj["cw"]["reason"] == "end", "cw/play of unplayable text: done end", obj)

        # 6. cw/stop with nothing playing: ok, and no done
        _send_expect_ok(m32, "PUT cw/stop")
        obj, _, _ = _wait_for(m32, is_done, 1.0)
        report.check(obj is None, "cw/stop while idle: no done", obj)

        # 7. Serial Output: the event arrives whatever is echoed; with "Keyed" the cw/play text must NOT appear in
        #    the stream (the M3c keyed-answer source depends on it - open item 2), with "All" it must
        serial_before = m32.get_config("Serial Output")
        try:
            for value, label in ((0, "Nothing"), (1, "Keyed"), (5, "All")):
                m32.set_config("Serial Output", value)
                objects, outside, ok_at, done_at = _play_and_capture(m32, "paris", 6.0)
                report.check(done_at is not None, "Serial Output %s: done arrives" % label, objects)
                letters = "".join(ch for ch in outside.lower() if ch.isalpha())
                if value == 5:
                    report.check("paris" in letters, "Serial Output All: the played text is in the stream",
                                 "outside JSON: %r" % outside)
                else:
                    report.check(letters == "", "Serial Output %s: the played text is NOT in the stream" % label,
                                 "outside JSON: %r" % outside)
        finally:
            m32.set_config("Serial Output", serial_before)

        # 8. leaving the Keyer mid-text: done stopped
        _send_expect_ok(m32, "PUT cw/play/paris paris paris paris")
        time.sleep(0.6)
        _send_expect_ok(m32, "PUT menu/stop")
        obj, _, _ = _wait_for(m32, is_done, 2.0)
        report.check(obj is not None and obj["cw"]["reason"] == "stopped", "menu/stop during cw/play: done stopped", obj)
        # 9. a paddle touch stops it: done "stopped", and no unrequested "ok" (1.6)
        if args.paddle:
            start_mode(m32, "CW Keyer")
            _send_expect_ok(m32, "PUT cw/repeat/paris")
            print("\n   Touch a paddle now (within 15 s) to stop the repeating PARIS.\n")
            obj, _, seen = _wait_for(m32, is_done, 15.0)
            report.check(obj is not None and obj["cw"]["reason"] == "stopped", "paddle touch: done stopped", obj)
            report.check(not any("ok" in o for o in seen), "paddle touch: no unrequested ok", seen)

        # 10. R1 soak: many short items, each timed against its sounded length
        if args.soak:
            start_mode(m32, "CW Keyer")
            m32.command("PUT control/speed/20")
            import random
            rng = random.Random(1)
            errors, missing, worst = [], 0, []
            prev_done = None
            for n in range(args.soak):
                text = "".join(rng.choice(sorted(MORSE)) for _ in range(rng.randint(1, 5)))
                sent = _send_expect_ok(m32, "PUT cw/play/%s" % text)
                want_ms = sounded_dits(text, ics, iws) * dit_ms
                # done comes at the last element, but the keyer then still keeps a word space
                # (m32_v6.ino: genTimer = interWordSpace after a word): a text sent inside it
                # starts when it has passed, so consecutive texts stay a word apart
                held_ms = max(0.0, (prev_done + iws * dit_ms / 1000 - sent) * 1000) if prev_done else 0.0
                obj, at, _ = _wait_for(m32, is_done, (want_ms + held_ms) / 1000 + 3)
                if obj is None:
                    missing += 1
                    prev_done = None
                else:
                    errors.append((at - sent) * 1000 - want_ms - held_ms)
                    worst.append((errors[-1], text, held_ms))
                    prev_done = at
                # past the word space, so every item starts idle and held_ms stays 0: this measures delivery
                # (R1), not the keyer's spacing. The first soak slept 0.2 s and mixed the two (2026-10-31).
                time.sleep(iws * dit_ms / 1000 + 0.15)
                if (n + 1) % 50 == 0:
                    print("   soak %d/%d" % (n + 1, args.soak))
            errors.sort()
            late = [e for e in errors if not -80 <= e <= 200]
            if errors:
                print("   soak: done minus sounded length, ms: min %.0f  median %.0f  p95 %.0f  max %.0f"
                      % (errors[0], errors[len(errors) // 2], errors[int(len(errors) * 0.95)], errors[-1]))
                worst.sort()
                for e, text, held in worst[:3] + worst[-3:]:
                    print("     %+5.0f ms  %-6s (held %.0f ms)" % (e, text, held))
            report.check(missing == 0, "soak: every one of %d items got its done" % args.soak, "%d missing" % missing)
            report.check(not late, "soak: every done within -80..+200 ms of the sounded length",
                         "%d outside: %s" % (len(late), late[:10]))
    finally:
        m32.command("PUT menu/stop")
        # leaving a mode announces the menu a moment later; that object must not pass for the speed reply
        _wait_for(m32, lambda o: "menu" in o, 1.0)
        m32._buffer = ""
        m32._serial.reset_input_buffer()
        m32._write_line("PUT control/speed/%d" % wpm_before)
        reply, _, seen = _wait_for(m32, lambda o: "control" in o, 3.0)
        why = "reply %r, other objects %r" % (reply, seen)
        if reply is None:                           # withheld (USB) or never sent? Nudge and look again
            m32._write_line("GET capabilities")
            late, _, _ = _wait_for(m32, lambda o: "control" in o, 2.0)
            why += "; after a nudge: %s" % ("WITHHELD, then arrived: %r" % late if late else "still nothing (never sent)")
        report.check(reply is not None and reply["control"].get("value") == wpm_before,
                     "speed restored to %d WpM" % wpm_before, why)

    echo_seconds = args.echo
    if echo_seconds:
        start_mode(m32, args.echo_menu)
        print("\n   %s is running for %d s. Touch a paddle to start it, then answer some" % (args.echo_menu, echo_seconds))
        if args.echo_menu.endswith(("Learn New Chr", "Preview Char")):
            print("   prompts right, some wrong, and let some repetitions pass unanswered (they must send nothing).\n")
        else:
            print("   prompts right, some wrong, and let at least one run out (Echo Think Time).\n")
        deadline = time.time() + echo_seconds
        results = []
        while time.time() < deadline:
            obj, _, _ = _wait_for(m32, is_result, deadline - time.time())
            if obj is None:
                break
            r = obj["trainer"]
            results.append(r)
            print("   %-10s %-10s %-5s lat=%-6s dur=%-6s %s" % (r.get("prompt"), r.get("answer"), r.get("ok"),
                  r.get("latency", "-"), r.get("duration", "-"), "TIMEOUT" if r.get("timeout") else ""))
            report.check(not check_trainer_result(obj), "trainer/result is well-formed: %s" % json.dumps(r),
                         check_trainer_result(obj))
        menu = args.echo_menu
        mode = "learn" if menu.endswith("Learn New Chr") else "preview" if menu.endswith("Preview Char") else "echo"
        report.check(len(results) > 0, "the Echo Trainer sent at least one result")
        report.check(all(r.get("mode") == mode for r in results), 'every result has mode "%s"' % mode,
                     sorted({r.get("mode") for r in results}))
        if menu.startswith("Koch Trainer"):
            report.check(all(isinstance(r.get("koch"), int) for r in results), "every result carries the koch lesson")
        else:
            report.check(not any("koch" in r for r in results), "no koch field outside the Koch Trainer")
        report.check(any(r.get("ok") for r in results), "at least one correct answer seen")
        report.check(any(not r.get("ok") and not r.get("timeout") for r in results), "at least one wrong answer seen")
        if mode == "echo":
            report.check(any(r.get("timeout") for r in results), "at least one timeout seen")
        else:                                       # an unanswered repetition sends nothing there
            report.check(not any(r.get("timeout") for r in results), "no timeout results in %s mode" % mode)
        report.check(all(r.get("timeout") or 0 < r.get("latency", 0) < 60000 for r in results),
                     "every answered result has a plausible latency")
        m32.command("PUT menu/stop")


def main():
    from m32_link import M32Link, find_port
    from run_tests import Report

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default=None)
    ap.add_argument("--echo", type=int, default=0, metavar="SECONDS",
                    help="also run the Echo Trainer for SECONDS and check its trainer/result events")
    ap.add_argument("--echo-menu", default="Echo Trainer/Random", metavar="PATH",
                    help="menu path for --echo, e.g. 'Koch Trainer/Learn New Chr' (default: %(default)s)")
    ap.add_argument("--paddle", action="store_true", help="also check that a paddle touch stops cw/repeat")
    ap.add_argument("--soak", type=int, default=0, metavar="N",
                    help="also time N cw/play items against their sounded length (risk R1)")
    ap.add_argument("-v", action="store_true", help="echo every command and reply")
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        print("no Morserino found on USB")
        return 2
    report = Report()
    with M32Link(port, verbose=args.v) as m32:
        print("device: %s" % m32.device)
        bench(report, m32, args)
    return report.summary()


if __name__ == "__main__":
    sys.exit(main())
