#!/usr/bin/env python3
# mutate_tone.py [cases]: every deliberate error in tone_muts.txt, each planted in
# XtSynth.cpp by mutate.sh (PORT=tone) and run through xt_tone.  One line each:
# caught, MISSED, or ERROR when the old text is no longer in the source.  From the
# repo root.  tone_muts.txt: name|old|new, with \n and \| escaped.
import concurrent.futures as cf, os, re, subprocess, sys
D = os.path.dirname(os.path.abspath(__file__))
cases = sys.argv[1] if len(sys.argv) > 1 else '2000'
env = dict(os.environ, PORT='tone')
muts = []
for line in open(D + '/tone_muts.txt'):
    line = line.rstrip('\n')
    if not line: continue
    p = re.split(r'(?<!\\)\|', line)
    f = lambda x: x.replace('\\n', '\n').replace('\\|', '|')
    muts.append((p[0], f(p[1]), f(p[2]) if len(p) > 2 else ''))
script = D + '/mutate.sh'
subprocess.run(['bash', script, 'zz-base', 'constexpr float kLand', 'constexpr float kLand', '1'], env=env, capture_output=True)   # builds the shared objects
def run(m):
    r = subprocess.run(['bash', script, m[0], m[1], m[2], cases, 'different'], env=env, capture_output=True, text=True)
    out = r.stdout + r.stderr
    tot = re.findall(r'^tone[^:\n]*: (\d+) different', out, re.M)
    return m[0], f'tone: {sum(map(int, tot))} different' if tot else None
with cf.ThreadPoolExecutor(8) as ex:
    res = list(ex.map(run, muts))
missed = errors = 0
for n, t in res:
    if t is None: errors += 1
    elif t.startswith('tone: 0 '): missed += 1
    print(('ERROR  ' if t is None else 'MISSED ' if t.startswith('tone: 0 ') else 'caught ') + n.ljust(20), t or '')
print('planted', len(res), 'missed', missed, 'errors', errors)
