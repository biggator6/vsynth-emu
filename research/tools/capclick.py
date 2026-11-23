#!/usr/bin/env python3
"""capclick.py: a cutter's take found by its click: where the wave's mark first arrived, whether its one-sided clicks
stand out where they should (a clean take), and the wave's three clicks anywhere in a recording, at any speed.  `Clicks`
is a part of capbase.Locate.  Split from capbase.py as it stood."""
import numpy as np

from caplayout import MARK, CLICK_L, CLICK_R


class Clicks:
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
        sides = sum(self.stands_out(t.mark + self.at(s - MARK), t.mark_peak) for s in (CLICK_L, CLICK_R))
        t.clean = t.mark_peak > 0.0 and sides >= self.clean_sides

    def find_triple(self, start=0, n=None):
        """The wave's three clicks anywhere in the recording's first minutes, at any speed: one on both sides, one on one side, one on the other.  No other part of
        the wave, and nothing likely in a room, does that.  Returns their samples, or None.
        With start and n: in the n frames from start."""
        n = max(0, min(self.rec.frames - start, self.at(240.0) if n is None else n))
        x = np.abs(self.rec.read(start, start + n))
        a = x.max(axis=1)
        top = float(a.max()) if len(a) else 0.0
        if top <= 0.0:
            return None
        thr = max(0.05 * top, 8.0 * float(np.median(a)))
        gap, i, events = self.at(0.1), 0, []
        loud = np.flatnonzero(a >= thr)
        while i < len(loud):
            j = int(loud[i])
            pk = x[j:j + 33].max(axis=0)
            kind = 'L' if pk[1] < 0.1 * pk[0] else ('R' if pk[0] < 0.1 * pk[1] else 'B')
            events.append((j + int(np.argmax(a[j:j + 33])), kind))
            i = int(np.searchsorted(loud, j + gap))
        for (b, kb), (c, kc), (d, kd) in zip(events, events[1:], events[2:]):
            if kb == 'B' and kc + kd in ('LR', 'RL') and abs((d - c) / float(c - b) - (CLICK_R - CLICK_L) / (CLICK_L - MARK)) < 0.03:
                return b + start, c + start, d + start
        return None
