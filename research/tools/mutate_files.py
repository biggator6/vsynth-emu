#!/usr/bin/env python3
# mutate_files.py [name ...]: every deliberate error in files_muts.txt (or the named ones), each planted in XtFiles.cpp, or
# XtFirmware.cpp with port filesfw, by mutate.sh and run through xt_files against fcheck's baseline.  One line each: caught,
# MISSED, or ERROR when the old text is no longer in the source.  files_muts.txt: name|old|new[|port], with \n and \| escaped.
import os, re, subprocess, sys
D = os.path.dirname(os.path.abspath(__file__))
names = sys.argv[1:]
muts = []
for line in open(D + '/files_muts.txt'):
    line = line.rstrip('\n')
    if not line: continue
    p = re.split(r'(?<!\\)\|', line)
    f = lambda x: x.replace('\\n', '\n').replace('\\|', '|')
    muts.append((p[0], f(p[1]), f(p[2]) if len(p) > 2 else '', p[3] if len(p) > 3 else 'files'))
if names:
    unknown = set(names) - {m[0] for m in muts}
    if unknown: sys.exit('mutate_files: not in files_muts.txt: ' + ' '.join(sorted(unknown)))
    muts = [m for m in muts if m[0] in names]
missed = errors = 0
for m in muts:
    r = subprocess.run(['bash', D + '/mutate.sh', m[0], m[1], m[2], '0', '^$'], env=dict(os.environ, PORT=m[3]), capture_output=True, text=True)
    out = r.stdout + r.stderr
    n = re.search(r'^files: (\d+) lines', out, re.M)
    if not n: errors += 1; print('ERROR  ', m[0].ljust(18), (out.strip().splitlines() or [''])[-1][:100]); continue
    k = int(n.group(1))
    if k == 0: missed += 1
    print(('MISSED ' if k == 0 else 'caught ') + m[0].ljust(18), k, 'lines')
print('planted', len(muts), 'missed', missed, 'errors', errors)
sys.exit(1 if missed or errors else 0)
