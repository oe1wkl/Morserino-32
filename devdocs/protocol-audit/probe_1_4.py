#!/usr/bin/env python3
"""Hardware check for the protocol 1.4 commands (M32 Pocket, USB).

    python3 probe_1_4.py

Needs pyserial. Adjust PORT below. Read-only: it never clears anything.
Notes for this board: opening the port RESETS it, so the script waits out the
boot; and the CDC port only yields bytes with dtr=False, rts=False.
Written for the 2026-08-19 verification of `GET configs/details`,
`GET capabilities` and `GET game/scores`; see RESOLUTION_PLAN.md.
"""
import serial, time, json, sys

PORT='/dev/cu.usbmodem12301'

def frame(buf):
    """The same string-aware framing the config tool now uses."""
    bc=0; js=-1; inStr=False; esc=False
    for i,c in enumerate(buf):
        if js<0:
            if c=='{': js=i; bc=1
            continue
        if esc: esc=False; continue
        if inStr:
            if c=='\\': esc=True
            elif c=='"': inStr=False
            continue
        if c=='"': inStr=True
        elif c=='{': bc+=1
        elif c=='}':
            bc-=1
            if bc==0: return buf[js:i+1], buf[i+1:]
    return None, buf

class M32:
    def __init__(self):
        self.s=serial.Serial()
        self.s.port=PORT; self.s.baudrate=115200; self.s.timeout=0.1
        self.s.dtr=False; self.s.rts=False        # both True yields zero bytes on this CDC port
        self.s.open()
        self.buf=''
        time.sleep(10.0)                    # opening the port resets the board; wait out the boot+splash
        self.s.reset_input_buffer()
    def cmd(self, c, timeout=6.0):
        self.buf=''
        self.s.write((c+'\n').encode())
        self.s.flush()
        t0=time.time()
        while time.time()-t0 < timeout:
            n=self.s.in_waiting
            if n: self.buf += self.s.read(n).decode('utf-8','replace')
            resp,rest = frame(self.buf)
            if resp:
                self.buf=rest
                return json.loads(resp)
            time.sleep(0.02)
        raise TimeoutError('no reply to %r (partial: %r)' % (c, self.buf[:120]))
    def close(self):
        try: self.s.write(b'put device/protocol/off\n'); self.s.flush(); time.sleep(0.3)
        except Exception: pass
        self.s.close()

fails=[]
def check(label, cond, detail=''):
    print(('  PASS  ' if cond else '  FAIL  ')+label+(('   '+detail) if detail else ''))
    if not cond: fails.append(label)

m=M32()
try:
    print('\n== handshake ==')
    d=m.cmd('put device/protocol/on')
    dev=d.get('device',{})
    print('   ', json.dumps(dev))
    check('protocol reports 1.4', dev.get('protocol')=='1.4', repr(dev.get('protocol')))

    print('\n== GET capabilities ==')
    t0=time.time(); c=m.cmd('get capabilities'); dt=time.time()-t0
    print('   ', json.dumps(c), ' (%.3f s)'%dt)
    feats=c.get('capabilities',{}).get('features',[])
    check('capabilities.protocol == device.protocol', c.get('capabilities',{}).get('protocol')==dev.get('protocol'))
    check('lists configs/details', 'configs/details' in feats)
    check('lists game/scores (Pocket build)', 'game/scores' in feats)

    print('\n== GET configs (baseline) ==')
    t0=time.time(); base=m.cmd('get configs', 8.0); t_configs=time.time()-t0
    names=[x['name'] for x in base['configs']]
    print('    %d parameters in %.3f s' % (len(names), t_configs))

    print('\n== GET configs/details (paged) ==')
    got={}; order=[]; frm=0; pages=0; t0=time.time()
    while pages < 64:
        pages+=1
        r=m.cmd('get configs/details'+(('/%d'%frm) if frm else ''), 8.0)
        cd=r['configdetails']
        print('    page %d: from=%-3d count=%-2d total=%-3d more=%s'
              % (pages, cd['from'], cd['count'], cd['total'], cd['more']))
        check('  page start matches request', cd['from']==frm, 'asked %d got %d'%(frm,cd['from']))
        check('  count matches items length', cd['count']==len(cd['items']))
        for it in cd['items']:
            got[it['name']]=it; order.append(it['name'])
        if not cd['more']: break
        frm=cd['from']+cd['count']
    t_bulk=time.time()-t0
    print('    %d parameters in %d pages, %.3f s' % (len(order), pages, t_bulk))
    check('same count as GET configs', len(order)==len(names), '%d vs %d'%(len(order),len(names)))
    check('same ORDER as GET configs', order==names)
    check('total matches reality', cd['total']==len(names))

    print('\n== item fidelity vs GET config/<name> ==')
    for probe in (names[0], names[len(names)//2], names[-1]):
        single=m.cmd('get config/'+probe)['config']
        check('identical: '+probe, single==got[probe],
              '' if single==got[probe] else json.dumps({'single':single,'bulk':got[probe]})[:300])

    print('\n== error handling ==')
    bad=m.cmd('get configs/details/%d' % (len(names)+5))
    print('   ', json.dumps(bad))
    check('out-of-range page -> error', 'error' in bad and 'INVALID PARAMETER' in bad['error'].get('content',''))
    bad2=m.cmd('get configs/details/xyz')
    check('non-numeric page -> error', 'error' in bad2, json.dumps(bad2)[:120])
    bad3=m.cmd('get configs/nonsense')
    check('unknown configs sub-command -> error', 'error' in bad3, json.dumps(bad3)[:120])
    still=m.cmd('get configs/details')
    check('recovers after errors', 'configdetails' in still)

    print('\n== GET game/scores ==')
    t0=time.time(); g=m.cmd('get game/scores', 8.0); dt=time.time()-t0
    gs=g.get('gamescores',{}).get('games',[])
    print('    %d games in %.3f s' % (len(gs), dt))
    for e in gs:
        print('      %-20s %-28s %s' % (e.get('id'), e.get('name'),
              ('saved=%s'%e['saved']) if 'saved' in e else ('%d score row(s)'%len(e.get('scores',[])))))
        for row in e.get('scores',[])[:3]:
            print('          '+json.dumps(row))
    ids=[e.get('id') for e in gs]
    check('all 7 games present', ids==['invaders','morsel','trailblazer','foxhunt',
                                       'memorychain-chars','memorychain-calls','radiocave'], str(ids))
    check('radiocave carries "saved"', any(e.get('id')=='radiocave' and 'saved' in e for e in gs))

    print('\n== timing summary ==')
    print('    GET configs (48 names)      %.3f s' % t_configs)
    print('    bulk details, %d pages       %.3f s' % (pages, t_bulk))
    t0=time.time()
    for n in names[:8]: m.cmd('get config/'+n)
    t_one=time.time()-t0
    print('    8 single GET config/<name>  %.3f s  -> all %d would be ~%.1f s'
          % (t_one, len(names), t_one/8*len(names)))
    print('    speed-up on the full read:  %.1fx' % ((t_one/8*len(names))/t_bulk))
finally:
    m.close()

print('\n'+('ALL CHECKS PASSED' if not fails else 'FAILURES: '+', '.join(fails)))
sys.exit(1 if fails else 0)
