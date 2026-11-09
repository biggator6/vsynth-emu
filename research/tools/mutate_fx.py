#!/usr/bin/env python3
# mutate_fx.py [--control] [samples] [name ...]: every deliberate error in fx_muts.txt (or the named ones), each planted in XtFxChip.cpp by mutate.sh (PORT=fx) and run through xt_fx,
# several at a time.  --control: fxc_muts.txt, in XtFxControl.cpp (PORT=fxc), through xt_fx control, where the number is its events.  One line each: caught and in how many cases (or "stopped",
# when it crashes or hangs), MISSED, or ERROR when the old text is no longer in the source or the run never started.  It stops first if the run with nothing planted doesn't say 0 different.
# The lists: name|old|new, with \\n and \\| escaped.
import concurrent.futures as cf, os, re, signal, subprocess, sys
import mutlist
D = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
control = '--control' in args
if control: args.remove('--control')
samples = args.pop(0) if args and args[0].isdigit() else ('60' if control else '4096')
port, listing, source, tag = ('fxc', 'fxc_muts.txt', 'XtFxControl::boot', 'fxc_') if control else ('fx', 'fx_muts.txt', 'XtFxChip::reset', 'fx_')
env = dict(os.environ, PORT=port, XT_FX_STOP='1')
muts = mutlist.plants(D + '/' + listing, args, 'mutate_fx')
def results(out): return [l for l in out.splitlines() if re.search(r': \d+ (samples|edits), \d+ different', l)]
# With nothing planted, which also builds the shared objects.  If this run fails, every plant's would, and would read as caught.
base = subprocess.run(['bash', D + '/mutate.sh', tag + '_control', source, source, samples, 'different'], env=env, capture_output=True, text=True, timeout=600)
base_out = base.stdout + base.stderr
if not results(base_out) or (control and len(results(base_out)) < 8) or any(not re.search(r', 0 different', l) for l in results(base_out)):
    sys.exit('mutate_fx: the run with nothing planted fails, so no plant would mean anything: ' + (base_out.strip().splitlines() or ['no output'])[-1][:120])
def one(m):
    p = subprocess.Popen(['bash', D + '/mutate.sh', tag + m[0], m[1], m[2], samples, 'different'], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, start_new_session=True)
    try:
        out, _ = p.communicate(timeout=int(os.environ.get('MUT_TIMEOUT', '150')))
    except subprocess.TimeoutExpired:                  # a planted error can hang the check itself, which counts: the whole group goes, or the binary would run on
        os.killpg(p.pid, signal.SIGKILL); p.communicate()
        return ('caught ', m[0], 0, 'stopped: it hung')
    lines = results(out)
    if 'not in' in out or ' error' in out or '== ' + tag + m[0] not in out: return ('ERROR  ', m[0], 0, (out.strip().splitlines() or [''])[-1][:110])   # never ran: not a crash of the plant's
    if control and len(lines) < 8: return ('caught ', m[0], 0, 'stopped: it crashed')
    if not lines: return ('ERROR  ', m[0], 0, (out.strip().splitlines() or [''])[-1][:110])
    bad = [l for l in lines if not re.search(r', 0 different', l)]
    return ('caught ' if bad else 'MISSED ', m[0], len(bad), bad[0][:70] if bad else '')
with cf.ThreadPoolExecutor(int(os.environ.get('JOBS', '6'))) as ex:
    res = list(ex.map(one, muts))
for st, n, k, s in res: print(st + n.ljust(18), str(k).rjust(3), s)
print('planted', len(res), 'missed', sum(r[0] == 'MISSED ' for r in res), 'errors', sum(r[0] == 'ERROR  ' for r in res))
