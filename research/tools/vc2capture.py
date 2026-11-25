#!/usr/bin/env python3
"""vc2capture.py [--out DIR] [--rom4 FILE] [--writer FILE] [--dev 17] [--ch 1] [--input-wavs]: what the VC-2 captures
need (RECORDING_GUIDE.md, "VC-2 captures"; research/VC2_MODE_PLAN.md, step 5, what each run settles).  Writes, by default
to analysis/test_files/vc2/ (gitignored):

  vc2probe.wav         the signal into the XT's input, 44.1 kHz, 24-bit, stereo (vc2probe.py)
  card/                what goes on the XT's card: "VC-2 Project.ini" and RUNS.vc2/Patch/NNN RUN NNN.p02, the patches
                       the runs select (vc2cardrun.py), made by rom.4's own routines through vc2patchwrite
  vc2_test.mid         about 23 s, to see that the XT takes the messages in VC-2 mode: patch RUN 001 from the card, the
                       input through as the direct sound, Master Level dropping during its sweep, MFX Type changing on
                       the screen
  vc2_run_1.mid ...    the four runs (vc2takes.py), each take with a cue at which fxplay --audio starts the input; a
                       take that sets what only the panel reaches (the system's Output Gain and External In Type) has a
                       pause before it, at a "hand:" cue fxplay shows, for the user to set that at the panel
  vc2_vsynth.mid       the same input into V-Synth mode's EXT IN OSC at each External In Type (vc2vsynth.py)
  vc2_takes.csv        every take: when it starts, what it sets in the panel's words, the input it gets, why
  vc2_run_N_in.wav     with --input-wavs: a run's whole input as one file, for a DAW that plays it beside the MIDI

The addresses are vc2map.py's: rom.4's own map, model 00 00 0D, read from the image (HARDWARE_FIRMWARE.md, "rom.4's
SysEx: the VC-2's own address map").  What rom.4 takes no SysEx for under PATCH Init's Category goes in the patches,
each take selecting its own by Bank Select and Program Change (the user's choice, 2026-11-25).  The runs and the
patches hold rom.4's defaults, so they stay in the gitignored folder, as the effects' runs do.

  vc2capture.py --check FILE.mid ...   parses runs back and checks every checksum (capsysex.check)
  vc2capture.py --learn FILE           names each DT1 in FILE (fxplay --in's output) by rom.4's map: the XT's own
                                       word, from edits at its panel with Transmit Edit Data on"""
import argparse, csv, os, re, struct

import vc2probe
import vc2takes
import vc2vsynth
from capsysex import check
from vc2cardrun import INI_NAME, Patches
from vc2map import MODEL, Map
from vc2writer import TEST_SETS, Vc2Run, build, probe_seconds, settings, test_run

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..')
FW = os.path.join(REPO, 'firmware/unpacked')
WRITER = os.path.join(REPO, 'build/tools/bin/vc2patchwrite')   # build.sh builds it
FIELDS = ['run', 'take', 'note_on_s', 'note_off_s', 'next_s', 'probe', 'settings', 'why']


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
    head = 3 + len(MODEL)
    for s in msgs:
        h = ' '.join('%02x' % b for b in s)
        if len(s) < head + 8 or s[1] != 0x41 or s[3:head + 1] != MODEL + [0x12]:
            print('  not a DT1 of model 00 00 0D: %s' % h)
            continue
        i = m.named(s[head + 1:head + 5])
        data = s[head + 5:-2]
        if i is None:
            print('  %s: an address the map has no ID for (data %s)' % (h, data))
        else:
            v = data[0] if len(data) == 1 else int(''.join('%x' % d for d in data), 16) - 32768
            print('  %s: %#05x %s = %s' % (h, i, m.names[i], m.text(i, v) if len(data) == 1 else v))


def unreachable(m):
    """What the test and the VC-2 runs set that rom.4 takes no DT1 for under PATCH Init's Category: no address, or one
    whose entry writes under other Categories alone (vc2map.py)."""
    wanted = set(vc2takes.BASE) | set(TEST_SETS)
    for takes in vc2takes.RUNS.values():
        for t in takes():
            wanted |= set(t['set'])
    category = vc2takes.BASE.get('Patch Category', m.descriptor(m.id_of('Patch Category'))[1])
    out = []
    for name in sorted(wanted):
        ids = [i for i, n in enumerate(m.names) if n == name]
        if not any(category in v[4] for i in ids for v in m.addresses(i)):
            out.append(name)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(REPO, 'analysis/test_files/vc2'))
    ap.add_argument('--rom4', default=os.path.join(FW, 'V-SynthXT.rom.4.bin'))
    ap.add_argument('--writer', default=WRITER, help='vc2patchwrite, built by its own line')
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
    m = Map(a.rom4)
    if a.learn:
        return learn(m, a.learn)
    if not os.access(a.writer, os.X_OK):
        raise SystemExit('no %s: research/tools/build.sh builds it' % a.writer)
    os.makedirs(a.out, exist_ok=True)
    x = vc2probe.probe()
    vc2probe.wav24(os.path.join(a.out, 'vc2probe.wav'), x)
    patches = Patches(m, vc2takes.BASE, unreachable(m))
    with open(os.path.join(a.out, 'vc2_test.mid'), 'wb') as f:
        f.write(test_run(m, a.dev - 1, a.ch - 1, patches).smf())
    runs = [(run, takes(), Vc2Run(a.dev - 1, a.ch - 1, m, patches), lambda t: settings(m, t, patches))
            for run, takes in vc2takes.RUNS.items()]
    runs.append(('vc2_vsynth', vc2vsynth.takes(), vc2vsynth.VsRun(a.dev - 1, a.ch - 1), vc2vsynth.settings))
    write_runs(a, runs, x)
    folder = patches.write(os.path.join(a.out, 'card'), a.writer, a.rom4)
    print('card/: %s and %d patches in %s; by hand at the panel: %s' % (
        INI_NAME, len(patches.numbers), os.path.relpath(folder, a.out), ', '.join(patches.by_hand)))


def write_runs(a, runs, x):
    """Each run's MIDI (and with --input-wavs its input), then the take list."""
    rows = []
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
