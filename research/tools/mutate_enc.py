#!/usr/bin/env python3
# mutate_enc.py [name ...]: every deliberate error in enc_muts.txt (or the named ones),
# each planted in XtSoloEncoder.cpp by mutate.sh (PORT=enc) and run through xt_enc on
# fcheck's waves.  One line each: caught and on how many waves, MISSED, or ERROR when the
# old text is no longer in the source or the run never started.  It stops first if a run with
# nothing planted doesn't equal the baseline.  enc_muts.txt: name|old|new, with \n and \| escaped.
import os, re, subprocess, sys
import mutlist
D = os.path.dirname(os.path.abspath(__file__))
env = dict(os.environ, PORT='enc')
muts = mutlist.plants(D + '/enc_muts.txt', sys.argv[1:], 'mutate_enc')
PATTERN = 'different|DIFFERENT|no result|TILE'
if muts:   # with nothing planted: the old text put back as it was.  If this run fails, no plant would mean anything.
    r = subprocess.run(['bash', D + '/mutate.sh', '_control', muts[0][1], muts[0][1], '0', PATTERN], env=env, capture_output=True, text=True)
    n = re.search(r'^enc: (\d+) waves', r.stdout, re.M)
    if not n or int(n.group(1)) != 0:
        sys.exit('mutate_enc: the run with nothing planted fails, so no plant would mean anything: ' + ((r.stdout + r.stderr).strip().splitlines() or ['no output'])[-1][:120])
missed = errors = 0
for m in muts:
    r = subprocess.run(['bash', D + '/mutate.sh', m[0], m[1], m[2], '0', PATTERN], env=env, capture_output=True, text=True)
    out = r.stdout + r.stderr
    n = re.search(r'^enc: (\d+) waves', out, re.M)
    if not n: errors += 1; print('ERROR  ', m[0].ljust(16), (out.strip().splitlines() or [''])[-1][:100]); continue
    k = int(n.group(1))
    if k == 0: missed += 1
    lines = [l for l in out.splitlines() if l.startswith('enc ')]
    print(('MISSED ' if k == 0 else 'caught ') + m[0].ljust(16), k, 'waves', (lines[0][:90] if lines else ''))
print('planted', len(muts), 'missed', missed, 'errors', errors)
