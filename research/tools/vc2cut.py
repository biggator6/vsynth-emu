#!/usr/bin/env python3
"""vc2cut.py <recording.wav> [--run vc2_run_1] [--from S] [--takes CSV] [--out DIR] [--no-cut]: checks a recording of a
VC-2 run and cuts it into takes, as fxcut.py does an effects run's (RECORDING_GUIDE.md, "VC-2 captures";
research/VC2_MODE_PLAN.md, step 5).  Needs numpy: .venv/bin/python3.

The run is found in the recording, and each take's input mark, by the shared parts (capbase.py and the rest): the
input's first 16.8 s are the effects' test wave, so its clicks place a take wherever a path carries them (the direct
sound, the EXT IN tone), and the run's timing places the rest (the vocoder's).  Then the path (the input at unity,
the click's width, the bits, the trim, the timing, a take repeated), and each run's own takes, by name (vc2checks.py,
vc2checks2.py): which input side the direct sound and the EXT IN tone carry, the Direct Sound Level's law, the EXTIN
flag, Auto Note's pitches, the vocoder's settings against their repeat's floor, the EQs, the effects' types.

A finding only a capture can give is said as INFO or LOOK, never FAIL: FAIL means the recording can't answer (the
wrong rate, nothing came back, the writes didn't land), so do it again.  The last line says whether to go on.  It
writes, beside the recording in a folder named after it, a WAV per take (its own bytes, the input's sample i at the
take's i + 0.25 s), levels.csv and report.txt.  vc2cut_test.py checks it on a simulated XT with faults planted."""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vc2probe
from caprec import Bad, Recording
from capbase import Locate, analysis_of
from capmeasure import Measure
from cappath import PathChecks
from capreport import Report, cli
from vc2checks import Vc2Checks
from vc2checks2 import Vc2Checks2
from vc2measure import Vc2Measure

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..')
RUNS = ['vc2_run_1', 'vc2_run_2', 'vc2_run_3', 'vc2_run_4', 'vc2_vsynth']
# The takes whose output carries the input's mark on time, by the direct sound: a late one is a timing fault.
DRY = {'vc2_run_1': {'main', 'main_again', 'direct_64', 'direct_16', 'master_100', 'gain_plus6', 'direct_rev'},
       'vc2_run_3': {'direct', 'peq1_direct'}}
# The takes whose silence is itself an answer; each run's checks say what that answer is, or fail it.
QUIET = {'vc2_run_1': {'main', 'main_again', 'direct_64', 'direct_16', 'direct_0', 'direct_pcm', 'direct_osc2',
                       'direct_rev', 'master_100', 'gain_plus6', 'extin_stereo', 'extin_monolr', 'extin_monol',
                       'extin_monor', 'extin_key67', 'autonote', 'autonote_retrig'},
         'vc2_run_3': {'direct', 'peq1_direct'}}


class Analysis(Vc2Checks, Vc2Checks2, Vc2Measure, Report, PathChecks, Measure, Locate):
    """A VC-2 run's analysis: the cutters' shared parts with the VC-2's measures and checks."""
    reference_names = ('main', 'main_again', 'direct', 'hireso_direct_64')
    sections = vc2probe.SECTIONS
    unity_fix = ('The input reaches the XT at another speed: play vc2probe.wav through fxplay --audio, or a DAW '
                 'that doesn\'t resample it, at 44.1 kHz.')
    fine_fix = 'Check that nothing between the Mac and the XT resamples the input.'
    no_clicks = ('Is the input reaching the XT (its cable, the INPUT or MIC Jack Switch, fxplay given --audio), and is '
                 'this the right recording?')
    probe_file = 'vc2probe.wav'
    clean_sides = 1                                     # the direct sound is mono: it may carry one input side alone
    match_floor = 0.25                                  # a dead path silences much of a run; its clicks tell a wrong file
    make_probe = staticmethod(vc2probe.probe)

    @property
    def dry(self):
        if self.run == 'vc2_run_2':                     # each take with an input is marked by the direct sound
            return {t.name for t in self.takes if t.note and 'the input silent' not in t.settings}
        return DRY.get(self.run, set())

    def check_repeat(self):
        if self.run != 'vc2_run_2':                     # the vocoder's repeat is held by its spectrum (check_vocoder)
            super().check_repeat()

    def find_triple(self, start=0, n=None):
        """The input's three clicks in each take's first seconds, by the run's timing: a mono path (the direct sound)
        has no one-sided click, and the EXT IN tone's stereo ones may lie past the recording's first minutes."""
        for t in self.takes:
            if t.note:
                found = super().find_triple(max(0, self.at(t.note_on + self.offset) - self.at(1.0)), self.at(6.0))
                if found:
                    return found
        return None

    @property
    def may_be_silent(self):
        return QUIET.get(self.run, set())

    def check_sides(self, ref):
        """The direct sound is mono by design, so the effects' leak and swap don't apply: each run's checks read the
        sides take by take (Vc2Measure.sides)."""
        self.path['sides'] = self.sides(ref)

    def next_step(self):
        i = RUNS.index(self.run)
        return 'run %d' % (i + 2) if i < 3 else 'the comparison'


def analyse(path, run=None, takes_csv=None, start=None, probe=None, only=None):
    """Everything but writing files: returns the Analysis, with its checks and takes filled in."""
    rec = Recording(path)
    if run is None:
        m = re.search(r'vc2_(run_[1-4]|vsynth)', os.path.basename(path))
        if not m:
            raise Bad('Which run is this? Say --run with one of %s.' % ', '.join(RUNS))
        run = m.group(0)
    listed = takes_csv or os.path.join(REPO, 'analysis/test_files/vc2/vc2_takes.csv')
    return analysis_of(Analysis, rec, run, listed, start, probe, only)


def main():
    cli(analyse, 'Checks a recording of a VC-2 run and cuts it into takes.', RUNS,
        'analysis/test_files/vc2/vc2_takes.csv')


if __name__ == '__main__':
    main()
