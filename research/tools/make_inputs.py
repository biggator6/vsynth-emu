#!/usr/bin/env python3
"""make_inputs.py <out dir>: the raw int16 test inputs the firmware checks and
A/B renders read (vocal, drum, chord, sine, w358 .bin), from the project's WAVs.
24-bit sources are reduced to int16 by dropping the low byte."""
import struct, sys, os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCES = {
    'vocal': 'analysis/test_files/raw/solo_male_aah_raw.wav',
    'drum':  'analysis/test_files/raw/drum_hit_original.wav',
    'chord': 'analysis/test_files/raw/chord_Cmaj_original.wav',
    'sine':  'analysis/test_files/raw/sine_440hz_raw.wav',
    'w358':  'analysis/projects/Emu_test.prj/Wave/358 Synth002.wav',
}

def pcm16(path):
    d = open(path, 'rb').read()
    i, bits, data = 12, 16, None
    while i + 8 <= len(d):
        cid, size = d[i:i + 4], struct.unpack_from('<I', d, i + 4)[0]
        if cid == b'fmt ': bits = struct.unpack_from('<H', d, i + 22)[0]
        if cid == b'data': data = d[i + 8:i + 8 + size]
        i += 8 + size + (size & 1)
    if bits == 16: return data
    if bits == 24: return b''.join(data[k + 1:k + 3] for k in range(0, len(data) - 2, 3))
    raise SystemExit(f'{path}: {bits}-bit not handled')

out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, 'build/tools')
os.makedirs(out, exist_ok=True)
for name, rel in SOURCES.items():
    src = os.path.join(REPO, rel)
    if not os.path.exists(src): print(f'skip {name}: {rel} missing'); continue
    open(os.path.join(out, name + '.bin'), 'wb').write(pcm16(src))
