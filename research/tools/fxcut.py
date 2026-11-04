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
with other --dev or --ch settings cuts the same.  fxcut_test.py checks this on simulated recordings with faults planted."""
import argparse, csv, math, os, re, struct, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxcapture

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..')
WAVE_FS = fxcapture.FS
CUT_LEAD = 0.25                       # seconds kept before a take's note-on
MARK, CLICK_L, CLICK_R = 0.5, 2.0, 4.0
SWEEP, STEPS, NOISE = (6.0, 11.0), (12.5, 13.8), (14.8, 15.8)
STEP_DB = [-72 + 6 * i for i in range(13)]
LOW_STEPS = (-48, -42)                # the 1 kHz steps a take's gain is read on: clear of clipping in `hotter` (+24 dB) even at a path of +9.4 dB
SAME_DB = 0.05                        # below this, two levels are the same level: a setting did nothing


def db(x):
    return 20.0 * math.log10(x) if x > 1e-12 else -240.0


def rms(x):
    return math.sqrt(float(np.mean(np.square(x)))) if len(x) else 0.0


class Bad(Exception):
    """The file can't be read as a recording: what to do is in the message."""


class Recording:
    """A WAV, by memmap: integer PCM of 16, 24 or 32 bits, or float; plain or extensible; any chunks around the data."""
    def __init__(self, path):
        self.path = path
        size = os.path.getsize(path)
        fmt = data = None
        with open(path, 'rb') as f:
            head = f.read(12)
            if head[:4] != b'RIFF' or head[8:12] != b'WAVE':
                raise Bad('%s is not a RIFF WAV file (an AIFF or a 64-bit RF64 needs exporting as WAV first)' % os.path.basename(path))
            pos = 12
            while pos + 8 <= size:
                f.seek(pos)
                cid = f.read(4); n = struct.unpack('<I', f.read(4))[0]
                if cid == b'fmt ':
                    fmt = f.read(n)
                elif cid == b'data':
                    data = (pos + 8, n if n and pos + 8 + n <= size else size - pos - 8)   # a recorder that died leaves 0, or too much
                pos += 8 + n + (n & 1)
        if fmt is None or data is None:
            raise Bad('%s has no fmt or data chunk' % os.path.basename(path))
        self.fmt_chunk = fmt
        tag, self.ch, self.fs, _, self.align, self.bits = struct.unpack('<HHIIHH', fmt[:16])
        if tag == 0xfffe and len(fmt) >= 26:
            tag = struct.unpack('<H', fmt[24:26])[0]
        self.float = tag == 3
        if (tag == 1 and self.bits not in (16, 24, 32)) or (tag == 3 and self.bits not in (32, 64)) or tag not in (1, 3):
            raise Bad('%s: format %d, %d bits; only 16, 24 or 32-bit integer and float can be read' % (os.path.basename(path), tag, self.bits))
        self.offset, self.data_size = data
        self.frames = self.data_size // self.align
        self.bytes = self.bits // 8
        self.mm = np.memmap(path, dtype=np.uint8, mode='r', offset=self.offset, shape=(self.frames * self.align,))
        self.scale = 1.0 if self.float else float(1 << (self.bits - 1))

    def raw(self, a, b):
        """Frames a..b as read, one row per frame, the first two channels; no padding."""
        a, b = max(0, a), min(self.frames, b)
        if b <= a:
            return np.zeros((0, 2), dtype=np.float64 if self.float else np.int32)
        v = np.asarray(self.mm[a * self.align:b * self.align])
        n = b - a
        if self.bits == 24:
            w = v.reshape(n, self.ch, 3).astype(np.int32)
            x = w[..., 0] | (w[..., 1] << 8) | (w[..., 2] << 16)
            x = (x ^ 0x800000) - 0x800000
        else:
            t = {16: '<i2', 32: '<f4' if self.float else '<i4', 64: '<f8'}[self.bits]
            x = np.frombuffer(v.tobytes(), dtype=t).reshape(n, self.ch)
        if self.ch == 1:
            x = np.concatenate([x, np.zeros_like(x)], axis=1)
        return x[:, :2]

    def read(self, a, b):
        """Frames a..b as two channels of float, full scale 1; zeros outside the file."""
        out = np.zeros((max(0, b - a), 2))
        x = self.raw(a, b)
        if len(x):
            out[max(0, -a):max(0, -a) + len(x)] = x / self.scale
        return out

    def is_full_scale(self, x):
        """Samples of x (float, full scale 1) at the file's top or bottom code."""
        lim = 1.0 if self.float else (self.scale - 1.0) / self.scale
        return np.abs(x) >= lim - 1e-12


class Take:
    def __init__(self, row):
        self.name = row['take']
        self.note_on, self.note_off, self.next = float(row['note_on_s']), float(row['note_off_s']), float(row['next_s'])
        self.settings, self.why = row['settings'], row['why']
        self.note = 'play nothing' not in row['settings']
        self.present = False
        self.cut_short = False
        self.mark = None           # the first arrival, a sample of the recording
        self.mark_peak = 0.0
        self.clean = False         # the wave's three clicks are there at their spacing
        self.base = None           # the recording's sample holding the wave's sample 0
        self.late_ms = None        # the mark against the run's timing
        self.sweep = self.noise = (None, None)
        self.law = [None] * 13
        self.peak = (-240.0, -240.0)
        self.floor = self.tail = self.after_noise = -240.0
        self.clips = 0
        self.clip_where = ''
        self.level = -240.0
        self.loudest = -240.0

    def lvl(self):
        """The take's gain at low level, on the 1 kHz steps: nothing clips there, so it says what a setting did."""
        v = [self.law[STEP_DB.index(d)] for d in LOW_STEPS if self.law[STEP_DB.index(d)] is not None]
        return float(np.mean(v)) if v else None


def load_takes(path, run, start, only=None):
    if not os.path.exists(path):
        raise Bad('no take list at %s: run research/tools/fxcapture.py' % path)
    with open(path, newline='') as f:
        rows = [r for r in csv.DictReader(f) if r['run'] == run]
    if not rows:
        raise Bad('no takes for %s in %s' % (run, path))
    takes = [Take(r) for r in rows]
    if only is not None:
        takes = [t for t in takes if t.name == only]
        if not takes:
            raise Bad('no take called %s in %s' % (only, run))
    if start is not None:
        takes = [t for t in takes if t.note_on >= start - 1e-6]
        if not takes:
            raise Bad('no take starts after --from %.3f' % start)
    return takes


def load_probe(folder):
    p = os.path.join(folder, 'fxprobe.wav')
    if os.path.exists(p):
        r = Recording(p)
        return r.read(0, r.frames)
    frames, _ = fxcapture.probe()
    return np.frombuffer(frames, dtype='<i2').reshape(-1, 2) / 32768.0


class Analysis:
    def __init__(self, rec, takes, probe, run):
        self.rec, self.takes, self.pr, self.run = rec, takes, probe, run
        self.fs = rec.fs
        self.by = {t.name: t for t in takes}
        self.checks = []          # (status, text)
        self.offset = None        # the recording's time of the run's time 0
        self.slope_ppm = None
        self.match = 0.0
        self.path = {}

    def at(self, s):
        return int(round(s * self.fs))

    def pat(self, s):
        return int(round(s * WAVE_FS))

    def add(self, status, text):
        self.checks.append((status, text))

    # ---------------------------------------- where the run is in the recording
    def locate(self):
        """Correlates the recording's sounding and silent frames, 10 ms each, with what the take list and the wave say they should be."""
        hop = max(1, int(round(self.fs / 100.0)))
        env = []
        for a in range(0, self.rec.frames, hop * 2000):
            x = np.abs(self.rec.read(a, min(a + hop * 2000, self.rec.frames))).max(axis=1)
            n = len(x) // hop
            if n:
                env.append(x[:n * hop].reshape(n, hop).max(axis=1))
        if not env:
            raise Bad('%s holds no audio' % os.path.basename(self.rec.path))
        env = np.concatenate(env)
        lev = 20.0 * np.log10(np.maximum(env, 1e-7))
        floor = float(np.percentile(lev, 5))
        self.env_floor = floor
        r = np.where(lev > max(floor + 20.0, -90.0), 1.0, -1.0)
        ph = 441
        n = len(self.pr) // ph
        pk = np.abs(self.pr[:n * ph]).max(axis=1).reshape(n, ph).max(axis=1)
        m = np.zeros(n)
        m[pk >= 10 ** (-42 / 20.0)] = 1.0
        m[pk == 0.0] = -1.0
        PRE = 30.0                             # silence is expected before the run: it tells the first take from the second
        first = next((t for t in self.takes if t.note), None)
        if first is None:
            raise Bad('no take with a note')
        t0 = self.takes[0].note_on - PRE
        w = np.zeros(int((self.takes[-1].next - t0) * 100) + 2)
        w[:int((first.note_on - t0) * 100)] = -1.0
        for t in self.takes:
            if t.note:
                i = int((t.note_on - t0) * 100)
                k = min(len(m), int((t.note_off - t.note_on) * 100), len(w) - i)
                w[i:i + k] = m[:k]
        N = 1
        while N < len(r) + len(w):
            N *= 2
        score = np.fft.irfft(np.fft.rfft(r, N) * np.conj(np.fft.rfft(w, N)), N)
        # A lag k puts the mask's frame i on the recording's frame i + k; negative ones wrap to the top.
        order = np.argsort(score)[::-1]
        cands = []
        for kmax in order[:4000]:
            k = int(kmax) - N if kmax >= len(r) else int(kmax)
            if -len(w) < k < len(r) and all(abs(k - c) >= 50 for c in cands):
                cands.append(k)
                if len(cands) == 5:
                    break
        # The best score is nearly always right, but takes full of tails blur the pattern: then the clicks decide among the next best.
        best, best_clean = None, -1
        notes = sum(1 for t in self.takes if t.note)
        for n, k in enumerate(cands):
            self.offset = k * 0.01 - t0
            clean = self.count_clean()
            if clean > best_clean:
                best, best_clean = k, clean
            if n == 0 and clean >= max(3, 0.3 * notes):
                break
        k = best
        self.offset = k * 0.01 - t0
        if best_clean <= 0:                     # nothing fits: maybe the clicks can say where the run is, whichever take they're from
            found = self.find_triple()
            if found and abs(self.at(CLICK_L - MARK) / float(found[1] - found[0]) - 1.0) <= 3e-4:
                for t in [t for t in self.takes if t.note][:8]:
                    self.offset = found[0] / float(self.fs) - (t.note_on + MARK)
                    clean = self.count_clean()
                    if clean > best_clean:
                        best_clean, k = clean, int(round((self.offset + t0) * 100.0))
            self.offset = k * 0.01 - t0
        i0, i1 = max(0, -k), min(len(w), len(r) - k)
        if i1 > i0:
            ww, rr = w[i0:i1], r[i0 + k:i1 + k]
            self.match = float(np.mean(rr[ww > 0] > 0)) if (ww > 0).any() else 0.0     # of what should sound: effects fill the gaps, so those can't count

    # ---------------------------------------- each take's click
    def count_clean(self):
        n = 0
        for t in self.takes:
            t.mark, t.clean = None, False
            if t.note and 0 <= self.at(t.note_on + self.offset) < self.rec.frames:
                self.find_mark(t, self.at(t.note_on + MARK + self.offset))
                n += t.clean
        return n

    def stands_out(self, i, peak):
        """A click near i: as large as the wave says against the first, and clear of what's around it, which a reverb's tail isn't."""
        near = float(np.abs(self.rec.read(i - 16, i + 17)).max())
        around = max(float(np.abs(self.rec.read(i - 400, i - 100)).max()), float(np.abs(self.rec.read(i + 100, i + 400)).max()))
        return near >= 0.2 * peak and near >= 3.0 * around

    def find_mark(self, t, centre):
        half = self.at(0.4)
        a0 = centre - half
        x = self.rec.read(a0, centre + half)
        a = np.abs(x).max(axis=1)
        top = float(a.max()) if len(a) else 0.0
        if top <= 0.0:
            return
        i = int(np.argmax(a >= min(top, max(0.25 * top, 8.0 * float(np.median(a))))))      # the first arrival, as a delay's echo can be the larger, and clear of the noise
        j = i + int(np.argmax(a[i:i + 64]))
        t.mark = a0 + j
        t.mark_peak = float(a[j])
        t.clean = t.mark_peak > 0.0 and self.stands_out(t.mark + self.at(CLICK_L - MARK), t.mark_peak) and self.stands_out(t.mark + self.at(CLICK_R - MARK), t.mark_peak)

    def align(self):
        """Finds every take's mark, then fits the run's timing through the clean ones, for the takes with none."""
        for t in self.takes:
            start = self.at(t.note_on + self.offset)
            end = self.at(t.next + self.offset)
            t.present = start >= 0 and start < self.rec.frames
            t.cut_short = t.present and end > self.rec.frames
            if t.note and t.present:
                self.find_mark(t, self.at(t.note_on + MARK + self.offset))
        pts = [(t.note_on + MARK, t.mark - (t.note_on + MARK) * self.fs) for t in self.takes if t.clean]
        a, b = (self.offset * self.fs, 0.0)
        if pts:
            xs, ys = np.array([p[0] for p in pts]), np.array([p[1] for p in pts])
            a, b = float(np.median(ys)), 0.0
            # The wet takes' first arrivals are late by their effect, so a fit stays on what agrees.
            for _ in range(4):
                good = np.abs(ys - (a + b * xs)) <= 0.015 * self.fs
                if good.sum() >= 3 and np.ptp(xs[good]) > 30.0:
                    b, a = np.polyfit(xs[good], ys[good], 1)
                elif good.sum():
                    a, b = float(np.median(ys[good])), 0.0
            self.slope_ppm = b / self.fs * 1e6 if b else None
        self.fit = (a, b)
        for t in self.takes:
            expect = (t.note_on + MARK) * self.fs + a + b * (t.note_on + MARK)
            if t.mark is not None:
                t.late_ms = (t.mark - expect) / self.fs * 1000.0
            t.base = (t.mark if t.clean else int(round(expect))) - self.at(MARK)
            if t.note and t.mark is not None and not t.clean and t.mark < expect - self.at(0.1):
                t.mark = None                                # it found something else, far ahead of the note

    # ---------------------------------------- levels
    def measure(self):
        ps = {}
        for name, (a, b) in (('sweep', SWEEP), ('noise', NOISE)):
            seg = self.pr[self.pat(a):self.pat(b)]
            ps[name] = (rms(seg[:, 0]), rms(seg[:, 1]))
        for t in self.takes:
            if not t.present:
                continue
            base = t.base
            end = base + self.at(t.next - t.note_on)
            x = self.rec.read(base, end)
            if not len(x):
                continue
            t.peak = (db(float(np.abs(x[:, 0]).max())), db(float(np.abs(x[:, 1]).max())))
            fs_mask = self.rec.is_full_scale(x).any(axis=1)
            t.clips = int(fs_mask.sum())
            if t.clips:
                first = int(np.argmax(fs_mask)) / self.fs
                t.clip_where = next((n for n, s in reversed(fxcapture.SECTIONS) if first >= s), 'lead')
            pre = rms(self.rec.read(base + self.at(0.05), base + self.at(0.45)).ravel())
            t.floor = db(pre)
            tl = self.rec.read(self.at(t.next + self.offset) - self.at(0.3), self.at(t.next + self.offset))
            t.tail = db(rms(tl.ravel()))
            t.after_noise = db(rms(self.rec.read(base + self.at(NOISE[1] + 0.05), base + self.at(NOISE[1] + 0.35)).ravel()))   # the wave is silent from there: a tail, or nothing
            t.loudest = db(rms(x.ravel()))
            if not t.note:
                continue
            if base + self.at(SWEEP[1]) <= self.rec.frames:
                seg = self.rec.read(base + self.at(SWEEP[0]), base + self.at(SWEEP[1]))
                t.sweep = (db(rms(seg[:, 0]) / ps['sweep'][0]), db(rms(seg[:, 1]) / ps['sweep'][1]))
            if base + self.at(NOISE[1]) <= self.rec.frames:
                seg = self.rec.read(base + self.at(NOISE[0]), base + self.at(NOISE[1]))
                t.noise = (db(rms(seg[:, 0]) / ps['noise'][0]), db(rms(seg[:, 1]) / ps['noise'][1]))
            for k in range(13):
                a, b = 12.5 + 0.1 * k + 0.01, 12.5 + 0.1 * k + 0.09
                if base + self.at(b) > self.rec.frames:
                    break
                got = self.amp_1k(self.rec.read(base + self.at(a), base + self.at(b)), self.fs)
                want = self.amp_1k(self.pr[self.pat(a):self.pat(b)], WAVE_FS)
                t.law[k] = db(got / want) if want > 0 else None

    @staticmethod
    def amp_1k(x, fs):
        n = np.arange(len(x)) * 2.0 * math.pi * 1000.0 / fs
        c, s = np.cos(n), np.sin(n)
        v = 0.0
        for ch in (0, 1):
            v += (float(np.dot(x[:, ch], c)) ** 2 + float(np.dot(x[:, ch], s)) ** 2)
        return math.sqrt(v / 2.0) * 2.0 / max(1, len(x))

    # ---------------------------------------- comparing takes in frequency
    def section_of(self, a, b):
        """The test signal to compare two takes on: the sweep unless a take has clipped there, then the noise, whichever clipped least."""
        best = None
        for name, (s0, s1) in (('sweep', SWEEP), ('noise', NOISE)):
            xa = self.rec.read(a.base + self.at(s0), a.base + self.at(s1 + 0.3))
            xb = self.rec.read(b.base + self.at(s0), b.base + self.at(s1 + 0.3))
            n = int(self.rec.is_full_scale(xa).sum() + self.rec.is_full_scale(xb).sum())
            if best is None or n < best[3]:
                best = (name, xa, xb, n)
            if n == 0:
                break
        return best

    def response(self, a, b, freqs, width_oct=1 / 96.0):
        """Take a against take b at each frequency, dB, averaged over both channels and a narrow band."""
        name, xa, xb, clipped = self.section_of(a, b)         # clipped: how many samples, in either
        pa = np.abs(np.fft.rfft(xa, axis=0)) ** 2
        pb = np.abs(np.fft.rfft(xb, axis=0)) ** 2
        df = self.fs / float(len(xa))
        out = []
        for f in freqs:
            lo = max(1, int(f * 2 ** (-width_oct) / df))
            hi = max(lo + 1, int(math.ceil(f * 2 ** width_oct / df)))
            A, B = float(pa[lo:hi].sum()), float(pb[lo:hi].sum())
            out.append(10.0 * math.log10(A / B) if A > 0 and B > 0 else None)
        return out, name, clipped

    # ---------------------------------------- echoes
    def taps(self, t, lo, hi, ch, n=4):
        """Where a channel's largest peaks are after the mark, in samples, earliest first, with their level against the mark."""
        if t.mark is None:
            return []
        x = np.abs(self.rec.read(t.mark + lo, t.mark + hi)[:, ch]).copy()
        if not len(x) or x.max() <= 0.0:
            return []
        top = float(x.max())
        found = []
        for _ in range(n):
            i = int(np.argmax(x))
            if x[i] < 0.1 * top or x[i] < 1e-5:
                break
            found.append((lo + i, db(float(x[i]) / max(t.mark_peak, 1e-12))))
            x[max(0, i - 100):i + 101] = 0.0
        return sorted(found)

    # ---------------------------------------- the checks
    def check_format(self):
        r = self.rec
        kind = '%d-bit float' % r.bits if r.float else '%d-bit PCM' % r.bits
        self.header = '%s: %d Hz, %s, %d channel%s, %.1f s' % (os.path.basename(r.path), r.fs, kind, r.ch, '' if r.ch == 1 else 's', r.frames / float(r.fs))
        if r.fs != WAVE_FS:
            self.add('FAIL', 'Recorded at %d Hz. The chip runs at %d, so nothing can be compared sample by sample. Set the interface (and on the digital path, the XT\'s Digital Output Freq) to 44.1 kHz and record again.' % (r.fs, WAVE_FS))
        if r.ch == 1:
            self.add('FAIL', 'A mono recording: it can\'t show what crosses between the channels. Record stereo.')
        if not r.float and r.bits == 16:
            self.add('LOOK', 'A 16-bit file: a comparison ends at the 16th bit. Record 24-bit if the path allows it.')

    def check_coverage(self):
        expected = [t for t in self.takes if t.note]
        missing = [t for t in self.takes if not t.present]
        short = [t for t in self.takes if t.cut_short]
        if missing or short:
            names = [t.name for t in missing + short]
            self.add('FAIL', 'The recording doesn\'t hold the whole run: %d of %d takes are missing or cut off (%s). It starts %.1f s %s the run\'s start.' % (
                len(names), len(self.takes), ', '.join(names[:6]) + (', …' if len(names) > 6 else ''), abs(self.offset), 'before' if self.offset >= 0 else 'after'))
        if self.match < 0.6:
            self.add('FAIL', 'The recording doesn\'t look like this run: only %.0f%% of what should sound does. Check that it\'s the right file.' % (self.match * 100))
        silent = []
        floor = self.env_floor
        for t in expected:
            if t.present and not t.cut_short and max(t.peak) < max(floor + 20.0, -90.0):
                silent.append(t.name)
        if silent:
            self.add('FAIL', 'No sound in %d take%s: %s. A note that never arrived, or a setting that silences it.' % (len(silent), '' if len(silent) == 1 else 's', ', '.join(silent[:8]) + (', …' if len(silent) > 8 else '')))
        self.silent = set(silent)

    def check_silence(self):
        t = self.by.get('silence')
        if not t or not t.present:
            return
        x = self.rec.read(self.at(t.note_on + self.offset), self.at(t.note_off + self.offset))
        pk = float(np.abs(x).max())
        hop = self.at(0.01)
        n = len(x) // hop
        frames = np.sqrt(np.mean(np.square(x[:n * hop].reshape(n, -1)), axis=1)) if n else np.zeros(1)
        r = float(np.median(frames))                     # what's usually there, whatever else happened once
        self.floor_db = db(r)
        self.path['silence_peak_db'], self.path['silence_rms_db'] = db(pk), db(r)
        lsb = 1.0 / self.rec.scale if not self.rec.float else 2.0 ** -23
        self.path['silence_peak_lsb'] = pk / lsb
        if pk == 0.0:
            self.add('OK', '`silence` is digital silence: every sample zero.')
        elif pk <= 16 * lsb:
            self.add('OK', '`silence` is within %d LSB of zero, %.1f dBFS rms.' % (int(round(pk / lsb)), db(r)))
        elif db(pk) - db(r) > 30.0:
            self.add('LOOK', 'Something sounded during `silence`: peak %.1f dBFS over a floor of %.1f. A stray note, or a switch?' % (db(pk), db(r)))
        else:
            self.add('OK', '`silence` has a noise floor of %.1f dBFS rms, peak %.1f.' % (db(r), db(pk)))

    def reference_take(self):
        for n in ('main', 'main_again', 'multi_off'):
            t = self.by.get(n)
            if t is not None and t.clean:
                return t
        return next((t for t in self.takes if t.clean), None)

    def peak_pos(self, ch, i, half=16):
        """A channel's largest sample near i: its index, and its position to a fraction by a parabola through the three around it."""
        a0 = i - half
        x = np.abs(self.rec.read(a0, i + half + 1)[:, ch])
        j = int(np.argmax(x))
        if 0 < j < len(x) - 1:
            d = x[j - 1] - 2 * x[j] + x[j + 1]
            if d < 0 and x[j - 1] < x[j] > x[j + 1]:
                return a0 + j, a0 + j + 0.5 * (x[j - 1] - x[j + 1]) / d
        return a0 + j, float(a0 + j)

    def spacing(self, t, s):
        """The click s after a take's first, against that first, on the channel carrying it: in whole samples and in fractions."""
        c = int(np.argmax(np.abs(self.rec.read(t.mark + s - 16, t.mark + s + 17)).max(axis=0)))
        i0, p0 = self.peak_pos(c, t.mark)
        i1, p1 = self.peak_pos(c, t.mark + s)
        return i1 - i0, p1 - p0

    def check_unity(self):
        """The wave's clicks at their spacing, or how far off: the first check, since every window below rests on it."""
        if self.reference_take() is None:
            return self.wide_unity()
        dev = []
        exact = True
        s1, s2 = self.at(CLICK_L - MARK), self.at(CLICK_R - MARK)
        for t in self.takes:
            if t.clean:
                for s in (s1, s2):
                    whole, frac = self.spacing(t, s)
                    dev.append(frac / s - 1.0)
                    exact = exact and whole == s
        ppm = float(np.median(dev)) * 1e6
        self.path['spacing_ppm'], self.path['spacing_exact'] = ppm, exact
        if exact:
            self.add('OK', 'The wave plays at unity: on all %d clean takes its clicks are exactly %d and %d samples from the first.' % (sum(t.clean for t in self.takes), s1, s2))
        elif abs(ppm) < 300.0:
            self.add('OK', 'The wave plays at unity, within %+.1f ppm: two clocks, the XT\'s and the recorder\'s, or an analog path.' % ppm)
        else:
            self.add('FAIL', 'The wave runs %+.0f ppm off unity (%+.1f cents). Check Original Fine Tune.' % (ppm, 1200.0 * math.log(1.0 + ppm * 1e-6, 2)))
        return True

    def find_triple(self):
        """The wave's three clicks anywhere in the recording's first minutes, at any speed: one on both sides, one on one side, one on the other.  No other part of
        the wave, and nothing likely in a room, does that.  Returns their samples, or None."""
        n = min(self.rec.frames, self.at(240.0))
        x = np.abs(self.rec.read(0, n))
        a = x.max(axis=1)
        top = float(a.max()) if n else 0.0
        if top <= 0.0:
            return None
        thr = max(0.05 * top, 8.0 * float(np.median(a)))
        gap, i, events = self.at(0.1), 0, []
        idx = np.flatnonzero(a >= thr)
        while i < len(idx):
            j = int(idx[i])
            pk = x[j:j + 33].max(axis=0)
            kind = 'L' if pk[1] < 0.1 * pk[0] else ('R' if pk[0] < 0.1 * pk[1] else 'B')
            events.append((j + int(np.argmax(a[j:j + 33])), kind))
            i = int(np.searchsorted(idx, j + gap))
        for (b, kb), (c, kc), (d, kd) in zip(events, events[1:], events[2:]):
            if kb == 'B' and kc + kd in ('LR', 'RL') and abs((d - c) / float(c - b) - (CLICK_R - CLICK_L) / (CLICK_L - MARK)) < 0.03:
                return b, c, d
        return None

    def wide_unity(self):
        """No take has the clicks where they belong.  If they're anywhere, the wave runs at the wrong speed, and they say at what."""
        found = self.find_triple()
        if found:
            ratio = self.at(CLICK_L - MARK) / float(found[1] - found[0])
            self.path['unity_ratio'] = ratio
            if abs(ratio - 1.0) > 3e-4:
                self.add('FAIL', 'The wave is not playing at unity: it runs at %.5f× (%+.1f cents). In SAMPLE, Info: Original Pitch C4 and Original Fine Tune 0, and note 60 from the run; every window here rests on that.' % (ratio, 1200.0 * math.log(ratio, 2)))
                return False
        self.add('FAIL', 'No clicks in any take where the wave has them. Is this the right recording, with the wave `fxprobe` playing, and the level up?')
        return False

    def check_path(self):
        ref = self.reference_take()
        if ref is None:
            return
        x = np.abs(self.rec.read(ref.mark - 64, ref.mark + 65)).max(axis=1)
        width = int((x >= 0.01 * ref.mark_peak).sum())
        self.path['click_width'] = width
        def at_click(s, c):
            i = ref.mark + self.at(s - MARK)
            return float(np.abs(self.rec.read(i - 16, i + 17)[:, c]).max()), float(np.abs(self.rec.read(i - 1000, i - 100)[:, c]).max())
        (L1, _), (R1, q1) = at_click(CLICK_L, 0), at_click(CLICK_L, 1)
        (L2, q2), (R2, _) = at_click(CLICK_R, 0), at_click(CLICK_R, 1)
        self.path['cross_l_db'], self.path['cross_r_db'] = db(R1 / L1) if L1 else 0.0, db(L2 / R2) if R2 else 0.0
        if self.rec.ch >= 2:
            if R1 > L1 and L2 > R2:
                self.add('LOOK', 'Left and right are swapped: the wave\'s left-only click came out on the right. Fine for levels; a comparison will have to swap them.')
            elif L1 and R2 and ((R1 > 2.0 * q1 and R1 / L1 > 1e-3) or (L2 > 2.0 * q2 and L2 / R2 > 1e-3)):        # above the channel's own noise
                self.add('LOOK', 'The channels leak: a click on one side shows on the other at %.1f and %.1f dB.' % (db(R1 / L1), db(L2 / R2)))
        v = self.rec.read(ref.mark, ref.mark + 1)[0]
        c = int(np.argmax(np.abs(v)))
        self.path['polarity'] = 1 if v[c] > 0 else -1
        if v[c] < 0:
            self.add('LOOK', 'The output is inverted: the wave\'s positive click comes out negative.')
        used = self.bits_used()
        self.path['bits_used'] = used
        exact = self.path.get('spacing_exact')
        if exact and width == 1:
            verdict = 'Sample-exact: one-sample clicks at exact spacing, %d bits in use. It can be compared sample by sample.' % used
            self.add('OK', verdict)
        elif exact:
            self.add('LOOK', 'One clock, but a click is %d samples wide, so something filters it. On the digital outs that\'s expected: the firmware keeps the XT\'s CS8420 converter in them at every rate. Otherwise it\'s the interface\'s own converter, or the chip\'s output stage. Levels, times and responses are fine; a sample comparison has to know that filter first.' % width)
        else:
            self.add('OK', 'Two clocks, or an analog path: a click is %d samples wide. Levels, times and responses only.' % width)
        if not self.rec.float and used < self.rec.bits and used <= 20 and len(self.takes) >= 8:      # one take can sit at a round gain, and a 16-bit wave then shows few bits
            self.add('LOOK', 'Only %d of the file\'s %d bits are in use: the path carried %d bits.' % (used, self.rec.bits, used))

    def bits_used(self):
        """How many of the file's bits any take uses, on each take's noise: one take at a round gain (a 16-bit wave at unity, say) would say too few."""
        nz = 0
        width = 24 if self.rec.float else self.rec.bits
        for t in self.takes:
            if not t.present or not t.note or t.base is None:
                continue
            y = self.rec.raw(t.base + self.at(NOISE[0]), t.base + self.at(NOISE[1]))
            y = y[np.abs(y / self.rec.scale) < 1.0 - 1e-6]               # a clipped sample's code says nothing of the path's width
            if self.rec.float:
                y = y * float(1 << 23)
                if not np.all(y == np.round(y)):
                    return 32
            nz |= int(np.bitwise_or.reduce(np.abs(y.astype(np.int64)))) if len(y) else 0
        return width - ((nz & -nz).bit_length() - 1) if nz else 0

    def check_trim(self):
        ref = self.by.get('main') or self.reference_take()
        if ref is None or ref.sweep[0] is None:
            return
        base = ref.base
        seg = self.rec.read(base + self.at(SWEEP[0]), base + self.at(NOISE[1]))
        clipped = int(self.rec.is_full_scale(seg).any(axis=1).sum())
        if ref.law[0] is not None:
            top = max((k for k in range(13) if ref.law[k] is not None), default=0)
            flat = ref.lvl()
            bent = next((STEP_DB[k] for k in range(13) if ref.law[k] is not None and flat is not None and abs(ref.law[k] - flat) > 0.5 and STEP_DB[k] > LOW_STEPS[-1]), None)
            self.path['law_top'] = STEP_DB[top]
            self.path['clip_step'] = bent
        rms_db = db(rms(seg.ravel()))
        if clipped:
            where = self.rec.is_full_scale(self.rec.read(base + self.at(SWEEP[0]), base + self.at(SWEEP[1]))).any()
            if where or self.rec.is_full_scale(self.rec.read(base + self.at(NOISE[0]), base + self.at(NOISE[1]))).any():
                self.add('FAIL', '`%s` clips in its sweep or noise (%d samples): the level is too high for anything to be measured. Lower the interface\'s trim if there is one; on a digital path, tell Claude.' % (ref.name, clipped))
                return
        if rms_db < -50.0:
            self.add('LOOK', '`%s` is very low, %.1f dBFS rms: low-level results will sit in the noise. Raise the trim if there is one.' % (ref.name, rms_db))

    def check_timing(self):
        late = [t.late_ms for t in self.takes if t.clean and t.late_ms is not None]
        if len(late) >= 2:
            spread = float(np.percentile(late, 95) - np.percentile(late, 5))
            self.path['timing_spread_ms'] = spread
        out = [t for t in self.takes if t.clean and t.late_ms is not None and abs(t.late_ms) > 15.0]
        odd = [t for t in out if t.name in DRY_TAKES]        # a wet take's first arrival is late by its effect
        if odd:
            self.add('LOOK', 'Note%s off the run\'s timing: %s.' % ('' if len(odd) == 1 else 's', ', '.join('`%s` %+.1f ms' % (t.name, t.late_ms) for t in odd)))
        unclean = [t.name for t in self.takes if t.name in DRY_TAKES and t.present and not t.cut_short and not t.clean and t.name not in self.silent]
        if unclean:
            self.add('LOOK', 'No clean click where one belongs: %s. They were placed by timing alone.' % ', '.join(unclean))

    def check_repeat(self):
        a, b = self.by.get('main'), self.by.get('main_again')
        if not a or not b:
            return
        if not a.clean or not b.clean:
            self.add('LOOK', '`main` or `main_again` has no clean click, so the repeat can\'t be read.')
            return
        n0, n1 = self.at(0.4), self.at(16.0)
        x = self.rec.raw(a.base + n0, a.base + n1).astype(np.float64)
        y = self.rec.raw(b.base + n0, b.base + n1).astype(np.float64)
        if x.shape != y.shape or not len(x):
            return
        d = x - y
        nd = int(np.count_nonzero(d.any(axis=1)))
        self.path['repeat_different'] = nd
        same_lat = a.late_ms is not None and b.late_ms is not None
        lat = (b.late_ms - a.late_ms) * self.fs / 1000.0 if same_lat else 0.0
        self.path['repeat_shift_samples'] = lat
        if nd == 0:
            self.add('OK', '`main_again` equals `main` in every sample (the note landed %+.0f samples apart).' % lat)
        else:
            res = db(rms(d.ravel()) / max(rms(x.ravel()), 1e-30))
            mx = float(np.abs(d).max())
            self.path['repeat_residual_db'] = res
            level = abs(a.lvl() - b.lvl()) if a.lvl() is not None and b.lvl() is not None else 0.0
            if level > 0.1:
                self.add('LOOK', '`main_again` is %.2f dB off `main`: the path\'s level moved between takes.' % level)
            else:
                self.add('OK' if not self.path.get('spacing_exact') else 'LOOK', '`main_again` differs from `main` in %d samples, %.1f dB down, by at most %.0f codes (the note landed %+.0f samples apart).' % (nd, res, mx, lat))

    def delta(self, a, b):
        """Take a's low-level gain against take b's, or None."""
        ta, tb = self.by.get(a), self.by.get(b)
        if ta is None or tb is None or ta.lvl() is None or tb.lvl() is None:
            return None
        return ta.lvl() - tb.lvl()

    def check_setting(self, take, ref, want, what, fix):
        """A setting that should move the level by `want` dB: FAIL if it didn't move it, LOOK if it moved it elsewhere."""
        d = self.delta(take, ref)
        if d is None:
            return None
        if abs(d) < SAME_DB:
            self.add('FAIL', '%s: `%s` has the level of `%s`. %s' % (what, take, ref, fix))
        elif want is not None and abs(d - want) > 0.5:
            self.add('LOOK', '%s: %+.2f dB against `%s`, not %+.2f.' % (what, d, ref, want))
        else:
            self.add('OK', '%s: %+.2f dB against `%s`.' % (what, d, ref))
        return d

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

    def check_run2(self):
        same = []
        prev = None
        for t in self.takes:
            if not t.present or t.cut_short or t.sweep[0] is None:
                prev = None
                continue
            if prev is not None and all(abs(a - b) < 0.02 for a, b in zip(t.sweep + t.noise, prev.sweep + prev.noise)):
                same.append('%s = %s' % (t.name, prev.name))
            prev = t
        if same:
            self.add('LOOK', 'Takes with the levels of the one before, so maybe a type that didn\'t change: %s.' % ', '.join(same[:8]))
        else:
            self.add('OK', 'Every take differs from the one before.')

    def run_checks(self):
        self.check_format()
        at_unity = self.check_unity()
        self.check_coverage()
        self.check_silence()
        if at_unity:
            self.check_path()
            self.check_trim()
            self.check_timing()
            self.check_repeat()
            if self.run == 'fx_run_1':
                self.check_run1()
            else:
                self.check_run2()

    # ---------------------------------------- the report
    def table(self):
        ref = self.by.get('main') or self.reference_take()
        rl = ref.lvl() if ref is not None else None
        lines = ['%-17s %9s %8s %-6s %14s %15s %15s %8s %7s %7s  %s' % ('take', 'click, s', 'late ms', 'click', 'peak L/R', 'sweep L/R', 'noise L/R', 'vs ref', 'floor', 'tail', 'clips')]
        def pair(p):
            return '%+7.2f %+7.2f' % p if p[0] is not None else '      -       -'
        for t in self.takes:
            if not t.present:
                lines.append('%-17s not in the recording' % t.name)
                continue
            kind = 'clean' if t.clean else ('timing' if t.note else '-')
            at = '%9.3f' % (t.mark / float(self.fs)) if t.mark is not None else '        -'
            late = '%+8.2f' % t.late_ms if t.late_ms is not None else '       -'
            vs = '%+8.2f' % (t.lvl() - rl) if rl is not None and t.lvl() is not None else '       -'
            clip = '%d %s' % (t.clips, t.clip_where) if t.clips else '0'
            lines.append('%-17s %s %s %-6s %+7.1f %+6.1f %s %s %s %7.1f %7.1f  %s%s' % (t.name, at, late, kind, t.peak[0], t.peak[1], pair(t.sweep), pair(t.noise), vs, t.floor, t.tail, clip, ' (cut off)' if t.cut_short else ''))
        return lines

    def verdict(self):
        fails = [c[1] for c in self.checks if c[0] == 'FAIL']
        looks = [c[1] for c in self.checks if c[0] == 'LOOK']
        nxt = 'run 2' if self.run == 'fx_run_1' else 'the comparison'
        if fails:
            return 'Don\'t go on to %s yet: %d thing%s to fix, marked FAIL.' % (nxt, len(fails), '' if len(fails) == 1 else 's')
        if looks:
            return 'Nothing stops %s, but %d thing%s marked LOOK are worth reading first.' % (nxt, len(looks), '' if len(looks) == 1 else 's')
        return 'This recording is good: go on to %s.' % nxt

    def report(self):
        out = [self.header]
        if self.offset is not None:
            s = 'The run\'s time 0 is %.3f s into the recording; %.0f%% of what should sound does.' % (self.offset, self.match * 100)
            if self.slope_ppm is not None:
                s += ' The recording\'s clock runs %+.1f ppm against the Mac\'s.' % self.slope_ppm
            out.append(s)
        out += [''] + self.table() + ['']
        out.append('vs ref is the gain on the low 1 kHz steps against `main`, which nothing clips.  Sweep and noise are rms against the wave.  dB throughout.')
        ref = self.by.get('main') or self.reference_take()
        if ref is not None and ref.law[0] is not None:
            out.append('Against the wave, 1 kHz steps of `%s`: ' % ref.name + ', '.join('%+d: %+.2f' % (d, ref.law[k]) for k, d in enumerate(STEP_DB) if ref.law[k] is not None))
        out.append('')
        for st, text in self.checks:
            out.append('%-5s %s' % (st, text))
        out += ['', self.verdict()]
        return out

    # ---------------------------------------- the files
    def write(self, folder, cut=True):
        os.makedirs(folder, exist_ok=True)
        with open(os.path.join(folder, 'levels.csv'), 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(['take', 'present', 'cut_short', 'clean', 'click_sample', 'wave0_sample', 'cut_start_sample', 'late_ms', 'peak_l_dbfs', 'peak_r_dbfs', 'sweep_l_db', 'sweep_r_db', 'noise_l_db', 'noise_r_db',
                        'gain_low_db', 'floor_dbfs', 'tail_dbfs', 'after_noise_dbfs', 'clips', 'clip_from'] + ['step%+d_db' % d for d in STEP_DB])
            for t in self.takes:
                def n(v):
                    return '' if v is None else ('%.4f' % v if isinstance(v, float) else v)
                start = t.base - self.at(CUT_LEAD) if t.base is not None else None
                w.writerow([t.name, int(t.present), int(t.cut_short), int(t.clean), n(t.mark), n(t.base), n(start), n(t.late_ms), n(t.peak[0]), n(t.peak[1]), n(t.sweep[0]), n(t.sweep[1]), n(t.noise[0]), n(t.noise[1]),
                            n(t.lvl()), n(t.floor), n(t.tail), n(t.after_noise), t.clips, t.clip_where] + [n(v) for v in t.law])
        with open(os.path.join(folder, 'report.txt'), 'w') as f:
            f.write('\n'.join(self.report()) + '\n')
        if not cut:
            return
        fmt = self.rec.fmt_chunk
        for t in self.takes:
            if not t.present or t.base is None:
                continue
            start = t.base - self.at(CUT_LEAD)
            a = max(0, start)
            b = min(self.rec.frames, t.base + self.at(t.next - t.note_on))
            if b <= a:
                continue
            # A recording that starts at the note, as a resampled take can, has no lead: zeros keep the wave's sample i at sample i + CUT_LEAD.
            body = bytes((a - start) * self.rec.align) + bytes(np.asarray(self.rec.mm[a * self.rec.align:b * self.rec.align]))
            with open(os.path.join(folder, t.name + '.wav'), 'wb') as f:
                size = 4 + 8 + len(fmt) + (len(fmt) & 1) + 8 + len(body)
                f.write(b'RIFF' + struct.pack('<I', size) + b'WAVE')
                f.write(b'fmt ' + struct.pack('<I', len(fmt)) + fmt + (b'\0' if len(fmt) & 1 else b''))
                f.write(b'data' + struct.pack('<I', len(body)) + body)


DRY_TAKES = {'main', 'main_again', 'multi_off', 'multi_thru', 'multi_off_m64', 'multi_thru_m64', 'master_100', 'gain_plus6', 'gain_minus6', 'hot', 'hotter'}


def analyse(path, run=None, takes_csv=None, start=None, probe=None, only=None):
    """Everything but writing files: returns the Analysis, with its checks and takes filled in."""
    rec = Recording(path)
    if run is None:
        m = re.search(r'fx_run_[12]', os.path.basename(path))
        if not m and only is None:
            raise Bad('Which run is this? Say --run fx_run_1 or --run fx_run_2.')
        run = m.group(0) if m else ('fx_run_2' if re.match(r'(mfx|rev|cho)\d\d$', only) else 'fx_run_1')
    folder = os.path.join(REPO, 'analysis/test_files/fx')
    takes = load_takes(takes_csv or os.path.join(folder, 'fx_takes.csv'), run, start, only)
    if probe is None:
        probe = load_probe(os.path.dirname(takes_csv) if takes_csv else folder)
    a = Analysis(rec, takes, probe, run)
    a.locate()
    a.align()
    a.measure()
    a.run_checks()
    return a


def main():
    ap = argparse.ArgumentParser(description='Checks a recording of an effects run and cuts it into takes.')
    ap.add_argument('recording')
    ap.add_argument('--run', choices=['fx_run_1', 'fx_run_2'], help='which run; by default, from the file name')
    ap.add_argument('--from', dest='start', type=float, help="a recording made with fxplay --from: the next_s it started at")
    ap.add_argument('--take', help='a recording of this take alone')
    ap.add_argument('--takes', help='the take list; by default analysis/test_files/fx/fx_takes.csv')
    ap.add_argument('--out', help='where to write; by default a folder beside the recording, named after it')
    ap.add_argument('--no-cut', action='store_true', help='report only')
    a = ap.parse_args()
    if not os.path.exists(a.recording):
        print('FAIL  no file at %s' % a.recording)
        sys.exit(2)
    try:
        an = analyse(a.recording, a.run, a.takes, a.start, None, a.take)
    except Bad as e:
        print('FAIL  %s' % e)
        sys.exit(2)
    for line in an.report():
        print(line)
    folder = a.out or os.path.splitext(os.path.abspath(a.recording))[0]
    an.write(folder, not a.no_cut)
    print('\nWrote %s' % folder)
    sys.exit(1 if any(c[0] == 'FAIL' for c in an.checks) else 0)


if __name__ == '__main__':
    main()
