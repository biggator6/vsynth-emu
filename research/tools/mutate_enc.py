#!/usr/bin/env python3
# mutate_enc.py [name ...]: every deliberate error in enc_muts.txt (or the named ones),
# each planted in XtSoloEncoder.cpp by mutate.sh (PORT=enc) and run through xt_enc on
# fcheck's waves.  One line each: caught and on how many waves, MISSED, or ERROR when the
# old text is no longer in the source.  enc_muts.txt: name|old|new, with \n and \| escaped.
import os, re, subprocess, sys
D = os.path.dirname(os.path.abspath(__file__))
names = sys.argv[1:]
env = dict(os.environ, PORT='enc')
muts = []
for line in open(D + '/enc_muts.txt'):
    line = line.rstrip('\n')
    if not line: continue
    p = re.split(r'(?<!\\)\|', line)
    f = lambda x: x.replace('\\n', '\n').replace('\\|', '|')
    muts.append((p[0], f(p[1]), f(p[2]) if len(p) > 2 else ''))
if names:
    unknown = set(names) - {m[0] for m in muts}
    if unknown: sys.exit('mutate_enc: not in enc_muts.txt: ' + ' '.join(sorted(unknown)))
    muts = [m for m in muts if m[0] in names]
missed = errors = 0
for m in muts:
    r = subprocess.run(['bash', D + '/mutate.sh', m[0], m[1], m[2], '0', 'different|no result|TILE'], env=env, capture_output=True, text=True)
    out = r.stdout + r.stderr
    n = re.search(r'^enc: (\d+) waves', out, re.M)
    if not n: errors += 1; print('ERROR  ', m[0].ljust(16), (out.strip().splitlines() or [''])[-1][:100]); continue
    k = int(n.group(1))
    if k == 0: missed += 1
    lines = [l for l in out.splitlines() if l.startswith('enc ')]
    print(('MISSED ' if k == 0 else 'caught ') + m[0].ljust(16), k, 'waves', (lines[0][:90] if lines else ''))
print('planted', len(muts), 'missed', missed, 'errors', errors)
