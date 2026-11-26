#!/usr/bin/env python3
"""vc2sim.py: a made-up VC-2 for vc2cut_test.py, so the cutter can be tested on recordings of known content with
faults planted.  Nothing here is the XT: the input word, the direct sound, the EXT IN tone, a four- or eight-band
vocoder, a carrier, and the effects' stand-ins from fxcut_test.py, each answering the takes' settings as rom.4's
findings say it should (VC2_VOCAL_DESIGNER.md, "The part mixer, run"), and each fault a field of `Vc2Sim`."""
import math
import numpy as np

import vc2probe
import vc2takes
import vc2vsynth
from capsim import FS, Sim, amp, path, peaking
from vc2takes import EXTIN, HIRESO, MONO_L, MONO_LR, MONO_R, STEREO, VOCODER

DIRECT = 0.5                          # the direct sound at 127 against the input word
PATTERN = {STEREO: (0, 1), MONO_L: (0, 0), MONO_R: (1, 1), MONO_LR: (2, 2)}      # each output's input: L, R, the mix


class Vc2Sim(Sim):
    """A recording's circumstances: fxcut_test's, and the VC-2's own."""
    def __init__(self, **k):
        mine = dict(run='vc2_run_1', mic='L', input_db=-6.0, clip=1.0, no_input=False, lost_patch=False,
                    lost_level=False, osc2_flag=False, vs_types=None, tracks='L', vocoder_dead=False,
                    carrier_dead=False, dead_setting=None, feed_dead=False, lost_type=False, extin_delay=64,
                    extin_fixed=False, extin_found=True)   # an XT with no External In Type page; what the user said
        super().__init__(**{**mine, **k})


def settings_of(sim, run, name):
    """A take's whole settings, as the XT would hold them: the base, the take's, and the faults."""
    takes = vc2vsynth.takes() if run == 'vc2_vsynth' else vc2takes.RUNS[run]()
    t = next(x for x in takes if x['name'] == name)
    s = {**vc2takes.BASE, **t['set']}
    if sim.lost_patch:
        s = dict(vc2takes.BASE)
    if sim.lost_level:
        s['Direct Sound Level'] = 127 if 'Direct Sound Level' in t['set'] else s['Direct Sound Level']
    if sim.lost_system:
        s['Master Level'], s['Output Gain'] = 127, 16
    if sim.lost_type:
        s['MFX Type'] = 0
    if sim.extin_fixed and run != 'vc2_vsynth':
        s['External In Type'] = vc2takes.BASE['External In Type']
    if name == sim.dead_setting:
        s = {**vc2takes.BASE, **next(x for x in takes if x['name'] == 'main')['set']}
    return t, s


def band(x, lo, hi):
    """x's band lo..hi Hz, by its transform."""
    f = np.fft.rfftfreq(len(x), 1.0 / FS)
    return np.fft.irfft(np.fft.rfft(x) * ((f >= lo) & (f < hi)), len(x))


def envelope(x, block):
    n = len(x) // block * block
    e = np.sqrt(np.mean(np.square(x[:n]).reshape(-1, block), axis=1))
    return np.concatenate([np.repeat(e, block), np.full(len(x) - n, e[-1] if len(e) else 0.0)])


def carrier(key, kind, n, with_input):
    f = 440.0 * 2 ** ((key - 69) / 12.0)
    ph = (np.arange(n) * f / FS) % 1.0
    saw = (2.0 * ph - 1.0) * 0.1
    if kind != VOCODER:
        return saw
    noise = np.random.RandomState(9).uniform(-1, 1, n) * 0.03
    return (saw * 0.7 + noise) * (1.2 if with_input else 1.0)          # its Dynamics follow the input


def vocode(mic, car, s, name):
    """A channel vocoder: the carrier's bands at the mic's envelopes, the settings each moving something."""
    bands = 8 if s['Vocoder Type'] == HIRESO else 4
    edges = 150.0 * (5000.0 / 150.0) ** (np.arange(bands + 1) / float(bands))
    shift = 2 ** ((s['Formant Shift'] - 64) / 63.0)
    block = int(441 * (1 + (s['Env Attack'] + s['Env Release']) / 40.0))
    out = np.zeros((len(mic), 2))
    for k in range(bands):
        e = envelope(band(mic, edges[k], edges[k + 1]), block) * (1.0 + 0.05 * s['Formant Template'] * k / bands)
        c = band(car, edges[k] * shift, edges[k + 1] * shift) * e * 20.0
        out[:, k % 2 if s['Stereo Process Sw'] else 0] += c
    if not s['Stereo Process Sw']:
        out[:, 1] = out[:, 0]
    depth = s['Vocoder Depth'] / 127.0
    out = out * depth + car[:, None] * (1.0 - depth)
    unvoiced = band(mic, 4000.0, 12000.0) * s['Unvoice Level'] / 127.0 * s['Unvoice Detect'] / 25.0 * 0.5
    spread = 1.5 if s['Unvoice Spread switch'] else 1.0
    out += np.stack([unvoiced * spread, -unvoiced * spread if spread > 1.0 else unvoiced], axis=1)
    smoothing = 1.0 - 0.002 * (s['Env Attack'] + s['Env Release'])          # slower envelopes, a little less
    return out * s['Vocoder Sound Level'] / 127.0 * smoothing


def tracked(x, sim, n):
    """Auto Note's tone: a sine at the tracked pitch, rounded to the semitone, over the input's voiced parts."""
    out = np.zeros(n)
    near = lambda f: 440.0 * 2 ** (round(12 * math.log2(f / 440.0)) / 12.0)
    parts = [(vc2probe.SIDES, 220.0 if sim.tracks == 'L' else 330.0), (vc2probe.VOWEL_A, 220.0),
             (vc2probe.VOWEL_I, 220.0)]
    parts += [((vc2probe.STEPS[0] + 0.4 * k, vc2probe.STEPS[0] + 0.4 * k + 0.4), f)
              for k, f in enumerate(vc2probe.STEPS_F0)]
    for (a, b), f in parts:
        i, j = min(n, vc2probe.at(a)), min(n, vc2probe.at(b))
        out[i:j] = 0.1 * np.sin(2 * math.pi * near(f) * np.arange(j - i) / FS)
    return out


def delayed(x, d):
    return np.concatenate([np.zeros((d,) + x.shape[1:]), x[:len(x) - d]])


def the_input(sim, probe, t, n):
    """What the XT's input gets during a take: the probe's part from the take's start, at the input's gain, clipped."""
    x = np.zeros((n, 2))
    if t['probe'] and not sim.no_input:
        src = probe[:vc2probe.at(vc2probe.END if t['probe'] == 'full' else vc2probe.FX_END)]
        if sim.pitch != 1.0:
            i = np.arange(int(len(src) / sim.pitch)) * sim.pitch
            src = np.stack([np.interp(i, np.arange(len(src)), src[:, c]) for c in (0, 1)], axis=1)
        m = min(n, len(src))
        x[:m] = np.clip(src[:m] * amp(sim.input_db), -sim.clip, sim.clip)
    return x


def effects(xt, multi, s, n):
    """MULTI through the patch EQ and the MFX, the MFX's sends to the Chorus and Reverb, the system EQ: the mix."""
    for gain_id, f0 in (('EQ 1 Gain', 800.0), ('EQ 2 Gain', 2500.0)):
        if s[gain_id] != 15:
            h = peaking(n, f0, s[gain_id] - 15)
            multi = np.fft.irfft(np.fft.rfft(multi, axis=0) * h[:, None], n, axis=0)
    out = multi
    if s['MFX Switch']:
        m = delayed(xt.mfx(multi, {'mfx': s['MFX Type'], 'mfx_over': {}}), 3)
        out = m * (s['MFX Master Level'] / 127.0) ** 2
        if s['Chorus Switch'] and s['Chorus Type']:
            out = out + xt.chorus(m.mean(axis=1) * s['MFX Chorus Send Level'] / 127.0) * amp(-0.7 * s['Chorus Type'])
        if s['Reverb Switch'] and s['Reverb Type']:
            out = out + xt.reverb(m.mean(axis=1) * s['MFX Reverb Send Level'] / 127.0) * amp(-1.0 * s['Reverb Type'])
    return out


def output(xt, sim, run, name, n):
    """A take's output, index 0 at its note-on and the input's sample 0; and where its mark is, if it has one."""
    t, s = settings_of(sim, run, name)
    x = the_input(sim, xt.pr, t, n)
    mono = {'L': x[:, 0], 'R': x[:, 1], 'LR': x.mean(axis=1), None: np.zeros(n)}[sim.mic]
    if run == 'vc2_vsynth':
        types = sim.vs_types or {}
        pick = PATTERN[types.get(s['External In Type'], s['External In Type'])]
        cols = [x[:, 0], x[:, 1], x.mean(axis=1)]
        y = delayed(np.stack([cols[pick[0]], cols[pick[1]]], axis=1) * 0.5, sim.extin_delay)
        return y if t['key'] is not None else y * 0.0
    flag = s['OSC 1 Type'] == EXTIN or (sim.osc2_flag and s['OSC 2 Type'] == EXTIN)
    level = np.full(n, float(s['Direct Sound Level']))
    if t.get('marker') and not sim.lost_patch:
        level[:vc2probe.at(vc2takes.MARKER_S)] = 127.0
    direct = mono * level / 127.0 * DIRECT if flag else np.zeros(n)
    main = np.stack([direct, direct], axis=1)
    if s['Reverb Switch'] and s['Reverb Type'] and s['Reverb Send Level']:
        main = main + xt.reverb(direct * s['Reverb Send Level'] / 127.0) * amp(-1.0 * s['Reverb Type'])
    multi = np.zeros((n, 2))
    key = t['key']
    if key is not None and s['OSC 1 Switch'] and s['OSC 1 Type'] == EXTIN and not sim.feed_dead:
        pick = PATTERN[s['External In Type']]
        cols = [x[:, 0], x[:, 1], x.mean(axis=1)]
        tone = np.stack([cols[pick[0]], cols[pick[1]]], axis=1) * 0.5
        if s['Auto Note On Switch']:
            tone = np.stack([tracked(x, sim, n)] * 2, axis=1)
        elif key != 60:
            i = np.arange(n) * 2 ** ((key - 60) / 12.0)
            tone = np.stack([np.interp(i, np.arange(n), tone[:, c], right=0.0) for c in (0, 1)], axis=1)
        multi = multi + delayed(tone, sim.extin_delay)
    if key is not None and s['OSC 2 Switch'] and not sim.carrier_dead:
        car = carrier(key, s['OSC 2 Type'], n, t['probe'] is not None)
        if s['Vocoder Sw']:
            multi = multi + (np.zeros((n, 2)) if sim.vocoder_dead else vocode(mono, car, s, name))
        else:
            multi = multi + np.stack([car, car], axis=1)
        if t.get('marker'):                                  # a marked take's key goes down after the marker
            multi[:vc2probe.at(vc2takes.MARKER_S)] = 0.0
    y = main + effects(xt, multi, s, n)                 # the system EQ at rom.4's defaults is flat, on or off
    return y * (s['Master Level'] / 127.0) ** 2 * amp(s['Output Gain'] - 16)


def build(xt, sim, rows):
    """The recording as floats, from the take list's rows for the run, and each dry take's wave-sample-0 position."""
    t0 = sim.from_s or 0.0
    rows = [r for r in rows if float(r['note_on_s']) >= t0 - 1e-6]
    if sim.only:                                         # one take alone, recorded by hand
        rows = [r for r in rows if r['take'] == sim.only]
        t0 = float(rows[0]['note_on_s'])
    end = float(rows[-1]['next_s']) - t0
    N = int((sim.head + end + 2.0) * FS)
    out = np.zeros((N, 2), dtype=np.float32)
    rng = np.random.RandomState(11)
    truth = {}
    for r in rows:
        n = int(round((float(r['next_s']) - float(r['note_on_s'])) * FS))
        y = output(xt, sim, sim.run, r['take'], n)
        j = int(round(rng.uniform(-sim.jitter_ms, sim.jitter_ms) * FS / 1000.0))
        s = int(round((float(r['note_on_s']) - t0 + sim.head) * FS)) + sim.latency + j
        out[s:s + n] += y[:max(0, N - s)].astype(np.float32)
        truth[r['take']] = dict(base=s, click=s + int(0.5 * FS))
    return path(out, truth, sim, N)
