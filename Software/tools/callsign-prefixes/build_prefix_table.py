#!/usr/bin/env python3
"""Build callsign_prefixes.h - the prefix table behind generated call signs and prefixes.

    python3 build_prefix_table.py MASTER.SCP cty.dat [-o .../callsign_prefixes.h]

Inputs (not committed - fetch current copies before rebuilding):
  MASTER.SCP     Super Check Partial call list (supercheckpartial.com): calls heard in contests.
                 Gives how common each prefix is and which call-area digits are really used.
  cty.dat        AD1C country file (country-files.com/cty/cty.dat): entity, continent and CQ zone
                 for every prefix, including call-area splits (R9 = Asiatic Russia, EA8 = Canaries).
  base_prefixes.txt (next to this script, committed): the ITU-block prefixes of the original table.

One table entry per (base prefix, continent):
  - base prefix: an entry of base_prefixes.txt, or a block found in MASTER.SCP that it lacked
    (KD, WA, IK, UA, R, TI ...: a real prefix minus its call-area digit).
  - weight: 255 * ln(n) / ln(n_max), n = calls under the base_prefixes.txt entry the prefix falls
    under (the original formula - so the mix of countries stays as it was). A new block takes its
    share of that entry's weight; its own count decides only that share.
  - common: that entry's weight >= COMMON_WEIGHT ("Call Prefixes: Common only").
  - digits: 10 nibble weights for call-area digit 0..9, from MASTER.SCP where at least MIN_CALLS
    calls back them, else 1 for every digit cty.dat places on this continent. All zero = the
    prefix takes no digit (3B8, I1, M4: the calls are 3B8AB, I1ABC, M4A).
Calls with '/' and special-event forms (R59, YR95, OE2008: more than one digit after the base)
are left out.
"""

import argparse
import collections
import math
import os
import re
import sys

COMMON_WEIGHT = 81   # firmware threshold of the original table ("Claude recommended 2 but that gave way too many rare prefixes")
MIN_CALLS = 20       # calls needed before the digit spread is taken from the data
BARE_MIN_CALLS = 5   # calls needed before the data decides whether the prefix takes a digit at all
CONT = {'EU': 0x01, 'NA': 0x02, 'SA': 0x04, 'AF': 0x08, 'AS': 0x10, 'OC': 0x20, 'AN': 0x40}


def read_cty(path):
    """prefix -> (entity, continent bit, CQ zone); exact-call entries ('=CALL') are skipped."""
    table, entity = {}, None
    version = '?'
    for line in open(path, encoding='latin-1'):
        line = line.rstrip('\r\n')
        if not line.strip():
            continue
        if not line[0].isspace():
            f = [x.strip() for x in line.split(':')]
            # cty.dat files Antarctica under SA (a logging convention); the M32 keeps it apart, as
            # the original table did, so it does not turn up under "S America"
            entity = (f[0], CONT['AN'] if f[0] == 'Antarctica' else CONT[f[3]], int(f[1]))
            continue
        for tok in line.strip().rstrip(';').split(','):
            tok = tok.strip()
            if not tok:
                continue
            if tok.startswith('='):
                m = re.match(r'=(VER\d{8})', tok)               # the file's own version stamp
                if m:
                    version = m.group(1)
                continue
            p = re.match(r'[A-Z0-9/]+', tok).group(0)
            z = re.search(r'\((\d+)\)', tok)
            c = re.search(r'\{(\w+)\}', tok)
            table[p] = (entity[0], CONT[c.group(1)] if c else entity[1], int(z.group(1)) if z else entity[2])
    return table, version


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('master_scp')
    ap.add_argument('cty_dat')
    ap.add_argument('-o', '--output',
                    default=os.path.join(here, '../../src/Version 6 and newer/callsign_prefixes.h'))
    args = ap.parse_args()

    base_list = [l.strip() for l in open(os.path.join(here, 'base_prefixes.txt'))
                 if l.strip() and not l.startswith('#')]
    cty, cty_version = read_cty(args.cty_dat)
    # A base that already ends in its digit (3B8, KH6) is a prefix of its own only if cty.dat lists it as
    # one - KC4 in the original table was the Antarctic special case, while the KC4 calls on the air are
    # plain US calls of the KC block.
    not_own = [b for b in base_list if b[-1].isdigit() and len(b) > 2 and b not in cty]
    base_set = set(base_list) - set(not_own)

    def cty_lookup(call):
        for n in range(min(len(call), 7), 0, -1):
            if call[:n] in cty:
                return cty[call[:n]]
        return None

    scp_release = '?'
    calls = []
    for line in open(args.master_scp, encoding='latin-1'):
        line = line.strip().upper()
        if line.startswith('#'):
            m = re.search(r'RELEASE\s+(\S+)', line)
            if m:
                scp_release = m.group(1)
            continue
        if line and '/' not in line:
            calls.append(line)

    def longest_base(call):
        return next((call[:n] for n in (4, 3, 2, 1) if call[:n] in base_set), None)

    # --- sort every call into (base, digit) ------------------------------------------------
    digit_count = collections.defaultdict(collections.Counter)   # base -> digit ('' = none) -> calls
    parent_of = {}                                                # base -> base_prefixes entry it falls under
    parent_calls = collections.Counter()                          # base_prefixes entry -> calls
    skipped = collections.Counter()
    for call in calls:
        m = re.match(r'^([A-Z0-9]*?[0-9])[A-Z]+$', call)          # real prefix: up to the last digit
        if not m or not cty_lookup(call):
            skipped['no prefix / unknown entity'] += 1
            continue
        wpx = m.group(1)
        b = longest_base(call)
        if b and wpx == b:
            base, digit = b, ''
        elif b and len(wpx) == len(b) + 1 and wpx.startswith(b):
            base, digit = b, wpx[-1]
        elif len(wpx) >= 2 and not wpx[-2].isdigit() and (not b or wpx.startswith(b)):
            base, digit = wpx[:-1], wpx[-1]                       # KD9 -> KD under K, UA3 -> UA
        else:
            skipped['special-event form'] += 1
            continue
        if len(base) > 4:
            skipped['prefix too long'] += 1
            continue
        parent = b or base
        digit_count[base][digit] += 1
        parent_of[base] = parent
        parent_calls[parent] += 1

    n_max = max(parent_calls.values())

    def log_weight(n):
        return 1 if n <= 1 else max(1, min(255, round(255 * math.log(n) / math.log(n_max))))

    # --- one entry per (base, continent) ---------------------------------------------------
    entries, dropped = [], []
    for base in sorted(base_set | set(digit_count)):
        dc = digit_count.get(base, collections.Counter())
        n_all = sum(dc.values())
        if n_all >= BARE_MIN_CALLS:
            bare = dc[''] > n_all / 2
        else:
            bare = base[-1].isdigit() and len(base) > 2
        slots = [''] if bare else [str(d) for d in range(10)]
        by_cont = collections.defaultdict(list)
        for d in slots:
            e = cty_lookup(base + d + 'AA')
            if e:
                by_cont[e[1]].append((d, e))
        if not by_cont:
            dropped.append(base)
            continue
        parent = parent_of.get(base, base)
        pn = parent_calls.get(parent, 0)
        pw = log_weight(pn)
        for cont, ds in by_cont.items():
            n = sum(dc[d] for d, _ in ds)
            weight = max(1, round(pw * n / pn)) if n else 1
            digits = [0] * 10
            if not bare:
                if n >= MIN_CALLS:
                    top = max(dc[d] for d, _ in ds)
                    for d, _ in ds:
                        if dc[d]:
                            digits[int(d)] = max(1, round(15 * dc[d] / top))
                else:
                    for d, _ in ds:
                        digits[int(d)] = 1
            zone = collections.Counter(e[2] for d, e in ds for _ in range(max(1, dc[d]))).most_common(1)[0][0]
            entries.append((base, cont, zone, weight, pw >= COMMON_WEIGHT and n > 0, digits, ds[0][1][0]))

    # --- write the header ------------------------------------------------------------------
    common = sum(1 for e in entries if e[4])
    seen = sum(1 for e in entries if e[3] > 1)
    out = []
    out.append('// callsign_prefixes.h - generated by Software/tools/callsign-prefixes/build_prefix_table.py; do not edit.')
    out.append('// Sources: MASTER.SCP release %s (%d calls), cty.dat %s; %d base prefixes.' %
               (scp_release, len(calls), cty_version, len(base_list)))
    out.append('// %d entries x %d bytes = %d bytes of flash; %d heard in MASTER.SCP, %d "common".' %
               (len(entries), 14, len(entries) * 14, seen, common))
    out.append('//')
    out.append('// One entry per base prefix and continent. A generated prefix is the base plus one call-area digit,')
    out.append('// drawn by the digit weights (0 = never used); all digit weights 0 = the base is the whole prefix.')
    out.append('//')
    out.append('// Continent bitmask: EU=0x01, NA=0x02, SA=0x04, AF=0x08, AS=0x10, OC=0x20, AN=0x40')
    out.append('//')
    out.append('#ifndef CALLSIGN_PREFIXES_H')
    out.append('#define CALLSIGN_PREFIXES_H')
    out.append('')
    out.append('#define PREFIX_COUNT %d' % len(entries))
    out.append('')
    for k in ('EU', 'NA', 'SA', 'AF', 'AS', 'OC', 'AN'):
        out.append('#define CONT_%s  0x%02X' % (k, CONT[k]))
    out.append('#define CONT_ALL 0x7F')
    out.append('')
    out.append('#define PFX_COMMON 0x01     // flags: counts as "common" (Call Prefixes: Common only)')
    out.append('')
    out.append('struct PrefixEntry {')
    out.append('    char prefix[5];      // base prefix, null-terminated, max 4 chars')
    out.append('    uint8_t continent;   // continent bitmask (one continent per entry)')
    out.append('    uint8_t cqZone;      // primary CQ zone (1-40)')
    out.append('    uint8_t weight;      // log-scaled frequency (1=rare/unheard, 255=very common)')
    out.append('    uint8_t flags;       // PFX_COMMON')
    out.append('    uint8_t digits[5];   // call-area digit weights 0-15, two per byte: digit 2i low nibble, 2i+1 high')
    out.append('};')
    out.append('')
    out.append('const PrefixEntry prefixTable[PREFIX_COUNT] PROGMEM = {')
    rows = []
    for base, cont, zone, weight, is_common, digits, ent in entries:
        packed = ', '.join('0x%02X' % (digits[2 * i] | digits[2 * i + 1] << 4) for i in range(5))
        rows.append('    {%-7s 0x%02X, %2d, %3d, %d, {%s}}' %
                    ('"%s",' % base, cont, zone, weight, 1 if is_common else 0, packed))
    out.append(',\n'.join(rows))
    out.append('};')
    out.append('')
    out.append('#endif // CALLSIGN_PREFIXES_H')
    open(args.output, 'w').write('\n'.join(out) + '\n')

    print('wrote %s: %d entries (%d bytes), %d heard, %d common' %
          (args.output, len(entries), len(entries) * 14, seen, common))
    print('calls used: %d of %d; skipped: %s' % (sum(parent_calls.values()), len(calls), dict(skipped)))
    if not_own:
        print('base prefixes cty.dat does not list as prefixes of their own: %s' % ' '.join(not_own))
    if dropped:
        print('base prefixes unknown to cty.dat, left out: %s' % ' '.join(dropped))


if __name__ == '__main__':
    sys.exit(main())
