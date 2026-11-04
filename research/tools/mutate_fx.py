#!/usr/bin/env python3
# mutate_fx.py [samples] [name ...]: every deliberate error in fx_muts.txt (or the named ones), each planted in XtFxChip.cpp by mutate.sh (PORT=fx) and run through xt_fx,
# several at a time.  One line each: caught and in how many cases, MISSED, or ERROR when the old text is no longer in the source.  fx_muts.txt: name|old|new, with \\n and \\| escaped.
import concurrent.futures as cf, os, re, subprocess, sys
D = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
samples = args.pop(0) if args and args[0].isdigit() else '4096'
env = dict(os.environ, PORT='fx', XT_FX_STOP='1')
muts = []
for line in open(D + '/fx_muts.txt'):
    line = line.rstrip('\n')
    if not line or line.startswith('#'): continue
    p = re.split(r'(?<!\\)\|', line)
    f = lambda x: x.replace('\\n', '\n').replace('\\|', '|')
    muts.append((p[0], f(p[1]), f(p[2]) if len(p) > 2 else ''))
if args:
    unknown = set(args) - {m[0] for m in muts}
    if unknown: sys.exit('mutate_fx: not in fx_muts.txt: ' + ' '.join(sorted(unknown)))
    muts = [m for m in muts if m[0] in args]
subprocess.run(['bash', D + '/mutate.sh', '_prebuild', 'XtFxChip::reset', 'XtFxChip::reset', '1', 'nothing'], env=env, capture_output=True)   # the shared object, once
def one(m):
    r = subprocess.run(['bash', D + '/mutate.sh', 'fx_' + m[0], m[1], m[2], samples, 'different'], env=env, capture_output=True, text=True)
    out = r.stdout + r.stderr
    lines = [l for l in out.splitlines() if re.search(r': \d+ samples, \d+ different', l)]
    if not lines: return ('ERROR  ', m[0], 0, (out.strip().splitlines() or [''])[-1][:110])
    bad = [l for l in lines if not re.search(r', 0 different', l)]
    return ('caught ' if bad else 'MISSED ', m[0], len(bad), bad[0][:70] if bad else '')
with cf.ThreadPoolExecutor(int(os.environ.get('JOBS', '6'))) as ex:
    res = list(ex.map(one, muts))
for st, n, k, s in res: print(st + n.ljust(18), str(k).rjust(3), s)
print('planted', len(res), 'missed', sum(r[0] == 'MISSED ' for r in res), 'errors', sum(r[0] == 'ERROR  ' for r in res))
