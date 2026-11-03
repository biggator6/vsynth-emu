#!/usr/bin/env python3
"""make_inputs.py <out dir>: the raw int16 test inputs the firmware checks and
A/B renders read (vocal, drum, chord, sine, w358 .bin), from the project's WAVs.
24-bit sources are reduced to int16 by dropping the low byte.  gated.bin and the
SOLO encoder check's enc_*.bin are generated."""
import math, random, struct, sys, os

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

# A gated tone with vibrato.  The firmware's SOLO encoder ends its bursts with records of a few
# samples, which a backward note's repeat leaves only by overflowing (HARDWARE_FIRMWARE.md,
# "The anchor's overflow").
ph, tone = 0.0, bytearray()
for t in range(30000):
    ph += 2335.7 * (1 + 0.1 * math.sin(2 * math.pi * 18.44 * t / 44100)) / 44100
    x = math.sin(2 * math.pi * ph) if math.sin(2 * math.pi * 33.85 * t / 44100) > 0 else 0.0
    tone += struct.pack('<h', int(22936 * x))
open(os.path.join(out, 'gated.bin'), 'wb').write(bytes(tone))

# The SOLO encoder's check (fcheck's xt_enc, HARDWARE_FIRMWARE.md "The SOLO encoder against the
# firmware's"): generated waves, plain Python so that they come out the same every time.
def enc(name, xs):
    b = bytearray()
    for x in xs: b += struct.pack('<h', max(-32768, min(32767, int(x))))
    open(os.path.join(out, 'enc_' + name + '.bin'), 'wb').write(bytes(b))
def wave(name):
    p = os.path.join(out, name + '.bin')
    if not os.path.exists(p): return []
    d = open(p, 'rb').read()
    return list(struct.unpack('<%dh' % (len(d) // 2), d[:len(d) // 2 * 2]))
def mixed(name):                                  # stereo as the XT's reader takes it
    x = wave(name)
    return [(x[i] + x[i + 1]) >> 1 for i in range(0, len(x) - 1, 2)]
TWO_PI, N = 2 * math.pi, 66150
def sine(f, a, n=N): return [a * 32767 * math.sin(TWO_PI * f * t / 44100) for t in range(n)]
def saw(f, a, n=N): return [a * 32767 * (2 * ((t * f / 44100) % 1.0) - 1) for t in range(n)]
def square(f, a, n=N): return [a * 32767 * (1 if math.sin(TWO_PI * f * t / 44100) > 0 else -1 if math.sin(TWO_PI * f * t / 44100) < 0 else 0) for t in range(n)]
def gate(x, f, ph): return [v if math.sin(TWO_PI * f * t / 44100 + ph) > 0 else 0 for t, v in enumerate(x[:N])]
def chirp(f0, f1, a, n=N):
    ph, r = 0.0, []
    for t in range(n):
        ph += f0 * (f1 / f0) ** (t / n) / 44100
        r.append(a * 32767 * math.sin(TWO_PI * ph))
    return r
# Loud sines and saws: the silence test's sum wraps.
for name, xs in (('sin440', sine(440, 0.7)), ('sin220', sine(220, 0.7)), ('sin880', sine(880, 0.7)), ('saw440', saw(440, 0.7)),
                 ('saw220', saw(220, 0.7)), ('sin440b', sine(440, 0.55)), ('sin440c', sine(440, 0.9))):
    enc(name, xs)
# Gated material: the fills between runs, their crossings and limits.
vocal, chord, drum, w358 = wave('vocal'), wave('chord'), mixed('drum'), mixed('w358')
for name, x in (('vocal', vocal), ('chord', chord), ('drum', drum), ('w358', w358)):
    if not x: continue
    for f, ph in ((3, 0.3), (7, 1.1), (17, 2.3), (47, 0.0)):
        enc('%s_g%d' % (name, f), gate(x, f, ph))
    y = x[:N]
    enc(name + '_quiet', [v * 0.05 for v in y])
    enc(name + '_clip', [v * 8 for v in y])
    enc(name + '_rev', y[::-1])
    enc(name + '_pad', [0] * 5000 + y[:20000] + [0] * 5000 + y[20000:40000])
# Very low notes: long lags, reads past the buffers.  Falling and rising, fading: the first marks.
enc('saw40', saw(40, 0.5)); enc('square40', square(40, 0.5)); enc('saw55', saw(55, 1.0))
enc('chirpd', chirp(2000, 60, 0.7)); enc('chirpu', chirp(60, 2000, 0.2))
enc('fade', [0.2 * 32767 * (t / N) * math.sin(TWO_PI * 330 * t / 44100) for t in range(N)])
enc('decay', [0.7 * 32767 * math.exp(-t / 6000) * math.sin(TWO_PI * 330 * t / 44100) for t in range(N)])
# Fast falls: the filter's coefficients to the last bit, and a wave that ends on a voiced mark with room
# for one more period (the end mark).  A fading tone on a negative constant: drift scans past their window,
# into the buffer's earlier contents and the variable under it.
for f0, f1, n in ((1500, 150, 4006), (1500, 150, 4048), (400, 60, 8087), (400, 60, 8222), (800, 100, 6156)):
    enc('fall%d_%d' % (f0, n), chirp(f0, f1, 0.7, n))
enc('dcfade100', [-5487 + 20000 * math.exp(-t / 4000) * math.sin(TWO_PI * 100 * t / 44100) for t in range(88000)])
enc('dcfade800', [-12000 + 6000 * math.exp(-t / 4000) * math.sin(TWO_PI * 800 * t / 44100) for t in range(88000)])
# Short waves: the shorter lag and filter under 2004 samples, no list at 2 and 2004 to 2005,
# a pool of none under 160.
for n in (2, 3, 40, 100, 159, 160, 1000, 2003, 2004, 2005, 2006, 4000, 12288, 14000):
    enc('n%d' % n, sine(440, 0.7, n))
    enc('v%d' % n, sine(220, 0.2, n))
# Silence, a constant, clicks.
enc('zeros', [0] * N); enc('dc', [12000] * N); enc('dcmax', [32767] * N)
enc('clicks', [32767 if t % 4410 == 0 else 0 for t in range(N)])
enc('alt', [32767 if t & 1 else -32767 for t in range(N)])
# Noise, and tones in noise and bursts, from a seed.
rnd = random.Random(2011)
enc('noise', [rnd.uniform(-16000, 16000) for t in range(N)])
for i in range(12):
    n, f0, a = rnd.randint(5000, 90000), 10 ** rnd.uniform(1.3, 3.5), 32767 * 10 ** rnd.uniform(-2.5, 0.2)
    amps = [rnd.uniform(0, 1) / h for h in range(1, rnd.randint(2, 11))]
    phs = [rnd.uniform(0, 6) for _ in amps]
    vib, depth, gf, gp, gth = rnd.uniform(0.2, 12), rnd.uniform(0, 0.05), rnd.uniform(1, 60), rnd.uniform(0, 6), rnd.uniform(-1.5, 0.5)
    noisy = rnd.uniform(0, 0.3) if i % 3 == 0 else 0.0
    ph, xs = 0.0, []
    for t in range(n):
        ph += f0 * (1 + depth * math.sin(TWO_PI * vib * t / 44100)) / 44100
        v = sum(p * math.sin(TWO_PI * h * ph + q) for h, (p, q) in enumerate(zip(amps, phs), 1)) + noisy * rnd.uniform(-1, 1)
        xs.append(a * v if math.sin(TWO_PI * gf * t / 44100 + gp) > gth else 0)
    enc('mix%02d' % i, xs)
