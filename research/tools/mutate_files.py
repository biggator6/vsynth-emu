#!/usr/bin/env python3
# mutate_files.py [name ...]: every deliberate error in files_muts.txt (or the named ones), each planted in XtFiles.cpp, or
# XtFirmware.cpp with port filesfw, by mutate.sh and run through xt_files against fcheck's baseline.  One line each: caught,
# MISSED, or ERROR when the old text is no longer in the source or the run never started.  It stops first if a run with
# nothing planted doesn't equal the baseline, for each port the list uses.  files_muts.txt: name|old|new[|port], with \n and \| escaped.
import os, re, subprocess, sys
import mutlist
D = os.path.dirname(os.path.abspath(__file__))
muts = mutlist.plants(D + '/files_muts.txt', sys.argv[1:], 'mutate_files', ('files',))
def run(m):
    r = subprocess.run(['bash', D + '/mutate.sh', m[0], m[1], m[2], '0', '^$'], env=dict(os.environ, PORT=m[3]), capture_output=True, text=True)
    out = r.stdout + r.stderr
    return out, re.search(r'^files: (\d+) lines', out, re.M)
# With nothing planted: an old text put back as it was, once per port.  If this run fails, every plant's would, and would read as caught.
for port in sorted({m[3] for m in muts}):
    c = next(m for m in muts if m[3] == port)
    out, n = run(('_control-' + port, c[1], c[1], port))
    if not n or int(n.group(1)) != 0:
        sys.exit(f'mutate_files: the run with nothing planted ({port}) fails, so no plant would mean anything: ' + (out.strip().splitlines() or ['no output'])[-1][:120])
missed = errors = 0
for m in muts:
    out, n = run(m)
    if not n: errors += 1; print('ERROR  ', m[0].ljust(18), (out.strip().splitlines() or [''])[-1][:100]); continue
    k = int(n.group(1))
    if k == 0: missed += 1
    print(('MISSED ' if k == 0 else 'caught ') + m[0].ljust(18), k, 'lines')
print('planted', len(muts), 'missed', missed, 'errors', errors)
sys.exit(1 if missed or errors else 0)
