#!/usr/bin/env python3
"""capmeasure.py: a cutter's measures of each take against the test wave: levels, the 1 kHz law, responses, echoes, a click's
place to a fraction and the bits in use.  `Measure` is a part of a cutter's analysis (capbase.Locate's fields).  Split from
fxcut.py as it stood."""
import math
import numpy as np

from caprec import db, rms
from capbase import WAVE_FS, SWEEP, NOISE


class Measure:
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
                t.clip_where = next((n for n, s in reversed(self.sections) if first >= s), 'lead')
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

    def delta(self, a, b):
        """Take a's low-level gain against take b's, or None."""
        ta, tb = self.by.get(a), self.by.get(b)
        if ta is None or tb is None or ta.lvl() is None or tb.lvl() is None:
            return None
        return ta.lvl() - tb.lvl()
