#!/usr/bin/env python3
"""vc2cut_test.py [names]: vc2cut.py against simulated recordings of the VC-2 runs, with faults planted.  Nothing here
is the XT: vc2sim.py stands in for it, to test that the tool reads a recording right and says so when one is wrong.
It needs the take list from vc2capture.py (analysis/test_files/vc2/), and numpy, scipy and soundfile:
.venv/bin/python3.  Several minutes, as each case is a whole run; files go in build/tools/vc2cut_test/ and are removed.

Each case says which line its recording must give (a FAIL for a fault, a LOOK or INFO for a finding), and a clean
case must give no FAIL and no LOOK, so a check that fires for nothing fails here too; some also say what must be read
back (where each click landed, a law, a side, a pitch)."""
import csv, math, os, shutil, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxcut_test
import vc2cut
import vc2probe
from capsim import Case, judged, run_all, write_wav
from vc2sim import Vc2Sim, build
from vc2takes import MONO_L, MONO_R

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..'))
FOLDER = os.path.join(REPO, 'analysis/test_files/vc2')
TAKES = os.path.join(FOLDER, 'vc2_takes.csv')
WORK = os.path.join(REPO, 'build/tools/vc2cut_test')
VS_WANT = {'main': 'STEREO', 'vs_monolr': 'MONO-L+R', 'vs_monol': 'MONO-L', 'vs_monor': 'MONO-R'}
R1_WANT = {'main': 'MONO-L', 'extin_stereo': 'STEREO', 'extin_monol': 'MONO-L', 'extin_monor': 'MONO-R',
           'extin_monolr': 'MONO-L+R'}


def rows_of(run):
    with open(TAKES, newline='') as f:
        return [r for r in csv.DictReader(f) if r['run'] == run]


def near(bad, what, v, want, tol):
    if v is None or abs(v - want) > tol:
        bad.append('%s: %s, not %.3f' % (what, v, want))


def sides_read(an, want):
    return ['%s reads as %s, not %s' % (n, an.pattern(an.sides(an.by[n])), w) for n, w in want.items()
            if an.pattern(an.sides(an.by[n])) != w]


def read_run1(an, truth):
    """The dry takes' clicks on their planted samples, the Direct Sound law, each take's sides, Auto Note's notes."""
    bad = ['%s: click at %s, not %d' % (n, an.by[n].mark, truth[n]['click']) for n in ('main', 'direct_64')
           if an.by[n].mark != truth[n]['click']]
    near(bad, 'Direct Sound Level 64', an.delta('direct_64', 'main'), 20 * math.log10(64 / 127.0), 0.05)
    bad += sides_read(an, R1_WANT)
    for f, w in zip(an.steps_f0(an.by['autonote']), vc2probe.STEPS_F0):
        rounded = 440.0 * 2 ** (round(12 * math.log2(w / 440.0)) / 12.0)
        if f is None or abs(f / rounded - 1.0) > 0.01:
            bad.append('Auto Note on %.1f Hz read %s, not %.1f' % (w, f, rounded))
    return bad


def read_run2(an, truth):
    bad = []
    near(bad, 'the carrier', an.f0_of(an.by['carrier_analog'], vc2probe.VOWEL_A), 261.63, 1.5)
    c = [an.centroid(an.by[n], vc2probe.VOWEL_A) for n in ('hireso_fshift_0', 'main', 'hireso_fshift_127')]
    if not c[0] < c[1] < c[2]:
        bad.append('Formant Shift\'s centres %s are not in order' % c)
    return bad


def read_run3(an, truth):
    bad = []
    v, _, _ = an.response(an.by['peq1_plus15'], an.by['main'], [800.0])
    near(bad, 'EQ 1 Gain +15 at 800 Hz', v[0], 15.0, 1.0)
    return bad


def cases():
    r2, r3 = dict(run='vc2_run_2'), dict(run='vc2_run_3')
    return [
        Case('r1_clean', Vc2Sim(), clean=True, read=read_run1,
             oks=['Direct Sound Level 64', 'Direct Sound Level 16', 'Master Level 100', 'Output Gain +6',
                  'Direct Sound Level 0: `direct_0` is silent', 'OSC 1 Type PCM: `direct_pcm` is silent']),
        Case('r1_mic_right', Vc2Sim(mic='R'), clean=True, oks=['`main` carries the input\'s right alone']),
        Case('r1_mic_mixed', Vc2Sim(mic='LR'), clean=True, oks=['`main` carries the input\'s left and right']),
        Case('r1_mic_none', Vc2Sim(mic=None), looks=['The direct sound is silent']),
        Case('r1_osc2_flag', Vc2Sim(osc2_flag=True), clean=True, oks=['OSC 2\'s Type sets the flag too']),
        Case('r1_tracks_right', Vc2Sim(tracks='R'), clean=True, oks=['out L 329.6']),
        Case('r1_input_hot', Vc2Sim(input_db=6.0, clip=0.5), clean=True, oks=['bend from']),
        Case('r1_analog', Vc2Sim(noise_db=-85.0, drift_ppm=16.0, fir=19000.0, jitter_ms=3.0, head=12.7),
             oks=['Two clocks']),
        Case('r1_no_input', Vc2Sim(no_input=True), fails=['No clicks in any take']),
        Case('r1_patch_lost', Vc2Sim(lost_patch=True), fails=['No clicks in any take']),
        Case('r1_level_lost', Vc2Sim(lost_level=True), fails=['Direct Sound Level 64: `direct_64` has the level']),
        Case('r1_system_lost', Vc2Sim(lost_system=True), fails=['Master Level 100', 'Output Gain +6']),
        Case('r1_at_48k', Vc2Sim(rate=48000), fails=['Recorded at 48000']),
        Case('r1_mono', Vc2Sim(channels=1), fails=['A mono recording']),
        Case('r1_stopped', Vc2Sim(stop_at=300.0), fails=['doesn\'t hold the whole run']),
        Case('r1_wrong_speed', Vc2Sim(pitch=2 ** (1 / 12.0)), fails=['not playing at unity', '(+100.0 cents)']),
        Case('r1_one_take', Vc2Sim(only='main', head=1.3, latency=500), oks=['The wave plays at unity'],
             read=lambda an, t: [] if an.by['main'].mark == t['main']['click'] else ['main\'s click misplaced']),
        Case('r1_from_part_way', Vc2Sim(from_s=300.0), clean=True),
        Case('r2_clean', Vc2Sim(**r2), clean=True, read=read_run2),
        Case('r2_analog', Vc2Sim(noise_db=-80.0, drift_ppm=-30.0, fir=18000.0, **r2), oks=['Two clocks']),
        Case('r2_vocoder_dead', Vc2Sim(vocoder_dead=True, **r2), fails=['Every vocoder take is silent']),
        Case('r2_carrier_dead', Vc2Sim(carrier_dead=True, **r2), fails=['The carrier is silent']),
        Case('r2_dead_setting', Vc2Sim(dead_setting='hireso_tmpl_5', **r2),
             looks=['`hireso_tmpl_5` changed nothing measurable']),
        Case('r3_clean', Vc2Sim(**r3), clean=True, read=read_run3,
             oks=['the patch EQ is MULTI\'s alone', 'Every take differs']),
        Case('r3_feed_dead', Vc2Sim(feed_dead=True, **r3), fails=['The feed is silent']),
        Case('r3_type_lost', Vc2Sim(lost_type=True, **r3), looks=['Takes with the levels of the one before']),
        Case('r4_clean', Vc2Sim(run='vc2_run_4'), clean=True, oks=['Every take differs']),
        Case('vs_clean', Vc2Sim(run='vc2_vsynth'), clean=True, read=lambda an, t: sides_read(an, VS_WANT),
             oks=['as v204\'s mapping says']),
        Case('vs_monol_wrong', Vc2Sim(run='vc2_vsynth', vs_types={MONO_L: MONO_R}),
             looks=['`vs_monol` carries the input\'s right alone, where v204\'s mapping says']),
    ]


def run_case(xt, c):
    os.makedirs(WORK, exist_ok=True)
    x, truth = build(xt, c.sim, rows_of(c.sim.run))
    path = os.path.join(WORK, c.sim.run + '_' + c.name + '.wav')
    write_wav(path, x, c.sim)
    del x
    an = vc2cut.analyse(path, c.sim.checked_as or c.sim.run, TAKES, c.sim.from_s, xt.pr, c.sim.only)
    bad = judged(an, c, ('OK', 'INFO'))
    if c.read and not c.fails:
        bad += c.read(an, truth)
    os.remove(path)
    return an, bad


def main():
    if not os.path.exists(TAKES):
        sys.exit('no take list: run research/tools/vc2capture.py first')
    xt = fxcut_test.Xt(vc2cut.Analysis.load_probe(FOLDER))
    failed = run_all(cases(), lambda c: run_case(xt, c), sys.argv[1:])
    if os.path.isdir(WORK):
        shutil.rmtree(WORK)
    print('%d wrong' % failed)
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
