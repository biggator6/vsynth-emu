#!/usr/bin/env python3
# mutate_tone.py [cases] [name ...]: every deliberate error in tone_muts.txt (or the
# named ones), each planted in XtSynth.cpp by mutate.sh (PORT=tone) and run through
# xt_tone, which stops at its first difference (XT_TONE_STOP): the verdict is the
# whole run's, for the cases up to it.  One line each: caught (a run the error crashes
# counts), MISSED, or ERROR when the old text is no longer in the source or the run never
# started.  It stops first if a run with nothing planted doesn't say 0 different, on each
# seed and for solocmp, whichever the list uses.  From the repo root.  tone_muts.txt:
# name|old|new[|port[|seed[|mode]]], with \n and \| escaped; port is mutate.sh's PORT for an error in
# another file than XtSynth.cpp (toneosc, XtOsc.cpp), solo for one only `solocmp` reaches:
# very short records, or a timer that overflows.  seed is for one the default seed's cases
# don't happen to reach, so fcheck's run doesn't guard it.  A port wire… (v193) is the Multi Step Modulator's wiring,
# run through xt_tone's MSM mode (`xt_tone rom msm`), [cases] its cases.  mode extin (v203): through xt_tone's EXT IN
# mode (`xt_tone rom extin`), the only run that draws EXT IN.
# --changed <rev> first (2026-11-21, the user's rule): only the plants whose port's file (mutate_tonefile.sh's
# tonefile, mutate.sh's own table) differs between rev and the working tree; the whole list when one of those files is
# xt_tone's own check's, as every plant's verdict is the check's.  --list: say which plants a run would plant, and stop.
import concurrent.futures as cf, os, re, subprocess, sys
import mutlist
D = os.path.dirname(os.path.abspath(__file__))
CHECK = ('tonecheck', 'tonefw', 'tonefwcpp', 'tonefwh', 'tonecases', 'tonecarry', 'tonepatchids',
         'wirecheck', 'wirefwmsm')   # the ports whose files are xt_tone's own check's
args = [a for a in sys.argv[1:] if a != '--list']
rev = args[1] if args[:1] == ['--changed'] else None
args = args[2:] if rev else args
cases = args[0] if args else '2000'
env = dict(os.environ, PORT='tone', XT_TONE_STOP='1')
muts = mutlist.plants(D + '/tone_muts.txt', args[1:], 'mutate_tone', ('tone', '', ''))
SAY_FILES = 'source "$0"; for p; do echo "$(tonefile "$p")"; done'   # each port's file, a line each
def changedPlants(muts):
    # each port's file by tonefile (solo's plants are XtSoloVoice.cpp's, as tonesolo's), against git's files changed
    ports = sorted({m[3] for m in muts})
    asked = ['tonesolo' if p == 'solo' else p for p in ports]
    said = subprocess.run(['bash', '-c', SAY_FILES, D + '/mutate_tonefile.sh', *asked],
                          capture_output=True, text=True, check=True).stdout.split('\n')
    files = dict(zip(ports, said))
    diff = subprocess.run(['git', '-C', D + '/../..', 'diff', '--name-only', rev, '--', 'plugin/Source'],
                          capture_output=True, text=True, check=True).stdout.split()
    changed = {os.path.basename(f) for f in diff}
    hit = {p for p in ports if files[p] in changed}
    print(f'mutate_tone: changed against {rev}, with plants:',
          ' '.join(sorted({files[p] for p in hit})) or 'none', flush=True)
    if hit & set(CHECK):
        print('mutate_tone: xt_tone\'s own check changed, so the whole list', flush=True)
        return muts
    return [m for m in muts if m[3] in hit]
muts = changedPlants(muts) if rev else muts
if '--list' in sys.argv:
    for m in muts: print(m[0], m[3])
    print('mutate_tone:', len(muts), 'plants')
    sys.exit(0)
if not muts: sys.exit(0)
script = D + '/mutate.sh'
subprocess.run(['bash', script, 'zz-base', 'constexpr float kLand', 'constexpr float kLand', '1'], env=env, capture_output=True)   # builds the shared objects
def plantEnv(m):
    # its port, and the seed and xt_tone mode (v203: extin, `xt_tone rom extin`) its line gives
    return dict(env, PORT=m[3], **({'SEED': m[4]} if m[4] else {}), **({'TONE_MODE': m[5]} if m[5] else {}))
def run(m, pattern=None):
    # solocmp: its verdict is DIFFERENT, as a run that dies says nothing with "different" in it, and that counts
    verdict = pattern or ('DIFFERENT' if m[3] == 'solo' else 'different')
    r = subprocess.run(['bash', script, m[0], m[1], m[2], cases, verdict], env=plantEnv(m),
                       capture_output=True, text=True)
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
# (its text in XtSynth.cpp: kLand went to XtSynthConstants.h, v203, and render to XtSynthOut.cpp, v204)
KEPT = 'void XtSynth::prepare('
controls = [('zz-control-' + (md + '-' if md else '') + (s or 'default'), KEPT, KEPT, 'tone', s, md)
            for s, md in sorted({(x[4], x[5]) for x in muts if x[3] != 'solo'})]
controls += [('zz-control-solo', x[1], x[1], 'solo', '', '') for x in muts if x[3] == 'solo'][:1]
# the Multi Step Modulator's wiring (port wire…, v193) runs xt_tone's MSM mode: its own run with nothing planted, by seed
wire = sorted({x[4] for x in muts if x[3].startswith('wire')})
controls += [('zz-control-msm-' + (s or 'default'), '#include "XtSynth.h"', '#include "XtSynth.h"', 'wiremsm', s, '')
             for s in wire]
def control(c):
    return run(c, 'same|DIFFERENT' if c[3] == 'solo' else 'different')
with cf.ThreadPoolExecutor(8) as ex:
    for (n, t), c in zip(ex.map(control, controls), controls):
        if t != 'tone: 0 different':
            how = f'{c[3]}, seed {c[4] or "default"}' + (f', mode {c[5]}' if c[5] else '')
            sys.exit(f'mutate_tone: the run with nothing planted ({how}) says {t or "nothing"}, '
                     'so no plant would mean anything')
    # each line said as soon as it and those before it are in, in the list's order, so a long run shows how far it is
    missed = errors = 0
    for n, t in ex.map(run, muts):
        if t is None: errors += 1
        elif t.startswith('tone: 0 '): missed += 1
        print(('ERROR  ' if t is None else 'MISSED ' if t.startswith('tone: 0 ') else 'caught ') + n.ljust(20), t or '',
              flush=True)
print('planted', len(muts), 'missed', missed, 'errors', errors)
