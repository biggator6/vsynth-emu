#!/usr/bin/env python3
"""cappath.py: a cutter's checks of the recording and its path, alike for every run: the file, the wave at unity, the takes all
there and sounding, the floor, the click's width, polarity and sides, the bits, the trim, the timing, a take repeated, and a
setting's level.  `PathChecks` is a part of a cutter's analysis.  Split from fxcut.py as it stood."""
import math, os
import numpy as np

from caprec import db, rms
from capbase import WAVE_FS, MARK, CLICK_L, CLICK_R, SWEEP, NOISE, STEP_DB, LOW_STEPS, SAME_DB


class PathChecks:
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
        for n in self.reference_names:
            t = self.by.get(n)
            if t is not None and t.clean:
                return t
        return next((t for t in self.takes if t.clean), None)

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
            cents = 1200.0 * math.log(1.0 + ppm * 1e-6, 2)
            self.add('FAIL', 'The wave runs %+.0f ppm off unity (%+.1f cents). %s' % (ppm, cents, self.fine_fix))
        return True

    def wide_unity(self):
        """No take has the clicks where they belong.  If they're anywhere, the wave runs at the wrong speed, and they say at what."""
        found = self.find_triple()
        if found:
            ratio = self.at(CLICK_L - MARK) / float(found[1] - found[0])
            self.path['unity_ratio'] = ratio
            if abs(ratio - 1.0) > 3e-4:
                cents = 1200.0 * math.log(ratio, 2)
                said = 'The wave is not playing at unity: it runs at %.5f× (%+.1f cents). ' % (ratio, cents)
                self.add('FAIL', said + self.unity_fix)
                return False
        self.add('FAIL', 'No clicks in any take where the wave has them. ' + self.no_clicks)
        return False

    def check_path(self):
        ref = self.reference_take()
        if ref is None:
            return
        x = np.abs(self.rec.read(ref.mark - 64, ref.mark + 65)).max(axis=1)
        width = int((x >= 0.01 * ref.mark_peak).sum())
        self.path['click_width'] = width
        self.check_sides(ref)
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

    def click_at(self, ref, s, c):
        """Channel c at the wave's click s seconds in, on take ref: its peak, and the channel's peak just before it."""
        i = ref.mark + self.at(s - MARK)
        return float(np.abs(self.rec.read(i - 16, i + 17)[:, c]).max()), float(np.abs(self.rec.read(i - 1000, i - 100)[:, c]).max())

    def check_sides(self, ref):
        """The left-only and right-only clicks: each on its own side, or swapped, or leaking."""
        (L1, _), (R1, q1) = self.click_at(ref, CLICK_L, 0), self.click_at(ref, CLICK_L, 1)
        (L2, q2), (R2, _) = self.click_at(ref, CLICK_R, 0), self.click_at(ref, CLICK_R, 1)
        self.path['cross_l_db'], self.path['cross_r_db'] = db(R1 / L1) if L1 else 0.0, db(L2 / R2) if R2 else 0.0
        if self.rec.ch >= 2:
            if R1 > L1 and L2 > R2:
                self.add('LOOK', 'Left and right are swapped: the wave\'s left-only click came out on the right. Fine for levels; a comparison will have to swap them.')
            elif L1 and R2 and ((R1 > 2.0 * q1 and R1 / L1 > 1e-3) or (L2 > 2.0 * q2 and L2 / R2 > 1e-3)):        # above the channel's own noise
                self.add('LOOK', 'The channels leak: a click on one side shows on the other at %.1f and %.1f dB.' % (db(R1 / L1), db(L2 / R2)))

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
        odd = [t for t in out if t.name in self.dry]        # a wet take's first arrival is late by its effect
        if odd:
            self.add('LOOK', 'Note%s off the run\'s timing: %s.' % ('' if len(odd) == 1 else 's', ', '.join('`%s` %+.1f ms' % (t.name, t.late_ms) for t in odd)))
        unclean = [t.name for t in self.takes
                   if t.name in self.dry and t.present and not t.cut_short and not t.clean
                   and t.name not in self.silent]
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

    def run_checks(self):
        """Every check, in order: the run's own (check_run) last, once the wave is known to play at unity."""
        self.check_format()
        at_unity = self.check_unity()
        self.check_coverage()
        self.check_silence()
        if at_unity:
            self.check_path()
            self.check_trim()
            self.check_timing()
            self.check_repeat()
            self.check_run()
