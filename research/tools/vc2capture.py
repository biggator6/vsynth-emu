#!/usr/bin/env python3
"""vc2capture.py [--out DIR] [--rom4 FILE] [--rom2 FILE] [--dev 17] [--ch 1] [--input-wavs]: what the VC-2 captures need
(RECORDING_GUIDE.md, "VC-2 captures"; research/VC2_MODE_PLAN.md, step 5, what each run settles).  Writes, by default to
analysis/test_files/vc2/ (gitignored):

  vc2probe.wav         the signal into the XT's input, 44.1 kHz, 24-bit, stereo (vc2probe.py)
  vc2_test.mid         about 20 s, to see that the XT takes the messages in VC-2 mode: the input comes through as the
                       direct sound, Master Level drops during its sweep, and MFX Type changes on the screen
  vc2_run_1.mid ...    the four runs (vc2takes.py), each take with a cue at which fxplay --audio starts the input
  vc2_vsynth.mid       the same input into V-Synth mode's EXT IN OSC at each External In Type (vc2vsynth.py)
  vc2_takes.csv        every take: when it starts, what it sets in the panel's words, the input it gets, why
  vc2_run_N_in.wav     with --input-wavs: a run's whole input as one file, for a DAW that plays it beside the MIDI

The addresses are vc2map.py's: the V-Synth's map on rom.4's IDs, checked against both images here.  The runs hold
rom.4's typed defaults, sent with each type, so they stay in the gitignored folder, as the effects' runs do.

  vc2capture.py --check FILE.mid ...   parses runs back and checks every checksum (capsysex.check)
  vc2capture.py --learn FILE           names each DT1 in FILE (fxplay --in's output) by the map: the XT's own word
                                       on the map, from edits at its panel with Transmit Edit Data on"""
import argparse, csv, os, re, struct

import vc2probe
import vc2takes
import vc2vsynth
from capsysex import Run, check, nibbles
from vc2map import TYPES4, Map

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..')
FW = os.path.join(REPO, 'firmware/unpacked')
NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
EFFECT_SWITCHES = ('MFX Switch', 'Chorus Switch', 'Reverb Switch')
FIELDS = ['run', 'take', 'note_on_s', 'note_off_s', 'next_s', 'probe', 'settings', 'why']


def note_name(k):
    return '%s%d' % (NOTE_NAMES[k % 12], k // 12 - 1)


def probe_seconds(probe):
    return {'full': vc2probe.END, 'fx': vc2probe.FX_END}.get(probe, 5.0)


class Vc2Run(Run):
    """A VC-2 run: capsysex's Run writing rom.4's parameters by name through the map."""
    def __init__(self, dev, ch, m):
        super().__init__(dev, ch)
        self.map = m

    def put(self, name, v):
        if self.state.get(name) != v:
            self.sx(self.map.address(self.map.id_of(name)), [v])
            self.state[name] = v

    def apply(self, want):
        """Writes what differs from the last take: the effects' switches off first, so nothing sounds through a change;
        a type with each of its typed defaults; the rest; the switches on last.  Returns whether a program loaded."""
        reload = False
        for k in EFFECT_SWITCHES:
            if want[k] == 0:
                self.put(k, 0)
        for k in TYPES4:
            if self.state.get(k) != want[k]:
                self.put(k, want[k])
                reload = True
                for address, v in self.map.typed(k, want[k]):
                    self.sx(address, nibbles(v))
        for k, v in want.items():
            if k not in EFFECT_SWITCHES and k not in TYPES4:
                self.put(k, v)
        for k in EFFECT_SWITCHES:
            if want[k] == 1:
                self.put(k, 1)
        return reload

    def apply_take(self, t):
        """A take's settings over the base; with none, the base alone (the run's set-up)."""
        return self.apply({**vc2takes.BASE, **(t['set'] if t else {})})


def settings(m, t):
    """A take in the panel's words, what it sets over the base, for doing it by hand."""
    p = ['%s %s' % (k, m.text(m.id_of(k), v)) for k, v in t['set'].items()]
    p.append('play nothing' if t['key'] is None else 'hold %s' % note_name(t['key']))
    if t['probe'] is None:
        p.append('the input silent')
    if t.get('marker'):
        p.append('Direct Sound Level 127 for the input\'s first %.1f s (its clicks and sweep), then as set, and only '
                 'then the key' % vc2takes.MARKER_S)
    return '; '.join(p)


def build(run, takes, w, text):
    """A run on writer w (its apply_take: a take's writes, None the set-up): its take rows and cues [(ms, probe)]."""
    rows, cues = [], []
    w.wait(3000)
    w.apply_take(None)
    w.wait(2000)
    for t in takes:
        reload = w.apply_take(t)
        if t.get('marker'):
            w.put('Direct Sound Level', 127)
        w.wait(2500 if reload else 800)                     # a program's load, a mute ramp, a reverb's clear
        start = w.ms
        if t['probe']:
            w.cue('probe:%.1f' % probe_seconds(t['probe']))     # fxplay plays the input's first that many seconds
            cues.append((start, t['probe']))
        if t.get('marker'):                                 # the clicks through the direct sound, then the take's own level
            w.wait(vc2takes.MARKER_S * 1000)
            w.put('Direct Sound Level', {**vc2takes.BASE, **t['set']}['Direct Sound Level'])
        if t['key'] is not None:
            w.note(True, t['key'])
        w.wait(start + probe_seconds(t['probe']) * 1000 + 200 - w.ms)
        if t['key'] is not None:
            w.note(False, t['key'])
        off = w.ms
        w.wait(t['tail'] * 1000)
        rows.append(dict(run=run, take=t['name'], note_on_s='%.3f' % (start / 1000), note_off_s='%.3f' % (off / 1000),
                         next_s='%.3f' % (w.ms / 1000), probe=t['probe'] or '', settings=text(t), why=t['why']))
    w.wait(1000)
    return rows, cues


def test_run(m, dev, ch):
    """The test: the input as the direct sound and as the EXT IN tone on C4, so it is heard whichever word the mic is;
    Master Level down in its sweep; MFX Type to Parametric EQ and back."""
    w = Vc2Run(dev, ch, m)
    heard = {**vc2takes.BASE, 'Direct Sound Level': 127, 'OSC 1 Switch': 1, 'MFX Switch': 1}
    w.wait(1000)
    w.apply(heard)
    w.wait(1500)
    w.cue('probe:%.1f' % vc2probe.FX_END)
    w.note(True, 60)
    w.wait(8000)
    w.put('Master Level', 40)
    w.wait(1000)
    w.apply({**heard, 'Master Level': 40, 'MFX Type': 1})
    w.wait(8000)
    w.note(False, 60)
    w.put('Master Level', 127)
    w.apply(vc2takes.BASE)
    w.wait(200)
    return w


def input_wav(path, cues, total_ms, x):
    """A run's input as one 24-bit file: the probe's part at each cue, silence between; written as it goes."""
    frames = int(round(total_ms / 1000.0 * vc2probe.FS))
    fmt = struct.pack('<HHIIHH', 1, 2, vc2probe.FS, vc2probe.FS * 6, 6, 24)
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + 6 * frames) + b'WAVE' + b'fmt ' + struct.pack('<I', 16) + fmt)
        f.write(b'data' + struct.pack('<I', 6 * frames))
        at = 0
        for ms, probe in cues:
            a = int(round(ms / 1000.0 * vc2probe.FS))
            part = x[:vc2probe.at(probe_seconds(probe))]
            f.write(bytes(6 * (a - at)))
            v = (part * 8388608.0).round().clip(-8388608, 8388607).astype('<i4')
            f.write(v.view('<u1').reshape(-1, 4)[:, :3].tobytes())
            at = a + len(part)
        f.write(bytes(6 * (frames - at)))


def learn(m, path):
    """Each DT1 in the file named by the map, and anything else said as it is."""
    with open(path, errors='replace') as f:
        text = f.read()
    hexes = [int(h, 16) for h in re.findall(r'\b[0-9a-fA-F]{2}\b', text)]
    msgs, cur = [], None
    for b in hexes:
        if b == 0xf0:
            cur = [b]
        elif cur is not None:
            cur.append(b)
            if b == 0xf7:
                msgs.append(cur)
                cur = None
    print('%d SysEx messages in %s' % (len(msgs), path))
    for s in msgs:
        h = ' '.join('%02x' % b for b in s)
        if len(s) < 12 or s[1] != 0x41 or s[3:6] != [0x00, 0x53, 0x12]:
            print('  not a DT1 of model 00 53: %s' % h)
            continue
        i = m.named(s[6:10])
        data = s[10:-2]
        if i is None:
            print('  %s: an address the map has no ID for (data %s)' % (h, data))
        else:
            v = data[0] if len(data) == 1 else int(''.join('%x' % d for d in data), 16) - 32768
            print('  %s: %#05x %s = %s' % (h, i, m.names[i], m.text(i, v) if len(data) == 1 else v))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(REPO, 'analysis/test_files/vc2'))
    ap.add_argument('--rom4', default=os.path.join(FW, 'V-SynthXT.rom.4.bin'))
    ap.add_argument('--rom2', default=os.path.join(FW, 'V-SynthXT.rom.2.bin'), help="for the map's check")
    ap.add_argument('--dev', type=int, default=17, help='Device ID as the XT shows it, 17-32')
    ap.add_argument('--ch', type=int, default=1, help='MIDI channel, 1-16')
    ap.add_argument('--input-wavs', action='store_true', help="each run's input as one WAV, for a DAW")
    ap.add_argument('--check', nargs='+')
    ap.add_argument('--learn')
    a = ap.parse_args()
    if a.check:
        for p in a.check:
            check(p)
        return
    m = Map(a.rom4, a.rom2)
    if a.learn:
        return learn(m, a.learn)
    os.makedirs(a.out, exist_ok=True)
    x = vc2probe.probe()
    vc2probe.wav24(os.path.join(a.out, 'vc2probe.wav'), x)
    with open(os.path.join(a.out, 'vc2_test.mid'), 'wb') as f:
        f.write(test_run(m, a.dev - 1, a.ch - 1).smf())
    rows = []
    runs = [(run, takes(), Vc2Run(a.dev - 1, a.ch - 1, m), lambda t: settings(m, t))
            for run, takes in vc2takes.RUNS.items()]
    runs.append(('vc2_vsynth', vc2vsynth.takes(), vc2vsynth.VsRun(a.dev - 1, a.ch - 1), vc2vsynth.settings))
    for run, takes, w, text in runs:
        r, cues = build(run, takes, w, text)
        with open(os.path.join(a.out, run + '.mid'), 'wb') as f:
            f.write(w.smf())
        if a.input_wavs:
            input_wav(os.path.join(a.out, run + '_in.wav'), cues, w.ms, x)
        rows += r
        print('%s.mid: %d takes, %.1f minutes' % (run, len(r), w.ms / 60000.0))
    with open(os.path.join(a.out, 'vc2_takes.csv'), 'w', newline='') as f:
        cw = csv.DictWriter(f, fieldnames=FIELDS)
        cw.writeheader()
        cw.writerows(rows)
    print('vc2probe.wav: %.1f s, stereo, 44.1 kHz, 24-bit; parts at %s' % (
        vc2probe.END, ', '.join('%s %.1f' % s for s in vc2probe.SECTIONS)))
    print('in', os.path.abspath(a.out))


if __name__ == '__main__':
    main()
