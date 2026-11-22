#!/usr/bin/env python3
"""fxcut.py <recording.wav> [--run fx_run_1] [--from S] [--take NAME] [--takes CSV] [--out DIR] [--no-cut]: checks a recording of an effects run and cuts
it into takes (RECORDING_GUIDE.md, "Effects captures"; HARDWARE_FIRMWARE.md, "The effect chip's name, and cutting the captures").  Needs numpy:
.venv/bin/python3.  --take is for a recording of one take, done by hand or by resampling: levels and the path, with nothing to compare it to.

It says, last of all, whether the run is good enough to go on from.  Before that:
  - the file: rate, width, channels, length; where the run starts in it, found from the take list and the test wave's pattern
  - each take: where the wave's first click landed, levels against the wave, against `main`, its floor, tail and clipping
  - the path: the wave at unity, clicks one sample wide, spacing exact, the channels, the width in use, whether a take repeats
  - run 1's takes, named: whether each setting reached the sound, and what it measured

It writes into <recording's folder>/<recording's name>/: a WAV per take with the recording's own bytes and format, levels.csv and report.txt.
A cut take has its click at CUT_LEAD + 0.5 s, so a sample i of the wave is sample i + CUT_LEAD of the take.  A take with no clean click (wet only,
say) is placed by the run's timing, which is good to a few milliseconds, and marked `timing`.

FAIL means do it again, LOOK means worth reading but nothing stops the next run.  The takes' settings are read off the take list, so a run made
with other --dev or --ch settings cuts the same.  fxcut_test.py checks this on simulated recordings with faults planted.

The parts every cutter shares (vc2cut.py's too): caprec.py, the WAV reader; capbase.py, the takes and where they
are; capmeasure.py, the levels; cappath.py, the path's checks; capreport.py, the report and the files.  This file
is the effects' own checks."""
import os, re, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxcapture
from caprec import Bad, Recording
# CUT_LEAD and MARK for fxcut_test.py
from capbase import CUT_LEAD, MARK, STEP_DB, LOW_STEPS, SAME_DB, Locate, analysis_of
from capmeasure import Measure
from cappath import PathChecks
from capreport import Report, cli

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..')


DRY_TAKES = {'main', 'main_again', 'multi_off', 'multi_thru', 'multi_off_m64', 'multi_thru_m64', 'master_100', 'gain_plus6', 'gain_minus6', 'hot', 'hotter'}


class Analysis(Report, PathChecks, Measure, Locate):
    """An effects run's analysis: the cutters' shared parts with the effects' own checks."""
    dry = DRY_TAKES                                     # the takes with no effect on their path: their click is on time
    reference_names = ('main', 'main_again', 'multi_off')
    sections = fxcapture.SECTIONS
    unity_fix = ('In SAMPLE, Info: Original Pitch C4 and Original Fine Tune 0, and note 60 from the run; '
                 'every window here rests on that.')
    fine_fix = 'Check Original Fine Tune.'
    no_clicks = 'Is this the right recording, with the wave `fxprobe` playing, and the level up?'
    probe_file = 'fxprobe.wav'

    @staticmethod
    def make_probe():
        frames, _ = fxcapture.probe()
        return np.frombuffer(frames, dtype='<i2').reshape(-1, 2) / 32768.0

    def check_run1(self):
        by = self.by
        d = self.delta('multi_off', 'main')
        if d is not None:
            self.add('INFO', 'MULTI with MFX off is %+.2f dB against MAIN.' % d)
        d = self.delta('multi_thru', 'multi_off')
        if d is not None:
            self.add('INFO', 'MFX Thru is %+.2f dB against MFX off.' % d)
        system_fix = 'The System writes aren\'t reaching the chip: tell Claude, as the run depends on them.'
        self.check_setting('master_100', 'main', None, 'Master Level 100', system_fix)
        self.check_setting('gain_plus6', 'main', 6.0, 'Output Gain +6', system_fix)
        self.check_setting('gain_minus6', 'main', -6.0, 'Output Gain -6', system_fix)
        d = self.delta('multi_thru_m64', 'multi_thru')
        if d is not None:
            if abs(d) < SAME_DB:
                self.add('FAIL', 'MFX Master Level 64 did nothing to `multi_thru`. The patch\'s MFX writes aren\'t landing: tell Claude.')
            else:
                self.add('OK', 'MFX Master Level 64: %+.2f dB with Thru.' % d)
        d = self.delta('multi_off_m64', 'multi_off')
        if d is not None:
            self.add('OK' if abs(d) >= SAME_DB else 'LOOK', 'MFX Master Level 64: %+.2f dB with the Switch off%s.' % (d, '' if abs(d) >= SAME_DB else ', so it may not apply there, against the model'))
        flat, plus, minus, total = by.get('peq_flat'), by.get('peq_mid_plus15'), by.get('peq_mid_minus15'), by.get('peq_total_plus15')
        def ready(*ts):
            return all(t is not None and t.present and not t.cut_short and t.name not in self.silent for t in ts)
        if ready(flat, plus):
            v, name, clipped = self.response(plus, flat, [800.0])
            self.peq_result('Mid 1 Gain +15', v[0], 15.0, name, clipped)
        if ready(flat, minus):
            v, name, clipped = self.response(minus, flat, [800.0])
            self.peq_result('Mid 1 Gain -15', v[0], -15.0, name, clipped)
        self.check_setting('peq_total_plus15', 'peq_flat', 15.0, 'Parametric EQ Total Gain +15', 'The MFX\'s parameters aren\'t landing: tell Claude.')
        self.check_setting('peq_flat', 'multi_thru', None, 'Wave Gain -12', 'The zone\'s writes aren\'t landing: tell Claude.')
        thru = by.get('multi_thru')
        if ready(flat, thru):
            fr = [100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0, 6400.0, 12800.0]
            v, name, clipped = self.response(flat, thru, fr)
            v = [x for x in v if x is not None]
            if v:
                self.add('INFO', 'Parametric EQ at its defaults with Wave Gain -12, against Thru at 0, 100 Hz to 12.8 kHz: %+.2f to %+.2f dB.' % (min(v), max(v)))
        self.check_taps('sdelay_stereo', {0: [22040], 1: [28656]}, 'Stereo Delay')      # the address register's settled values; the model's prediction (HARDWARE_FIRMWARE.md, "The chip is an ESP")
        self.check_taps('sdelay_mono', None, 'Stereo Delay in MONO')
        self.check_taps('mtap', {0: [11025, 22050], 1: [11025, 22050]}, 'Multi Tap')
        a, b = by.get('sdelay_stereo'), by.get('sdelay_mono')
        if a is not None and b is not None and a.mark is not None and b.mark is not None and ready(a, b):
            ta, tb = [self.taps(a, 1000, 60000, c)[:1] for c in (0, 1)], [self.taps(b, 1000, 60000, c)[:1] for c in (0, 1)]
            if ta == tb and ta[0] and abs(ta[0][0][1] - tb[0][0][1]) < 0.01:
                self.add('LOOK', 'Mode MONO changed nothing in Stereo Delay\'s first echoes.')
        wet = [('wet_chorus', 'Chorus alone', False), ('wet_reverb', 'Reverb alone', True), ('mfx_to_chorus', 'the MFX\'s send to Chorus', False),
               ('mfx_to_reverb', 'the MFX\'s send to Reverb', True), ('chorus_to_reverb', 'Chorus to Reverb', True)]
        for name, what, tail in wet:
            t = by.get(name)
            if t is None or not ready(t):
                continue
            d = self.delta(name, 'multi_thru')
            floor = getattr(self, 'floor_db', self.env_floor)
            lvl = max(t.sweep) if t.sweep[0] is not None else None
            if t.loudest < floor + 6.0:
                self.add('FAIL', '%s: `%s` has no signal above the floor. The send or the type isn\'t landing: tell Claude.' % (what, name))
                continue
            if t.clean and d is not None and abs(d) < 1.0 and tail:
                self.add('LOOK', '%s: `%s` still has the dry path at %+.2f dB against Thru: MFX Master Level 0 didn\'t take.' % (what, name, d))
                continue
            if tail and t.after_noise < floor + 10.0:
                self.add('LOOK', '%s: `%s` has no tail after the wave\'s noise (%.1f dBFS, floor %.1f).' % (what, name, t.after_noise, floor))
                continue
            self.add('OK', '%s: `%s` at %+.1f dB against the wave%s.' % (what, name, lvl if lvl is not None else 0.0, ', with a tail at %.1f dBFS' % t.after_noise if tail else ''))
        for name, want, what in (('hot', 12.0, 'Output Gain +12'), ('hotter', 24.0, 'Output Gain +12 and Wave Gain +12')):
            t = by.get(name)
            d = self.delta(name, 'main')
            if t is None or d is None:
                continue
            bent = next((STEP_DB[k] for k in range(13) if t.law[k] is not None and t.lvl() is not None and abs(t.law[k] - t.lvl()) > 0.5 and STEP_DB[k] > LOW_STEPS[-1]), None)
            top = max(t.peak)
            where = 'clips %d samples, from %s' % (t.clips, t.clip_where) if t.clips else 'no sample at full scale, top %.2f dBFS' % top
            bend = '; the 1 kHz steps bend from %+d dB' % bent if bent is not None else ''
            if abs(d) < SAME_DB:
                self.add('FAIL', '%s: `%s` has the level of `main`. %s' % (what, name, system_fix))
            else:
                self.add('OK' if abs(d - want) <= 0.5 else 'LOOK', '%s: %+.2f dB against `main`; %s%s.' % (what, d, where, bend))

    def peq_result(self, what, d, want, name, clipped):
        if d is None:
            return
        note = ', read on the %s' % name + (', where %d samples clip' % clipped if clipped else '')
        if abs(d) < 0.5:
            self.add('FAIL', 'Parametric EQ %s: nothing at 800 Hz (%+.2f dB). The MFX\'s type or parameters aren\'t landing: tell Claude.' % (what, d))
        elif abs(d - want) > 0.5:
            self.add('LOOK', 'Parametric EQ %s: %+.2f dB at 800 Hz, not %+.2f%s.' % (what, d, want, note))
        else:
            self.add('OK', 'Parametric EQ %s: %+.2f dB at 800 Hz%s.' % (what, d, note))

    def check_taps(self, name, want, what):
        t = self.by.get(name)
        if t is None or not t.present or t.cut_short or t.mark is None:
            return
        got = {c: self.taps(t, 1000, 60000, c) for c in (0, 1)}
        txt = '; '.join('%s %s' % ('LR'[c], ', '.join('+%d (%.1f dB)' % g for g in got[c]) or 'none') for c in (0, 1))
        if not got[0] and not got[1]:
            self.add('FAIL', '%s: `%s` has no echo. The MFX\'s type isn\'t landing: tell Claude.' % (what, name))
            return
        if want is None:
            self.add('INFO', '%s: %s.' % (what, txt))
            return
        miss = []
        for c, ws in want.items():
            for w in ws:
                if not any(abs(g[0] - w) <= 16 for g in got[c]):
                    miss.append('%s +%d' % ('LR'[c], w))
        if miss:
            self.add('LOOK', '%s: expected a tap at %s; found %s.' % (what, ', '.join(miss), txt))
        else:
            self.add('OK', '%s: %s.' % (what, txt))

    def check_run(self):
        if self.run == 'fx_run_1':
            self.check_run1()
        else:
            self.check_differs()

    def next_step(self):
        return 'run 2' if self.run == 'fx_run_1' else 'the comparison'


def analyse(path, run=None, takes_csv=None, start=None, probe=None, only=None):
    """Everything but writing files: returns the Analysis, with its checks and takes filled in."""
    rec = Recording(path)
    if run is None:
        m = re.search(r'fx_run_[12]', os.path.basename(path))
        if not m and only is None:
            raise Bad('Which run is this? Say --run fx_run_1 or --run fx_run_2.')
        run = m.group(0) if m else ('fx_run_2' if re.match(r'(mfx|rev|cho)\d\d$', only) else 'fx_run_1')
    folder = os.path.join(REPO, 'analysis/test_files/fx')
    return analysis_of(Analysis, rec, run, takes_csv or os.path.join(folder, 'fx_takes.csv'), start, probe, only)


load_probe = Analysis.load_probe                         # the wave in a take list's folder, for fxcut_test.py


def main():
    cli(analyse, 'Checks a recording of an effects run and cuts it into takes.', ['fx_run_1', 'fx_run_2'],
        'analysis/test_files/fx/fx_takes.csv')


if __name__ == '__main__':
    main()
