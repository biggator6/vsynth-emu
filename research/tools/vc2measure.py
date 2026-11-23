#!/usr/bin/env python3
"""vc2measure.py: what vc2cut.py measures of a VC-2 take beyond the shared levels (capmeasure.py): which input side
reaches each output (the slot question), a fundamental (the tracker, Auto Note, the carrier), the spectrum's band
levels (the vocoder's settings) and where a sound starts (the vocoder's delay).  `Vc2Measure` is a part of
vc2cut.py's analysis."""
import math
import numpy as np

import vc2probe
from caprec import db, rms
from capbase import CLICK_L, CLICK_R

# Which input side each output carries, by the one-sided clicks against the mark (both sides) on each output: the
# four External In Types as v204 reads the returned words (HARDWARE_FIRMWARE.md, "rom.4's outside input").
PATTERNS = {
    'STEREO': ((0.0, None), (None, 0.0)),
    'MONO-L+R': ((-6.0, -6.0), (-6.0, -6.0)),
    'MONO-L': ((0.0, 0.0), (None, None)),
    'MONO-R': ((None, None), (0.0, 0.0)),
    'swapped': ((None, 0.0), (0.0, None)),
    'none': ((None, None), (None, None))}
HEARD_DB = -30.0                      # a click is heard on an output at most this far under the mark's there
LEVEL_DB = 2.0                        # a heard click's level matches a pattern's within this
BANDS = [100.0 * 2 ** (k / 3.0) for k in range(19)]   # third octaves, 100 Hz to 6.4 kHz


class Vc2Measure:
    def section(self, t, span):
        """A take's frames over a span of the input's timeline, both channels."""
        return self.rec.read(t.base + self.at(span[0]), t.base + self.at(span[1]))

    def sides(self, t):
        """[[L click on out L, on out R], [R click on L, R]], each dB against the mark on that output, None unheard."""
        if t.mark is None:
            return None
        out = []
        for s in (CLICK_L, CLICK_R):
            row = []
            for c in (0, 1):
                mark, _ = self.click_at(t, 0.5, c)
                peak, before = self.click_at(t, s, c)
                heard = mark > 0 and peak > 3.0 * before and db(peak / mark) > HEARD_DB
                row.append(db(peak / mark) if heard else None)
            out.append(row)
        return out

    @staticmethod
    def pattern(m):
        """The PATTERNS name a sides matrix matches, or None."""
        def fits(want):
            return all((w is None) == (g is None) and (w is None or abs(w - g) <= LEVEL_DB)
                       for wr, gr in zip(want, m) for w, g in zip(wr, gr))
        return next((name for name, want in PATTERNS.items() if fits(want)), None)

    @staticmethod
    def said(m):
        cell = lambda v: '-' if v is None else '%+.1f' % v
        return 'L click: out L %s, out R %s; R click: out L %s, out R %s dB' % tuple(cell(v) for r in m for v in r)

    def f0(self, x, lo=60.0, hi=1500.0):
        """The fundamental of x (mono) by YIN's normalised difference, in Hz, or None for no clear one."""
        fs = self.fs
        a, b = int(fs / hi), int(fs / lo)
        if len(x) < 2 * b or rms(x) <= 0.0:
            return None
        w = x[:len(x) - b]
        d = np.array([float(np.sum((w - x[k:k + len(w)]) ** 2)) for k in range(1, b + 1)])
        cum = np.cumsum(d) / np.arange(1, b + 1)
        n = d / np.maximum(cum, 1e-30)
        under = np.flatnonzero(n[a - 1:] < 0.2)
        if not len(under):
            return None
        k = a - 1 + int(under[0])
        while k + 1 < len(n) and n[k + 1] < n[k]:
            k += 1
        p = float(k)
        if 0 < k < len(n) - 1:
            den = n[k - 1] - 2 * n[k] + n[k + 1]
            p += 0.5 * (n[k - 1] - n[k + 1]) / den if den > 0 else 0.0
        return fs / (p + 1.0)

    def f0_of(self, t, span, ch=None):
        """A take's fundamental over the middle of a span of the input's timeline, on one output or their mean."""
        x = self.section(t, (span[0] + 0.1 * (span[1] - span[0]), span[1] - 0.1 * (span[1] - span[0])))
        return self.f0(x.mean(axis=1) if ch is None else x[:, ch])

    def steps_f0(self, t):
        """The take's fundamental on each of the input's pitch steps."""
        out = []
        for k in range(len(vc2probe.STEPS_F0)):
            s = vc2probe.STEPS[0] + 0.4 * k
            out.append(self.f0_of(t, (s + 0.05, s + 0.35)))
        return out

    def bands(self, t, span):
        """Third-octave levels, dB, of a take over a span, both outputs together."""
        x = self.section(t, span)
        if not len(x):
            return None
        p = np.abs(np.fft.rfft(x * np.hanning(len(x))[:, None], axis=0)) ** 2
        p = p.sum(axis=1)
        f = np.fft.rfftfreq(len(x), 1.0 / self.fs)
        return np.array([10.0 * math.log10(max(float(p[(f >= c * 2 ** (-1 / 6.0)) & (f < c * 2 ** (1 / 6.0))].sum()),
                                                1e-30)) for c in BANDS])

    def distance(self, a, b, span):
        """How far two takes' spectra are apart over a span: the rms of their band levels' difference, the overall
        level taken out, dB; and the level's own difference."""
        ba, bb = self.bands(a, span), self.bands(b, span)
        if ba is None or bb is None:
            return None, None
        d = ba - bb
        return float(np.sqrt(np.mean((d - d.mean()) ** 2))), float(d.mean())

    def centroid(self, t, span):
        b = self.bands(t, span)
        if b is None:
            return None
        w = 10 ** (b / 10.0)
        return float(np.sum(w * np.array(BANDS)) / np.sum(w))

    def onset(self, t, at):
        """Where a take's sound first rises 20 dB over what was before it, after the input's time `at`, in ms."""
        x = np.abs(self.section(t, (at - 0.3, at + 0.5))).max(axis=1)
        before = float(np.median(x[:self.at(0.25)])) if len(x) else 0.0
        over = np.flatnonzero(x[self.at(0.3):] > max(10.0 * before, 1e-6))
        return 1000.0 * over[0] / self.fs if len(over) else None
