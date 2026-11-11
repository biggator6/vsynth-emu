#!/usr/bin/env python3
# mutate_msm.py [cases] [name ...]: every deliberate error in msm_muts.txt (or the named ones), each planted by mutate.sh in
# XtMsm.cpp, or by its port in XtMsmTrack.cpp (msmtrack) or XtMsmPatch.cpp (msmpatch), and run through xt_msm on [cases]
# random cases (300) after the presets, each to its first case that differs (XT_MSM_STOP), several at a time.  One line
# each: caught (or "stopped", when it crashes or hangs), MISSED, or ERROR when the old text is no longer in the source or
# the run never started.  It stops first if a run with nothing planted doesn't say 0 different, for each port and seed
# the list uses.  msm_muts.txt: name|old|new[|port[|seed]], with \n and \| escaped.
import concurrent.futures as cf, os, re, signal, subprocess, sys
import mutlist
D = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
cases = args.pop(0) if args and args[0].isdigit() else '300'
muts = mutlist.plants(D + '/msm_muts.txt', args, 'mutate_msm', ('msm', '2024'))
TOTAL = re.compile(r'^xt_msm: (\d+) different', re.M)


def run(name, old, new, port, seed):
    """Its verdict: (status, detail)"""
    env = dict(os.environ, PORT=port, SEED=seed, XT_MSM_STOP='1')
    p = subprocess.Popen(['bash', D + '/mutate.sh', name, old, new, cases], env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, start_new_session=True)
    try:
        out, _ = p.communicate(timeout=int(os.environ.get('MUT_TIMEOUT', '300')))
    except subprocess.TimeoutExpired:   # it hung: the whole group goes, or the binary would run on
        os.killpg(p.pid, signal.SIGKILL)
        p.communicate()
        return 'caught ', 'stopped: it hung'
    last = (out.strip().splitlines() or [''])[-1][:110]
    if 'not in' in out or ' error' in out or '== ' + name not in out:
        return 'ERROR  ', last
    total = TOTAL.search(out)
    if not total:
        return 'caught ', 'stopped: ' + last
    return ('caught ', last) if int(total.group(1)) else ('MISSED ', '')


# With nothing planted, which also builds the shared objects.  If one of these fails, every plant's would, and would
# read as caught.
for port, seed in sorted({(m[3], m[4]) for m in muts}):
    c = next(m for m in muts if (m[3], m[4]) == (port, seed))
    status, detail = run('_control-' + port, c[1], c[1], port, seed)
    if status != 'MISSED ':
        sys.exit(f'mutate_msm: the run with nothing planted ({port}, seed {seed}) fails, so no plant would mean '
                 f'anything: {detail}')
with cf.ThreadPoolExecutor(int(os.environ.get('JOBS', '6'))) as ex:
    res = list(ex.map(lambda m: (m[0], *run(*m)), muts))
for name, status, detail in res:
    print(status + name.ljust(22), detail)
print('planted', len(res), 'missed', sum(r[1] == 'MISSED ' for r in res), 'errors', sum(r[1] == 'ERROR  ' for r in res))
sys.exit(1 if any(r[1] != 'caught ' for r in res) else 0)
