#!/usr/bin/env python3
"""capbase.py: a capture run's takes found in a recording, for the cutters (fxcut.py, vc2cut.py).  The test wave's layout
(its mark, the two one-sided clicks, sweep, 1 kHz steps and noise, fxcapture.probe()'s first 16.8 s), a take from the take
list, and `Locate`: where the run is in the recording and where each take's first click landed (capclick.py's part).
Split from fxcut.py as it stood; a cutter's analysis is `Locate` with capmeasure's, cappath's and capreport's parts.
The wave's layout is caplayout.py's, given on from here as the cutters import it."""
import csv, os
import numpy as np

from caprec import Bad, Recording
from capclick import Clicks
from caplayout import WAVE_FS, CUT_LEAD, MARK, CLICK_L, CLICK_R, SWEEP, STEPS, NOISE, STEP_DB, LOW_STEPS, SAME_DB


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



class Locate(Clicks):
    probe_file = None                 # a cutter's test wave, in its take list's folder; make_probe() makes it again
    clean_sides = 2                   # how many of the wave's one-sided clicks a clean take shows (a mono path, one)

    @classmethod
    def load_probe(cls, folder):
        p = os.path.join(folder, cls.probe_file)
        if os.path.exists(p):
            r = Recording(p)
            return r.read(0, r.frames)
        return cls.make_probe()

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
        w = self.mask(t0, m)
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
                if len(cands) == 30:
                    break
        k = self.choose(cands, t0)
        self.heard = (r, m, t0, k)
        self.match = self.heard_share()

    def mask(self, t0, m, skip=()):
        """What should sound, 10 ms a frame from t0: the wave's pattern m at each take with a note (but those in
        skip), silence before the first."""
        first = next(t for t in self.takes if t.note)
        w = np.zeros(int((self.takes[-1].next - t0) * 100) + 2)
        w[:int((first.note_on - t0) * 100)] = -1.0
        for t in self.takes:
            if t.note and t.name not in skip:
                i = int((t.note_on - t0) * 100)
                k = min(len(m), int((t.note_off - t.note_on) * 100), len(w) - i)
                w[i:i + k] = m[:k]
        return w

    def heard_share(self, skip=()):
        """Of what should sound at the run's place (the takes in skip aside), the share that does."""
        r, m, t0, k = self.heard
        w = self.mask(t0, m, skip)
        i0, i1 = max(0, -k), min(len(w), len(r) - k)
        if i1 > i0:
            ww, rr = w[i0:i1], r[i0 + k:i1 + k]
            return float(np.mean(rr[ww > 0] > 0)) if (ww > 0).any() else 0.0     # of what should sound: effects fill the gaps, so those can't count
        return 0.0

    def choose(self, cands, t0):
        """The run's place among the correlation's best lags (frames of 10 ms), by the clicks each puts in place; it
        sets self.offset, and returns the lag."""
        # The best score is nearly always right, but takes full of tails blur the pattern: then the clicks decide among the next best.
        best, best_clean, best_score = None, -1, -1.0
        notes = sum(1 for t in self.takes if t.note)
        for n, k in enumerate(cands[:5]):
            self.offset = k * 0.01 - t0
            clean = self.count_clean()
            score = clean + 0.5 * self.holds_run()        # takes alike in length fit shifted too: a tie keeps the run in
            if score > best_score:
                best, best_clean, best_score = k, clean, score
            if n == 0 and clean >= max(3, 0.3 * notes):
                break
        k = self.held(best, best_clean, cands[5:], t0)
        if best_clean <= 0:                     # nothing fits: maybe the clicks can say where the run is, whichever take they're from
            found = self.find_triple()
            if found and abs(self.at(CLICK_L - MARK) / float(found[1] - found[0]) - 1.0) <= 3e-4:
                for t in [t for t in self.takes if t.note][:8]:
                    self.offset = found[0] / float(self.fs) - (t.note_on + MARK)
                    clean = self.count_clean()
                    if clean > best_clean:
                        best_clean, k = clean, int(round((self.offset + t0) * 100.0))
            self.offset = k * 0.01 - t0
        return k

    def held(self, k, clean, more, t0):
        """Lag k, unless the run doesn't fit in the recording there and a later lag in `more` fits it with as many
        clean clicks: takes alike in length leave the best lags a whole take or more apart, all equally good."""
        self.offset = k * 0.01 - t0
        if self.holds_run():
            return k
        for other in more:
            self.offset = other * 0.01 - t0
            if self.holds_run() and self.count_clean() >= clean:
                return other
        self.offset = k * 0.01 - t0
        return k

    def holds_run(self):
        """Whether the recording holds every take at the run's place now."""
        first, last = self.takes[0], self.takes[-1]
        return self.at(first.note_on + self.offset) >= 0 and self.at(last.next + self.offset) <= self.rec.frames

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


def analysis_of(cls, rec, run, takes_csv, start=None, probe=None, only=None):
    """A cutter's analysis of a recording, run through: the run's takes from the list (from `start`, or `only` one),
    the test wave (the class's own in the list's folder, unless given), where the run and its takes are, their levels,
    the checks."""
    takes = load_takes(takes_csv, run, start, only)
    if probe is None:
        probe = cls.load_probe(os.path.dirname(takes_csv))
    a = cls(rec, takes, probe, run)
    a.locate()
    a.align()
    a.measure()
    a.run_checks()
    return a
