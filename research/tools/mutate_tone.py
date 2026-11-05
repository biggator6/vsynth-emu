#!/usr/bin/env python3
# mutate_tone.py [cases] [name ...]: every deliberate error in tone_muts.txt (or the
# named ones), each planted in XtSynth.cpp by mutate.sh (PORT=tone) and run through
# xt_tone, which stops at its first difference (XT_TONE_STOP): the verdict is the
# whole run's, for the cases up to it.  One line each: caught (a run the error crashes
# counts), MISSED, or ERROR when the old text is no longer in the source or the run never
# started.  It stops first if a run with nothing planted doesn't say 0 different, on each
# seed and for solocmp, whichever the list uses.  From the repo root.  tone_muts.txt:
# name|old|new[|port[|seed]], with \n and \| escaped; port is mutate.sh's PORT for an error in
# another file than XtSynth.cpp (toneosc, XtOsc.cpp), solo for one only `solocmp` reaches:
# very short records, or a timer that overflows.  seed is for one the default seed's cases
# don't happen to reach, so fcheck's run doesn't guard it.
import concurrent.futures as cf, os, re, subprocess, sys
D = os.path.dirname(os.path.abspath(__file__))
cases = sys.argv[1] if len(sys.argv) > 1 else '2000'
names = sys.argv[2:]
env = dict(os.environ, PORT='tone', XT_TONE_STOP='1')
muts = []
for line in open(D + '/tone_muts.txt'):
    line = line.rstrip('\n')
    if not line: continue
    p = re.split(r'(?<!\\)\|', line)
    f = lambda x: x.replace('\\n', '\n').replace('\\|', '|')
    muts.append((p[0], f(p[1]), f(p[2]) if len(p) > 2 else '', p[3] if len(p) > 3 else 'tone', p[4] if len(p) > 4 else ''))
if names:
    unknown = set(names) - {m[0] for m in muts}
    if unknown: sys.exit('mutate_tone: not in tone_muts.txt: ' + ' '.join(sorted(unknown)))
    muts = [m for m in muts if m[0] in names]
script = D + '/mutate.sh'
subprocess.run(['bash', script, 'zz-base', 'constexpr float kLand', 'constexpr float kLand', '1'], env=env, capture_output=True)   # builds the shared objects
def run(m, pattern=None):
    # solocmp: its verdict is DIFFERENT, as a run that dies says nothing with "different" in it, and that counts
    r = subprocess.run(['bash', script, m[0], m[1], m[2], cases, pattern or ('DIFFERENT' if m[3] == 'solo' else 'different')], env=dict(env, PORT=m[3], **({'SEED': m[4]} if m[4] else {})), capture_output=True, text=True)
    out = r.stdout + r.stderr
    if pattern and m[3] == 'solo' and not re.search(r'^same:', out, re.M): return m[0], None    # nothing planted: the cases must have run, and equalled the baseline
    if m[3] == 'solo':
        if '== ' + m[0] not in out: return m[0], None
        n = len(re.findall(r'^DIFFERENT', out, re.M))
        return m[0], f'solocmp: {n} cases differ' if n else 'tone: 0 different'
    at = re.search(r'^tone: 1 different, (at case .*)$', out, re.M)
    if at: return m[0], at.group(1)
    died = re.search(r'^.*: (the run died, signal \d+)$', out, re.M)
    if died: return m[0], died.group(1)
    tot = re.findall(r'^tone[^:\n]*: (\d+) different', out, re.M)
    return m[0], f'tone: {sum(map(int, tot))} different' if tot else None
# With nothing planted, an old text put back as it was.  If this run fails, every plant's would, and would read as caught or missed for no reason of its own.
controls = [('zz-control-' + (s or 'default'), 'constexpr float kLand', 'constexpr float kLand', 'tone', s) for s in sorted({x[4] for x in muts if x[3] != 'solo'})]
controls += [('zz-control-solo', x[1], x[1], 'solo', '') for x in muts if x[3] == 'solo'][:1]
with cf.ThreadPoolExecutor(8) as ex:
    for (n, t), c in zip(ex.map(lambda c: run(c, 'same|DIFFERENT' if c[3] == 'solo' else 'different'), controls), controls):
        if t != 'tone: 0 different': sys.exit(f'mutate_tone: the run with nothing planted ({c[3]}, seed {c[4] or "default"}) says {t or "nothing"}, so no plant would mean anything')
    res = list(ex.map(run, muts))
missed = errors = 0
for n, t in res:
    if t is None: errors += 1
    elif t.startswith('tone: 0 '): missed += 1
    print(('ERROR  ' if t is None else 'MISSED ' if t.startswith('tone: 0 ') else 'caught ') + n.ljust(20), t or '')
print('planted', len(res), 'missed', missed, 'errors', errors)
