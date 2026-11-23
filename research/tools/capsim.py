#!/usr/bin/env python3
"""capsim.py: what the cutters' tests share (fxcut_test.py, vc2cut_test.py): a recording's circumstances (`Sim`, each
field a thing that could happen), the path a simulated recording takes on its way to the file (`path`: a filter, two
clocks, noise, the rate, a narrower path, a swap, a stop), the file written in each format a recorder writes
(`write_wav`), a gain and a peaking filter, and a test case with the lines its analysis said.  Split from
fxcut_test.py as it stood."""
import math, struct, sys
import numpy as np
from scipy.signal import oaconvolve

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


def path(out, truth, sim, N):
    """The recording's way from the instrument to the file: what each circumstance does to it, and to the truth."""
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


def judged(an, c, ok_kinds=('OK',)):
    """What's wrong with a case's analysis: a line it wants and didn't get (an OK among ok_kinds' lines), a clean
    case's FAIL or LOOK, or either for nothing."""
    fails, looks = lines(an, 'FAIL'), lines(an, 'LOOK')
    oks = [line for kind in ok_kinds for line in lines(an, kind)]
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
    return bad


def run_all(cases, run_case, want):
    """Each case, or those named in want, a line each; returns how many were wrong."""
    failed = 0
    for c in cases:
        if want and c.name not in want:
            continue
        an, bad = run_case(c)
        said = lines(an, 'FAIL')[:1] or lines(an, 'LOOK')[:1] or [an.verdict()]
        print('%-20s %s' % (c.name, ('ok    ' + said[0][:110]) if not bad else 'WRONG: ' + '; '.join(b[:300] for b in bad)))
        sys.stdout.flush()
        failed += bool(bad)
    return failed
