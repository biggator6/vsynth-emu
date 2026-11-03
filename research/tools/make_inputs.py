#!/usr/bin/env python3
"""make_inputs.py <out dir>: the raw int16 test inputs the firmware checks and
A/B renders read (vocal, drum, chord, sine, w358 .bin), from the project's WAVs.
24-bit sources are reduced to int16 by dropping the low byte.  gated.bin and the
SOLO encoder check's enc_*.bin (mono) and enc2_*.bin (stereo) are generated, and
enc2_prj_*.bin are the project's own stereo waves."""
import glob, math, random, struct, sys, os

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

# The encoder's stereo mode (fcheck's xt_enc, each with :2; HARDWARE_FIRMWARE.md "The SOLO encoder's
# stereo mode").  The project's stereo waves as they are, the wave's number for a name.
def channels(path):
    d = open(path, 'rb').read()
    i = 12
    while i + 8 <= len(d):
        if d[i:i + 4] == b'fmt ': return struct.unpack_from('<H', d, i + 10)[0]
        i += 8 + struct.unpack_from('<I', d, i + 4)[0]
    return 0
for p in sorted(glob.glob(os.path.join(REPO, 'analysis/projects/Emu_test.prj/Wave/*.wav'))):
    if os.path.getsize(p) and channels(p) == 2:
        open(os.path.join(out, 'enc2_prj_' + os.path.basename(p)[:3] + '.bin'), 'wb').write(pcm16(p))
def enc2(name, l, r):
    b = bytearray()
    for u, v in zip(l, r): b += struct.pack('<hh', max(-32768, min(32767, int(u))), max(-32768, min(32767, int(v))))
    open(os.path.join(out, 'enc2_' + name + '.bin'), 'wb').write(bytes(b))
def chans(name):
    x = wave(name)
    return x[0::2], x[1::2]
# Gated, quiet, clipped, reversed and padded, from real stereo, two sources, and a source against itself a
# sample later, which keeps the sum odd half the time.
drumL, drumR = chans('drum')
w358L, w358R = chans('w358')
for name, l, r in (('drum', drumL, drumR), ('w358', w358L, w358R), ('vc', vocal, chord), ('vv', vocal, vocal[1:] + [0])):
    if not l or not r: continue
    for f, g, ph in ((3, 3, 0.3), (7, 17, 1.1), (47, 11, 2.3)):
        enc2('%s_g%d' % (name, f), gate(l, f, ph), gate(r, g, ph))
    y, z = l[:N], r[:N]
    enc2(name + '_quiet', [v * 0.05 for v in y], [v * 0.05 for v in z])
    enc2(name + '_clip', [v * 8 for v in y], [v * 8 for v in z])
    enc2(name + '_rev', y[::-1], z[::-1])
    enc2(name + '_pad', [0] * 5000 + y[:20000] + [0] * 5000 + y[20000:40000], [0] * 5000 + z[:20000] + [0] * 5000 + z[20000:40000])
    enc2(name + '_swap', z, y)
# The sum odd everywhere: the samples floor, the crossing scan truncates, and they differ below zero.  So too
# gaps that hold only -1, 0 and 1: the fills step across them with that scan.
for name, x in (('sin', sine(220, 0.7)), ('saw', saw(110, 0.4)), ('chirp', chirp(1500, 80, 0.6)), ('vocal', vocal)):
    if not x: continue
    enc2('odd_' + name, x, [v - 1 for v in x])
    enc2('oddp_' + name, x, [v + 1 for v in x])
    d = random.Random(len(name))
    y = gate(x, 7, 1.1)
    enc2('dith_' + name, y, [v if int(v) else d.choice((-1, 0, 1)) for v in y])
    enc2('anti_' + name, x, [-v - 1 for v in x])
# Both channels the same: mono mode's records, by the arithmetic.
for name, x in (('sin440', sine(440, 0.7)), ('vocal', vocal), ('gate', gate(chord, 17, 2.3))):
    if x: enc2('same_' + name, x, x)
# Loud, detuned, in one channel only; low notes, fast falls, fades on a constant.
enc2('sin440', sine(440, 0.7), sine(443, 0.6)); enc2('sin440c', sine(440, 0.9), sine(440, 0.88)); enc2('saw440', saw(440, 0.7), saw(441, 0.7))
enc2('left', sine(330, 0.8), [0] * N); enc2('right', [0] * N, saw(330, 0.8))
enc2('saw40', saw(40, 0.5), saw(40.2, 0.5)); enc2('square40', square(40, 0.5), square(40, 0.3))
enc2('chirpd', chirp(2000, 60, 0.7), chirp(2000, 62, 0.6)); enc2('chirpu', chirp(60, 2000, 0.2), chirp(61, 2000, 0.3))
for f0, f1, n in ((1500, 150, 4006), (1500, 150, 4048), (400, 60, 8087), (800, 100, 6156)):
    enc2('fall%d_%d' % (f0, n), chirp(f0, f1, 0.7, n), chirp(f0, f1, 0.65, n))
enc2('dcfade100', [-5487 + 20000 * math.exp(-t / 4000) * math.sin(TWO_PI * 100 * t / 44100) for t in range(88000)],
     [-5486 + 18000 * math.exp(-t / 4000) * math.sin(TWO_PI * 100 * t / 44100) for t in range(88000)])
enc2('decay', [0.7 * 32767 * math.exp(-t / 6000) * math.sin(TWO_PI * 330 * t / 44100) for t in range(N)],
     [0.6 * 32767 * math.exp(-t / 5000) * math.sin(TWO_PI * 331 * t / 44100) for t in range(N)])
# Short waves: all of them near the ends, and the first shift.
for n in (2, 3, 40, 100, 159, 160, 1000, 2003, 2004, 2005, 2006, 4000, 12288, 14000):
    enc2('n%d' % n, sine(440, 0.7, n), sine(447, 0.6, n))
# Silence, constants at the ends of the range, one below zero by half, clicks in one channel.
enc2('zeros', [0] * N, [0] * N); enc2('dc', [12000] * N, [12001] * N); enc2('dcmax', [32767] * N, [32767] * N)
enc2('dcmin', [-32768] * N, [-32768] * N); enc2('dchalf', [-1] * N, [0] * N)
enc2('clicks', [32767 if t % 4410 == 0 else 0 for t in range(N)], [0] * N)
enc2('alt', [32767 if t & 1 else -32767 for t in range(N)], [-32768 if t & 1 else 32767 for t in range(N)])
# Noise, and tones in noise and bursts, from a seed: a phase and a level apart in the channels.
rnd = random.Random(2113)
enc2('noise', [rnd.uniform(-16000, 16000) for t in range(N)], [rnd.uniform(-16000, 16000) for t in range(N)])
for i in range(12):
    n, f0, a = rnd.randint(5000, 90000), 10 ** rnd.uniform(1.3, 3.5), 32767 * 10 ** rnd.uniform(-2.5, 0.2)
    amps = [rnd.uniform(0, 1) / h for h in range(1, rnd.randint(2, 11))]
    phs = [rnd.uniform(0, 6) for _ in amps]
    vib, depth, gf, gp, gth = rnd.uniform(0.2, 12), rnd.uniform(0, 0.05), rnd.uniform(1, 60), rnd.uniform(0, 6), rnd.uniform(-1.5, 0.5)
    noisy, gain, shift = rnd.uniform(0, 0.3) if i % 3 == 0 else 0.0, rnd.uniform(0.3, 1), rnd.uniform(0, 1.5)
    ph, l, r = 0.0, [], []
    for t in range(n):
        ph += f0 * (1 + depth * math.sin(TWO_PI * vib * t / 44100)) / 44100
        on = math.sin(TWO_PI * gf * t / 44100 + gp) > gth
        u = sum(p * math.sin(TWO_PI * h * ph + q) for h, (p, q) in enumerate(zip(amps, phs), 1)) + noisy * rnd.uniform(-1, 1)
        v = sum(p * math.sin(TWO_PI * h * ph + q + shift) for h, (p, q) in enumerate(zip(amps, phs), 1)) + noisy * rnd.uniform(-1, 1)
        l.append(a * u if on else 0); r.append(a * gain * v if on else 0)
    enc2('mix%02d' % i, l, r)
