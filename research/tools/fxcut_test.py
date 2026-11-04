#!/usr/bin/env python3
"""fxcut_test.py [names]: fxcut.py against simulated recordings, with faults planted.  Nothing here is the XT: a made-up effect path, with gains, EQ, delays,
a chorus and a reverb of known size, stands in for it, to test that the tool reads a recording right and says so when one is wrong.  It needs the take
list from fxcapture.py, and numpy, scipy and soundfile: .venv/bin/python3.  About 3 minutes; files go in build/tools/fxcut_test/ and are removed.

Each case says what must be read from the recording (the sample each click landed on, levels, echoes), and which line a planted fault must produce.
A clean case must produce no FAIL and no LOOK, so a check that fires for nothing fails here too."""
import csv, json, math, os, shutil, struct, subprocess, sys
import numpy as np
from scipy.signal import oaconvolve
import soundfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxcapture, fxcut

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..'))
FOLDER = os.path.join(REPO, 'analysis/test_files/fx')
WORK = os.path.join(REPO, 'build/tools/fxcut_test')
FS = 44100


def amp(d):
    return 10.0 ** (d / 20.0)


def peaking(n, f0, gain_db, q=1.0):
    """A second-order peak on rfft bins of an n-point transform: exactly gain_db at f0."""
    A = amp(gain_db / 2.0)
    w0 = 2 * math.pi * f0 / FS
    al = math.sin(w0) / (2 * q)
    b = [1 + al * A, -2 * math.cos(w0), 1 - al * A]
    a = [1 + al / A, -2 * math.cos(w0), 1 - al / A]
    z = np.exp(-1j * 2 * math.pi * np.fft.rfftfreq(n))
    return (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)


class Xt:
    """The stand-in: a take's settings in, its output out, index 0 at the note-on and the wave's sample 0."""
    def __init__(self, probe):
        self.pr = probe
        self.cache = {}
        rng = np.random.RandomState(7)
        t = np.arange(int(1.5 * FS)) / FS
        ir = np.exp(-t / 0.2)
        self.irs = [rng.normal(size=len(t)) * ir for _ in range(2)]
        for i in self.irs:
            i[:441] = 0.0
            i *= 0.5 / np.sqrt(np.sum(i * i))

    def chorus(self, x):
        n = np.arange(len(x), dtype=float)
        out = np.zeros((len(x), 2))
        for c, sgn in ((0, 1.0), (1, -1.0)):
            d = 882 + sgn * 220 * np.sin(2 * math.pi * 0.7 * n / FS)
            out[:, c] = np.interp(n - d, n, x, left=0.0, right=0.0)
        return out

    def reverb(self, x):
        return np.stack([oaconvolve(x, i)[:len(x)] for i in self.irs], axis=1)

    def mfx(self, x, t):
        n = len(x)
        k = t['mfx']
        if k == 0:
            return x
        if k == 1:
            mid, tot = t['mfx_over'].get(4, 15) - 15, t['mfx_over'].get(10, 15) - 15
            H = peaking(n, 800.0, mid) * amp(tot)
            return np.fft.irfft(np.fft.rfft(x, axis=0) * H[:, None], n, axis=0)
        if k == 11:
            y = x.copy()
            mono = t['mfx_over'].get(0, 1) == 0
            for c, d in ((0, 22051), (1, 22051 if mono else 28667)):
                y[d:, c] += 0.5 * x[:-d, c]
                y[2 * d:, c] += 0.25 * x[:-2 * d, c]
            return y
        if k == 12:
            y = x.copy()
            for d, g in ((11028, 0.6), (22053, 0.4)):
                y[d:] += g * x[:-d]
            return y
        rng = np.random.RandomState(k)                      # run 2's other types: something different for each, with or without a dry path
        h = rng.normal(size=48) * np.exp(-np.arange(48) / 12.0) * 0.4
        y = np.stack([oaconvolve(x[:, c], h)[:n] for c in (0, 1)], axis=1)
        return y * amp(rng.uniform(-6, 2)) if k % 3 else y + x * 0.7

    def take(self, t, sim, n):
        key = (json.dumps(t, sort_keys=True, default=str), n, sim.pitch, sim.gain_db)
        if key in self.cache:
            return self.cache[key]
        x = np.zeros((n, 2))
        if t['note']:
            if sim.pitch != 1.0:
                i = np.arange(int(len(self.pr) / sim.pitch))
                src = np.stack([np.interp(i * sim.pitch, np.arange(len(self.pr)), self.pr[:, c]) for c in (0, 1)], axis=1)
            else:
                src = self.pr
            m = min(n, len(src))
            x[:m] = src[:m] * amp(t['wave_gain'])
        main = x if t['assign'] == 'MAIN' else np.zeros_like(x)
        multi = x if t['assign'] == 'MULTI' else np.zeros_like(x)
        out = multi.copy()
        if t['mfx_sw'] and t['assign'] == 'MULTI':
            out = self.mfx(multi, t)
            out = np.concatenate([np.zeros((3, 2)), out[:-3]])        # the model's three samples
        y = main + out * (t['mfx_master'] / 127.0) ** 2
        mono = x.mean(axis=1)
        wet = out.mean(axis=1)
        cho = np.zeros((n, 2))
        if t['cho_sw'] and t['cho']:
            cho = self.chorus(mono * t['tva_cho'] / 127.0 + wet * t['mfx_cho'] / 127.0) * amp(-0.7 * t['cho'])
        if t['rev_sw'] and t['rev']:
            rv = self.reverb(mono * t['tva_rev'] / 127.0 + wet * t['mfx_rev'] / 127.0 + cho.mean(axis=1) * t['cho_rev'] / 127.0) * amp(-1.0 * t['rev'])
            y = y + rv * (t['rev_master'] / 127.0) ** 2
        y = y + cho * (t['cho_master'] / 127.0) ** 2
        y = y * (t['master'] / 127.0) ** 2 * amp(t['out_gain']) * amp(sim.gain_db)
        y = np.clip(y, -1.0, 1.0 - 2.0 ** -23)                        # the output stage
        self.cache[key] = y
        return y


class Sim:
    """A recording's circumstances; every field a thing that could happen."""
    def __init__(self, **k):
        self.gain_db = 0.0
        self.head = 7.312            # seconds before the run's time 0
        self.latency = 2659          # samples from a note-on to the wave's start
        self.jitter_ms = 0.0
        self.pitch = 1.0
        self.noise_db = None
        self.drift_ppm = 0.0
        self.fir = None              # a linear-phase low-pass in the path, at this cutoff
        self.swap = self.invert = False
        self.width = 24
        self.format = 'pcm'
        self.chunks = False
        self.path_bits = None        # a path narrower than the file
        self.channels = 2
        self.rate = FS
        self.lost_system = self.lost_patch = self.lost_type = self.lost_mfx_master = False
        self.stop_at = None          # the recording stops this far into the run, seconds
        self.start_at = None         # the recording starts this far into the run
        self.lost_note = None
        self.late = {}               # take: ms
        self.stray_in_silence = False
        self.level_move = None       # take: dB
        self.from_s = None           # fxplay --from
        self.before = False          # something else sounds before the run
        self.other = False           # some other recording altogether
        self.silent = False          # the interface's input wasn't connected
        self.checked_as = None       # the tool is told the wrong run
        self.only = None             # one take alone, recorded by hand or resampled
        self.run = 'fx_run_1'
        self.__dict__.update(k)


def rows_of(run):
    with open(os.path.join(FOLDER, 'fx_takes.csv'), newline='') as f:
        return [r for r in csv.DictReader(f) if r['run'] == run]


def settings(sim, t):
    t = dict(t)
    if sim.lost_system:
        t['master'], t['out_gain'] = 127, 0
    if sim.lost_type:
        t['mfx'], t['mfx_over'] = 0, {}
    if sim.lost_mfx_master:
        t['mfx_master'] = 127
    if sim.lost_patch:
        t.update(assign='MAIN', mfx_sw=0, cho_sw=0, rev_sw=0, mfx=0, mfx_over={}, wave_gain=0)
    return t


def build(xt, sim):
    """The recording as floats, and the truth: each take's wave-sample-0 position, and its click's."""
    rows = rows_of(sim.run)
    takes = {t['name']: t for t in (fxcapture.run1() if sim.run == 'fx_run_1' else fxcapture.run2())}
    if sim.from_s is not None:
        rows = [r for r in rows if float(r['note_on_s']) >= sim.from_s - 1e-6]
    t0 = sim.from_s or 0.0
    if sim.only:
        rows = [r for r in rows if r['take'] == sim.only]
        t0 = float(rows[0]['note_on_s'])
    end = float(rows[-1]['next_s']) - t0
    N = int((sim.head + end + 2.0) * FS)
    out = np.zeros((N, 2))
    rng = np.random.RandomState(11)
    truth = {}
    if sim.before:                                       # the 15-second test recorded first, say: the wave's first seconds, well before the run
        k = int(3.0 * FS)
        out[:k] += xt.pr[int(6.0 * FS):int(6.0 * FS) + k]
    for r in rows:
        name = r['take']
        t = settings(sim, takes[name])
        if name == sim.lost_note:
            t['note'] = False
        n = int(round((float(r['next_s']) - float(r['note_on_s'])) * FS))
        y = xt.take(t, sim, n)
        j = int(round(rng.uniform(-sim.jitter_ms, sim.jitter_ms) * FS / 1000.0))
        j += int(round(sim.late.get(name, 0.0) * FS / 1000.0))
        s = int(round((float(r['note_on_s']) - t0 + sim.head) * FS)) + sim.latency + j
        g = amp(sim.level_move[1]) if sim.level_move and sim.level_move[0] == name else 1.0
        out[s:s + n] += y[:max(0, N - s)] * g
        dry_delay = 3 if t['mfx_sw'] and t['assign'] == 'MULTI' else 0
        truth[name] = dict(base=s, click=s + int(0.5 * FS) + dry_delay)
    if sim.stray_in_silence:
        r = next(r for r in rows if r['take'] == 'silence')
        i = int((float(r['note_on_s']) - t0 + sim.head + 2.0) * FS)
        out[i:i + 2205] += 0.2 * np.sin(2 * math.pi * 440.0 * np.arange(2205) / FS)[:, None]
    if sim.fir:
        k = np.arange(-60, 61)
        h = np.sinc(2 * sim.fir / FS * k) * 2 * sim.fir / FS * np.hanning(121)
        out = np.stack([oaconvolve(out[:, c], h)[60:60 + N] for c in (0, 1)], axis=1)
    if sim.drift_ppm:
        i = np.arange(N) * (1.0 + sim.drift_ppm * 1e-6)
        out = np.stack([np.interp(i, np.arange(N), out[:, c]) for c in (0, 1)], axis=1)
        for v in truth.values():                         # the recorder's clock runs slow against the XT's by that much, so a sample comes sooner
            v['base'] = int(round(v['base'] / (1.0 + sim.drift_ppm * 1e-6))); v['click'] = int(round(v['click'] / (1.0 + sim.drift_ppm * 1e-6)))
    if sim.silent:
        out = out * 0.0
    if sim.other:
        h = np.ones(32) / 32.0
        out = np.stack([oaconvolve(np.random.RandomState(c).normal(size=N), h)[:N] for c in (0, 1)], axis=1) * 0.2
    if sim.rate != FS:
        i = np.arange(int(N * sim.rate / FS)) * FS / sim.rate
        out = np.stack([np.interp(i, np.arange(N), out[:, c]) for c in (0, 1)], axis=1)
    if sim.noise_db is not None:
        out = out + np.random.RandomState(3).normal(size=out.shape) * amp(sim.noise_db)
    if sim.path_bits:
        q = 2.0 ** (sim.path_bits - 1)
        out = np.round(out * q) / q
    if sim.invert:
        out = -out
    if sim.swap:
        out = out[:, ::-1]
    if sim.start_at is not None:
        out = out[int((sim.head + sim.start_at) * sim.rate):]
        for v in truth.values():
            v['base'] -= int((sim.head + sim.start_at) * FS); v['click'] -= int((sim.head + sim.start_at) * FS)
    if sim.stop_at is not None:
        out = out[:int((sim.head + sim.stop_at) * sim.rate)]
    return out, truth


def write_wav(path, x, sim):
    ch = sim.channels
    if ch == 1:
        x = x[:, :1]
    if sim.format == 'float':
        tag, bits = 3, 32
        body = x.astype('<f4').tobytes()
    else:
        tag, bits = 1, sim.width
        q = 2.0 ** (bits - 1)
        v = np.clip(np.round(x * q), -q, q - 1).astype(np.int64)
        if bits == 16:
            body = v.astype('<i2').tobytes()
        elif bits == 32:
            body = v.astype('<i4').tobytes()
        else:
            u = v.astype(np.int32).astype('<i4').tobytes()
            b = np.frombuffer(u, dtype=np.uint8).reshape(-1, 4)[:, :3]
            body = b.tobytes()
    align = ch * bits // 8
    fmt = struct.pack('<HHIIHH', tag, ch, sim.rate, sim.rate * align, align, bits)
    if sim.chunks:                                        # what real DAWs write: extensible, JUNK before, odd-sized LIST after
        guid = struct.pack('<H', tag) + bytes.fromhex('000000001000800000aa00389b71')
        fmt = struct.pack('<HHIIHH', 0xfffe, ch, sim.rate, sim.rate * align, align, bits) + struct.pack('<HHI', 22, bits, 3) + guid
    with open(path, 'wb') as f:
        junk = (b'JUNK' + struct.pack('<I', 28) + bytes(28)) if sim.chunks else b''
        tail = (b'LIST' + struct.pack('<I', 5) + b'INFOx\0') if sim.chunks else b''
        size = 4 + len(junk) + 8 + len(fmt) + 8 + len(body) + len(tail)
        f.write(b'RIFF' + struct.pack('<I', size) + b'WAVE' + junk + b'fmt ' + struct.pack('<I', len(fmt)) + fmt + b'data' + struct.pack('<I', len(body)) + body + tail)


def lines(an, kind):
    return [c[1] for c in an.checks if c[0] == kind]


class Case:
    def __init__(self, name, sim, fails=(), looks=(), oks=(), read=None, clean=False):
        self.name, self.sim, self.fails, self.looks, self.oks, self.read, self.clean = name, sim, fails, looks, oks, read, clean


def clicks_exact(an, truth, slack=0, names=None):
    """Every take meant to be dry has its click on the planted sample."""
    bad = []
    for t in an.takes:
        if names is not None and t.name not in names:
            continue
        if t.name in fxcut.DRY_TAKES and t.present and t.note and t.name in truth:
            if not t.clean or abs(t.mark - truth[t.name]['click']) > slack:
                bad.append('%s: %s against %d' % (t.name, t.mark, truth[t.name]['click']))
    return bad


def read_clean(an, truth):
    """All that a clean digital recording must read back to."""
    bad = clicks_exact(an, truth)
    def near(what, v, want, tol):
        if v is None or abs(v - want) > tol:
            bad.append('%s: %s, not %.3f' % (what, v, want))
    m = an.by['main']
    near('main sweep', m.sweep[0], 0.0, 0.01)
    near('main noise', m.noise[1], 0.0, 0.01)
    near('gain_plus6', an.delta('gain_plus6', 'main'), 6.0, 0.01)
    near('gain_minus6', an.delta('gain_minus6', 'main'), -6.0, 0.01)
    near('master_100', an.delta('master_100', 'main'), 40 * math.log10(100 / 127.0), 0.01)
    near('peq +15', an.response(an.by['peq_mid_plus15'], an.by['peq_flat'], [800.0])[0][0], 15.0, 0.05)
    near('peq -15', an.response(an.by['peq_mid_minus15'], an.by['peq_flat'], [800.0])[0][0], -15.0, 0.05)
    near('peq total', an.delta('peq_total_plus15', 'peq_flat'), 15.0, 0.01)
    near('click width', an.path.get('click_width'), 1, 0)
    near('repeat', an.path.get('repeat_different'), 0, 0)
    t = an.taps(an.by['sdelay_stereo'], 1000, 60000, 0)
    near('Stereo Delay L', t[0][0] if t else None, 22051, 0)
    near('Stereo Delay L level', t[0][1] if t else None, -6.02, 0.02)
    t = an.taps(an.by['sdelay_stereo'], 1000, 60000, 1)
    near('Stereo Delay R', t[0][0] if t else None, 28667, 0)
    t = an.taps(an.by['mtap'], 1000, 60000, 0)
    near('Multi Tap 1', t[0][0] if t else None, 11028, 0)
    near('Multi Tap 2', t[1][0] if len(t) > 1 else None, 22053, 0)
    near('silence', an.path.get('silence_peak_lsb'), 0, 0)
    return bad


def read_no_lead(an, truth):
    """A recording that starts at the note: its cut still has the click where the header says."""
    bad = clicks_exact(an, truth)
    folder = os.path.join(WORK, 'cut')
    an.write(folder, True)
    y, _ = soundfile.read(os.path.join(folder, 'main.wav'), dtype='int32')
    i = an.at(fxcut.CUT_LEAD + fxcut.MARK)
    if int(np.argmax(np.abs(y[:i + 1000]).max(axis=1))) != i:
        bad.append('the click isn\'t at the cut\'s %d' % i)
    shutil.rmtree(folder)
    return bad


def cases():
    return [
        Case('clean', Sim(), oks=['Sample-exact', 'equals `main` in every sample', 'digital silence'], read=read_clean, clean=True),
        Case('gain_9dB', Sim(gain_db=9.4), oks=['Sample-exact'], read=lambda a, t: clicks_exact(a, t)),
        Case('analog', Sim(gain_db=3.0, noise_db=-85.0, drift_ppm=16.0, fir=19000.0, jitter_ms=3.0, head=12.7), oks=['Two clocks', 'within -16', 'noise floor'],
             read=lambda a, t: clicks_exact(a, t, slack=12)),
        Case('analog_noisy', Sim(gain_db=-3.0, noise_db=-68.0, drift_ppm=-40.0, fir=15000.0, jitter_ms=5.0), oks=['Two clocks'], read=lambda a, t: clicks_exact(a, t, slack=30)),
        Case('float32', Sim(format='float'), oks=['Sample-exact', 'equals `main`'], read=read_clean, clean=True),
        Case('wave_chunks', Sim(chunks=True), oks=['Sample-exact'], read=read_clean, clean=True),
        Case('file_16bit', Sim(width=16), looks=['A 16-bit file'], read=lambda a, t: clicks_exact(a, t)),
        Case('file_32bit', Sim(width=32), oks=['Sample-exact'], read=lambda a, t: clicks_exact(a, t), clean=True),
        Case('path_16bit', Sim(path_bits=16), looks=['Only 16 of'], read=lambda a, t: clicks_exact(a, t)),
        Case('filtered', Sim(fir=20000.0), looks=['samples wide'], read=lambda a, t: clicks_exact(a, t)),
        Case('swapped', Sim(swap=True), looks=['swapped']),
        Case('inverted', Sim(invert=True), looks=['inverted']),
        Case('system_lost', Sim(lost_system=True), fails=['Master Level 100', 'Output Gain +6', 'Output Gain -6', 'Output Gain +12']),
        Case('patch_lost', Sim(lost_patch=True), fails=['MFX Master Level 64', 'Mid 1 Gain +15', 'no echo']),
        Case('type_lost', Sim(lost_type=True), fails=['Mid 1 Gain +15', 'Mid 1 Gain -15', 'no echo']),
        Case('master_lost', Sim(lost_mfx_master=True), fails=['MFX Master Level 64'], looks=['dry path']),
        Case('semitone_up', Sim(pitch=2 ** (1 / 12.0)), fails=['not playing at unity', '(+100.0 cents)']),
        Case('five_cents', Sim(pitch=2 ** (5 / 1200.0)), fails=['not playing at unity', '(+5.0 cents)']),
        Case('octave_down', Sim(pitch=0.5), fails=['not playing at unity', '(-1200.0 cents)']),
        Case('at_48k', Sim(rate=48000), fails=['Recorded at 48000']),
        Case('mono', Sim(channels=1), fails=['A mono recording']),
        Case('stopped_early', Sim(stop_at=300.0), fails=['doesn\'t hold the whole run']),
        Case('started_late', Sim(start_at=60.0), fails=['doesn\'t hold the whole run']),
        Case('trim_too_hot', Sim(gain_db=22.0), fails=['clips in its sweep or noise']),
        Case('trim_too_low', Sim(gain_db=-40.0, noise_db=-110.0), looks=['very low']),
        Case('stray_in_silence', Sim(stray_in_silence=True), looks=['Something sounded during `silence`']),
        Case('note_lost', Sim(lost_note='multi_thru'), fails=['No sound in 1 take: multi_thru']),
        Case('note_late', Sim(late={'master_100': 120.0}), looks=['`master_100` +120']),
        Case('note_very_late', Sim(late={'gain_minus6': 600.0}), looks=['No clean click where one belongs: gain_minus6']),
        Case('level_moved', Sim(level_move=('main_again', -0.5)), looks=['the path\'s level moved']),
        Case('something_before', Sim(before=True, head=20.0), read=lambda a, t: clicks_exact(a, t), clean=True),
        Case('from_part_way', Sim(from_s=183.92), read=lambda a, t: clicks_exact(a, t), clean=True),
        Case('one_take', Sim(only='main', width=16, head=1.3, latency=500), looks=['A 16-bit file'], oks=['The wave plays at unity'], read=lambda a, t: clicks_exact(a, t)),
        Case('one_wet_take', Sim(only='sdelay_stereo', head=0.2, latency=500), oks=['Stereo Delay: L +22051'], read=lambda a, t: [] if abs(a.by['sdelay_stereo'].mark - t['sdelay_stereo']['click']) == 0 else ['no click']),
        Case('one_take_no_lead', Sim(only='main', head=0.0, latency=100), read=read_no_lead, clean=True),
        Case('other_audio', Sim(other=True), fails=['No clicks in any take']),
        Case('silent_file', Sim(silent=True), fails=['No clicks in any take']),
        Case('wrong_run', Sim(checked_as='fx_run_2'), fails=['doesn\'t hold the whole run']),
        Case('run_2', Sim(run='fx_run_2', format='pcm'), oks=['Every take differs']),
    ]


def run_case(xt, c):
    os.makedirs(WORK, exist_ok=True)
    x, truth = build(xt, c.sim)
    name = c.sim.run + '_' + c.name + '.wav'
    path = os.path.join(WORK, name)
    write_wav(path, x, c.sim)
    del x
    an = fxcut.analyse(path, c.sim.checked_as or c.sim.run, os.path.join(FOLDER, 'fx_takes.csv'), c.sim.from_s, xt.pr, c.sim.only)
    fails, looks, oks = lines(an, 'FAIL'), lines(an, 'LOOK'), lines(an, 'OK')
    bad = []
    for want, have, kind in ((c.fails, fails, 'FAIL'), (c.looks, looks, 'LOOK'), (c.oks, oks, 'OK')):
        for w in want:
            if not any(w in h for h in have):
                bad.append('no %s saying "%s"' % (kind, w))
    if c.clean and (fails or looks):
        bad.append('should be clean: ' + ' | '.join(fails + looks))
    if not c.fails and fails:
        bad.append('FAIL for nothing: ' + ' | '.join(fails))
    if not c.fails and not c.looks and looks and not c.clean:
        bad.append('LOOK for nothing: ' + ' | '.join(looks))
    if c.read:
        bad += c.read(an, truth)
    if c.name == 'clean':
        bad += check_files(an, path)
    if c.name in ('clean', 'system_lost'):
        bad += check_cli(path, 1 if c.fails else 0)
    os.remove(path)
    return an, bad


def check_files(an, path):
    """The files: each cut take is the recording's own bytes, with its click where the header says; another reader agrees."""
    bad = []
    folder = os.path.join(WORK, 'cut')
    an.write(folder, True)
    full, fs = soundfile.read(path, dtype='int32')
    for t in an.takes:
        p = os.path.join(folder, t.name + '.wav')
        y, fs2 = soundfile.read(p, dtype='int32')
        start = t.base - an.at(fxcut.CUT_LEAD)
        want = full[start:start + len(y)]
        if fs2 != fs or len(want) != len(y) or not np.array_equal(want, y):
            bad.append('%s isn\'t the recording\'s own samples' % t.name)
        if t.clean and t.name == 'main':
            i = an.at(fxcut.CUT_LEAD + fxcut.MARK)
            if int(np.argmax(np.abs(y[:i + 1000]).max(axis=1))) != i or np.count_nonzero(y[i - 1000:i + 1000].any(axis=1)) != 1:
                bad.append('%s: its click isn\'t alone at the cut\'s %d' % (t.name, i))
    with open(os.path.join(folder, 'levels.csv'), newline='') as f:
        rows = list(csv.DictReader(f))
    if len(rows) != len(an.takes):
        bad.append('levels.csv has %d rows' % len(rows))
    shutil.rmtree(folder)
    return bad


def check_cli(path, code):
    """The command line: the run from the file's name, the exit code, the files."""
    tool = os.path.join(REPO, 'research/tools/fxcut.py')
    folder = os.path.join(WORK, 'cli')
    r = subprocess.run([sys.executable, tool, path, '--takes', os.path.join(FOLDER, 'fx_takes.csv'), '--out', folder, '--no-cut'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    bad = []
    if r.returncode != code:
        bad.append('the command line exited %d, not %d: %s' % (r.returncode, code, r.stdout[-300:]))
    if not os.path.exists(os.path.join(folder, 'report.txt')) or not os.path.exists(os.path.join(folder, 'levels.csv')):
        bad.append('the command line wrote no report')
    r = subprocess.run([sys.executable, tool, os.path.join(WORK, 'no_run_in_this_name.wav')], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    if r.returncode != 2 or 'Traceback' in r.stdout:
        bad.append('a missing file: exit %d, %s' % (r.returncode, r.stdout[-200:]))
    shutil.rmtree(folder, ignore_errors=True)
    return bad


def check_reader():
    """fxcut's WAV reader against libsndfile, on each format, and on a real capture if the main checkout has one."""
    bad = []
    os.makedirs(WORK, exist_ok=True)
    rng = np.random.RandomState(5)
    x = np.clip(rng.normal(size=(5000, 2)) * 0.3, -1, 1)
    for sim in (Sim(width=16), Sim(width=24), Sim(width=32), Sim(format='float'), Sim(width=24, chunks=True), Sim(format='float', chunks=True), Sim(channels=1)):
        p = os.path.join(WORK, 'reader.wav')
        write_wav(p, x, sim)
        r = fxcut.Recording(p)
        got = r.read(100, 4900)
        ref, fs = soundfile.read(p, dtype='float64', always_2d=True)
        ref = ref[100:4900]
        if sim.channels == 1:
            ref = np.concatenate([ref, np.zeros_like(ref)], axis=1)
        if fs != r.fs or np.abs(got - ref).max() > 1e-7:
            bad.append('reader differs at %d bits, format %s' % (r.bits, sim.format))
        os.remove(p)
    real = '/Users/mborrow/Documents/vsynth-emu/analysis/test_files/passthrough/vocal_aah_passthrough.wav'
    if os.path.exists(real):
        r = fxcut.Recording(real)
        ref, fs = soundfile.read(real, dtype='float64', always_2d=True)
        got = r.read(0, min(r.frames, 200000))
        if fs != r.fs or np.abs(got - ref[:200000, :2]).max() > 1e-6:
            bad.append('reader differs on a real capture')
    return bad


def main():
    want = sys.argv[1:]
    if not os.path.exists(os.path.join(FOLDER, 'fx_takes.csv')):
        sys.exit('no take list: run research/tools/fxcapture.py first')
    xt = Xt(fxcut.load_probe(FOLDER))
    failed = 0
    if not want or 'reader' in want:
        bad = check_reader()
        print('%-20s %s' % ('reader', 'ok, against libsndfile' if not bad else 'WRONG: ' + '; '.join(bad)))
        failed += bool(bad)
    for c in cases():
        if want and c.name not in want:
            continue
        an, bad = run_case(xt, c)
        said = lines(an, 'FAIL')[:1] or lines(an, 'LOOK')[:1] or [an.verdict()]
        print('%-20s %s' % (c.name, ('ok    ' + said[0][:110]) if not bad else 'WRONG: ' + '; '.join(b[:300] for b in bad)))
        sys.stdout.flush()
        failed += bool(bad)
    if os.path.isdir(WORK):
        shutil.rmtree(WORK)
    print('%d wrong' % failed)
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
