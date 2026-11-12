#!/usr/bin/env python3
# mutate_arp.py [cases] [name ...]: every deliberate error in arp_muts.txt (or the named ones), each planted by mutate.sh in
# its port's file (the list's fourth field: arp XtArpeggio.cpp, arpstep XtArpeggioStep.cpp, arpmotif XtArpeggioMotifs.cpp,
# ticks XtTempoTicks.cpp, v189) and run through xt_arp on the presets and [cases] random cases (1000, fcheck's), stopping at
# the first that differs (XT_ARP_STOP), then its tempo ticks.
# One line each: caught and where, MISSED, or ERROR when the old text is no longer in the source or the run never started.
# It stops first unless the run with nothing planted says 0 different on every port.  The list: name|old|new|port.
import concurrent.futures as cf
import os
import re
import signal
import subprocess
import sys

import mutlist

D = os.path.dirname(os.path.abspath(__file__))
PORTS = ('arp', 'arpstep', 'arpmotif', 'ticks')
SOURCE = {'arp': 'XtArpeggio::reset', 'arpstep': 'XtArpeggio::tick', 'arpmotif': 'XtArpeggio::playLine',
          'ticks': 'XtTempoTicks::take'}
args = sys.argv[1:]
cases = args.pop(0) if args and args[0].isdigit() else '1000'   # fcheck's
muts = mutlist.plants(D + '/arp_muts.txt', args, 'mutate_arp', defaults=('arp',))
# How long a plant may run.  mutate.sh's own timeout stops the binary a minute sooner: it runs it in a process group of its
# own, which killing mutate.sh's group would miss, leaving the run reading its pipe until mutate.sh's 1500 s.
LIMIT = int(os.environ.get('MUT_TIMEOUT', '300'))


def results(out):
    return [l for l in out.splitlines() if re.match(r'(presets|random|ticks): \d+ cases, .*, \d+ different$', l)]


def start(name, old, new, port):
    env = dict(os.environ, PORT=port, XT_ARP_STOP='1', MUT_TIMEOUT=str(max(LIMIT - 60, 30)))
    return subprocess.Popen(['bash', D + '/mutate.sh', name, old, new, cases, '.'], env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, start_new_session=True)


# With nothing planted, on each port (which also builds the shared objects): a run that fails would make every plant
# read as caught
for port in PORTS:
    out, _ = start('arp_control_' + port, SOURCE[port], SOURCE[port], port).communicate(timeout=1200)
    lines = results(out)
    if len(lines) != 3 or any(not l.endswith(' 0 different') for l in lines):
        sys.exit('mutate_arp: the run with nothing planted fails on ' + port + ', so no plant would mean anything: '
                 + (out.strip().splitlines() or ['no output'])[-1][:120])


def one(m):
    name, old, new, port = m
    if port not in PORTS:
        return ('ERROR  ', name, 'no port ' + port)
    p = start('arp_' + name, old, new, port)
    try:
        out, _ = p.communicate(timeout=LIMIT)
    except subprocess.TimeoutExpired:   # a plant that hangs the check counts: the whole group goes
        os.killpg(p.pid, signal.SIGKILL)
        p.communicate()
        return ('caught ', name, 'stopped: it hung')
    if 'not in' in out or ' error' in out or '== arp_' + name not in out:
        return ('ERROR  ', name, (out.strip().splitlines() or [''])[-1][:110])
    if 'the run died, signal 14' in out:   # mutate.sh's timeout
        return ('caught ', name, 'stopped: it hung')
    if 'the run died' in out:
        return ('caught ', name, 'stopped: it crashed')
    bad = [l for l in results(out) if not l.endswith(' 0 different')]
    if not results(out):
        return ('ERROR  ', name, (out.strip().splitlines() or [''])[-1][:110])
    return ('caught ' if bad else 'MISSED ', name, bad[0][:80] if bad else '')


with cf.ThreadPoolExecutor(int(os.environ.get('JOBS', '6'))) as ex:
    res = list(ex.map(one, muts))
for st, n, s in res:
    print(st + n.ljust(22), s)
print('planted', len(res), 'missed', sum(r[0] == 'MISSED ' for r in res), 'errors', sum(r[0] == 'ERROR  ' for r in res))
