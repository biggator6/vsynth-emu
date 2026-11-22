#!/usr/bin/env python3
"""capreport.py: a cutter's report and files: the table of takes, the checks, the verdict, levels.csv, report.txt and a WAV
per take in the recording's own bytes.  `Report` is a part of a cutter's analysis; `cli` is a cutter's command line.
Split from fxcut.py as it stood."""
import argparse, csv, os, struct, sys
import numpy as np

from caprec import Bad
from capbase import CUT_LEAD, STEP_DB


class Report:
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
        nxt = self.next_step()
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


def cli(analyse, description, runs, takes_default):
    """A cutter's command line: analyse(recording, run, takes, from, probe, take) -> its analysis; exit 2 for a file
    it can't read, 1 for any FAIL, else 0."""
    ap = argparse.ArgumentParser(description=description)
    ap.add_argument('recording')
    ap.add_argument('--run', choices=runs, help='which run; by default, from the file name')
    ap.add_argument('--from', dest='start', type=float, help="a recording made with fxplay --from: the next_s it started at")
    ap.add_argument('--take', help='a recording of this take alone')
    ap.add_argument('--takes', help='the take list; by default %s' % takes_default)
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
