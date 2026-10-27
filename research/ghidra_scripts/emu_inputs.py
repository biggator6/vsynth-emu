"""Inputs for EmuSolo.java from a V-Synth XT SOLO wave (vpfl 0102).

    .venv/bin/python research/ghidra_scripts/emu_inputs.py <xt_wave.wav> <raw.wav> <outprefix>

Writes <outprefix>_recs.bin (the vpdt records as stored, terminator
included), <outprefix>_samp.bin (int16 LE mono of the raw source) and
<outprefix>_cuts.txt (VSE_CUTSFILE lines for the C++ engine, terminator
dropped).  The XT wave's audio is sample-identical to the raw source for
the waves in analysis/projects/Emu_test.prj (003 sine, 004 vocal).
"""
import os, struct, sys
import numpy as np, soundfile as sf
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import vpdt_parse as vp

wave, raw, pre = sys.argv[1:4]
v = vp.chunks(wave)['vpdt']
cnt = struct.unpack('<I', v[84:88])[0]
open(pre + '_recs.bin', 'wb').write(v[88:88 + 4 * cnt])
x, sr = sf.read(raw, dtype='int16')
assert sr == 44100 and x.ndim == 1, (sr, x.shape)
x.astype('<i2').tofile(pre + '_samp.bin')
with open(pre + '_cuts.txt', 'w') as f:
    for r in vp.xt_solo_records(wave):
        f.write('%.4f %.4f %d %d\n' % (r['start'], r['len'], 1 if r['info'] & 4 else 0, r['info']))
print(cnt, 'records,', len(x), 'samples')
