"""mutlist.py: the planted-error lists of the mutate_*.py runners (tone_muts.txt, enc_muts.txt, files_muts.txt,
fx_muts.txt, fxc_muts.txt). A line is one plant, name|old|new[|field ...], with \\n and \\| escaped; a blank line or
one starting with # is none. And run_plants, a runner for a harness of its own that says "<harness>: N different"
(mutate_msm.py's, v199, and mutate_vc2.py's)."""
import concurrent.futures as cf
import os
import re
import signal
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent


def changed_sources(rev):
    """The names of plugin/Source's files that differ between rev and the working tree: a plant in one of them may no
    longer mean what it did, and only running it again shows it (the runners' --changed)"""
    diff = subprocess.run(['git', '-C', str(REPO), 'diff', '--name-only', rev, '--', 'plugin/Source'],
                          capture_output=True, text=True, check=True).stdout.split()
    return {os.path.basename(f) for f in diff}


def changed_lines(listing, rev):
    """The names of the plants whose line in the list is new, or not as it was in rev's copy of the list"""
    then = subprocess.run(['git', '-C', str(REPO), 'show', f'{rev}:research/tools/{listing}'],
                          capture_output=True, text=True)
    old = set(then.stdout.split('\n')) if then.returncode == 0 else set()
    now = (HERE / listing).read_text().split('\n')
    return {re.split(r'(?<!\\)\|', line)[0] for line in now if line and not line.startswith('#') and line not in old}


def _text(field):
    return field.replace('\\n', '\n').replace('\\|', '|')


def plants(path, names, runner, defaults=()):
    """The list's plants as (name, old, new, *fields): each field after new is its default where the line has none.
    With names, only those, in the list's order; a name the list doesn't have stops the runner."""
    found = []
    for line in Path(path).read_text().split('\n'):
        if not line or line.startswith('#'):
            continue
        p = re.split(r'(?<!\\)\|', line)
        more = [p[3 + i] if len(p) > 3 + i else d for i, d in enumerate(defaults)]
        found.append((p[0], _text(p[1]), _text(p[2]) if len(p) > 2 else '', *more))
    if not names:
        return found
    unknown = set(names) - {m[0] for m in found}
    if unknown:
        sys.exit(f'{runner}: not in {Path(path).name}: ' + ' '.join(sorted(unknown)))
    return [m for m in found if m[0] in names]


@dataclass(frozen=True)
class Harness:
    """A runner's harness: the runner's name, the harness that says "<harness>: N different", its list of
    plants, the port and seed a plant has when its line gives none, and the harness's switch to stop at its first
    case that differs"""
    runner: str
    harness: str
    listing: str
    defaults: tuple
    stop: str = ''


def run_plants(h, names, cases):
    """Every plant in the harness's list (or the named ones), each built by mutate.sh with its port and seed and run
    through the harness on `cases`, several at a time (JOBS, 6).  One line each: caught (or "stopped", when it crashes
    or hangs), MISSED, or ERROR when the old text is no longer in the source or the run never started.  It stops first
    if a run with nothing planted doesn't say 0 different, for each port and seed the list uses.  The exit status."""
    muts = plants(HERE / h.listing, names, h.runner, h.defaults)
    total = re.compile(rf'^{h.harness}: (\d+) different', re.M)

    def run(name, old, new, port, seed):
        """Its verdict: (status, detail)"""
        env = dict(os.environ, PORT=port, SEED=seed, **({h.stop: '1'} if h.stop else {}))
        p = subprocess.Popen(['bash', str(HERE / 'mutate.sh'), name, old, new, cases], env=env, stdout=subprocess.PIPE,
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
        found = total.search(out)
        if not found:
            return 'caught ', 'stopped: ' + last
        return ('caught ', last) if int(found.group(1)) else ('MISSED ', '')

    # With nothing planted, which also builds the shared objects.  If one of these fails, every plant's would, and
    # would read as caught.
    for port, seed in sorted({(m[3], m[4]) for m in muts}):
        c = next(m for m in muts if (m[3], m[4]) == (port, seed))
        status, detail = run('_control-' + port, c[1], c[1], port, seed)
        if status != 'MISSED ':
            sys.exit(f'{h.runner}: the run with nothing planted ({port}, seed {seed}) fails, so no plant would mean '
                     f'anything: {detail}')
    with cf.ThreadPoolExecutor(int(os.environ.get('JOBS', '6'))) as ex:
        res = list(ex.map(lambda m: (m[0], *run(*m)), muts))
    for name, status, detail in res:
        print(status + name.ljust(22), detail)
    print('planted', len(res), 'missed', sum(r[1] == 'MISSED ' for r in res), 'errors',
          sum(r[1] == 'ERROR  ' for r in res))
    return 1 if any(r[1] != 'caught ' for r in res) else 0


def main(h, cases, argv):
    """A runner of a harness of its own: [cases] [name ...] from argv, `cases` when the first isn't a number, then
    run_plants's exit status (mutate_msm.py's and mutate_midi.py's, v462)"""
    args = list(argv)
    if args and args[0].isdigit():
        cases = args.pop(0)
    return run_plants(h, args, cases)
