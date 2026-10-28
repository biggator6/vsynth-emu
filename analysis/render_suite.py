#!/usr/bin/env python3
"""Render the batch-test suite with the offline renderer.

Usage (from the project root):
    python3 analysis/render_suite.py [--renderer analysis/variphrase_render] OUTDIR

Writes OUTDIR/sustained/<case>.wav for every reference in
analysis/test_files/sustained/.  Then score with:
    python3 analysis/batch_test.py --ref-dir analysis/test_files \
        --plugin-dir OUTDIR --output analysis/results/<version>

INPUTS (Session 16 correction): render from the RAW source that was sampled
into the V-Synth, never from a V-Synth capture.  passthrough/vocal_aah_
passthrough.wav is the V-Synth's own output at zero settings — it is not
transparent (waveform rebuilt; 8 dB below signal after gain match), and
using it as the engine input cost ~9 points on the vocal set (65.6 vs 74.6).
Chord and drum: raw/*_original.wav are byte-identical to their old
passthrough/ files (verified 2026-10-18) — those were always the originals.
Only the vocal passthrough is a V-Synth capture.
"""
import argparse, os, subprocess, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
TF = os.path.join(ROOT, 'test_files')

INPUTS = {
    'sine_440':   os.path.join(TF, 'raw', 'sine_440hz_raw.wav'),
    'vocal_aah':  os.path.join(TF, 'raw', 'solo_male_aah_raw.wav'),
    'chord_Cmaj': os.path.join(TF, 'raw', 'chord_Cmaj_original.wav'),
    'drum_hit':   os.path.join(TF, 'raw', 'drum_hit_original.wav'),
}
# Render ratios (Session 15): time_2x = twice as FAST → --time 0.5.
OPS = {
    'formant_downmax': ['--formant', '-12'], 'formant_upmax': ['--formant', '12'],
    'formant_max': ['--formant', '12'],      'formant_up4st': ['--formant', '4'],
    'pitch_down12st': ['--pitch', '-12'],    'pitch_up7st': ['--pitch', '7'],
    'time_2x': ['--time', '0.5'], 'time_4x': ['--time', '0.25'], 'time_halfspeed': ['--time', '2.0'],
    'time_quarterspeed': ['--time', '4.0'],
}

# The references were recorded with SOLO "Energy" on (output chain study,
# 2026-10-27: the fundamental sits ~1.6 dB low and sub-F0 is cut in every
# vocal reference, as the player's Energy highpass at the output fundamental
# predicts; pitch −12 / +7 fit g ≈ 0.05).  Only the SOLO player reads it.
SUITE_ENERGY = 0.05

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('outdir')
    ap.add_argument('--renderer', default=os.path.join(ROOT, 'variphrase_render'))
    ap.add_argument('--energy', type=float, default=SUITE_ENERGY, help='SOLO Energy g (0 = OFF)')
    a = ap.parse_args()
    os.makedirs(os.path.join(a.outdir, 'sustained'), exist_ok=True)
    for f in sorted(os.listdir(os.path.join(TF, 'sustained'))):
        if not f.endswith('.wav'):
            continue
        case = f[:-4]
        src = next((k for k in INPUTS if case.startswith(k + '_')), None)
        op = case[len(src) + 1:] if src else None
        if src is None or op not in OPS:
            print('skip (no mapping):', case); continue
        out = os.path.join(a.outdir, 'sustained', f)
        cmd = [a.renderer, '--input', INPUTS[src], '--output', out, '--algo', 'hybrid'] + OPS[op]
        if a.energy > 0: cmd += ['--energy', str(a.energy)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        print(f'{case}: rc={r.returncode} args={OPS[op]}')
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)

if __name__ == '__main__':
    main()
