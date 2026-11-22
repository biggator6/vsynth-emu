#!/usr/bin/env python3
"""vc2probe.py: the signal played into the XT's input for the VC-2 captures (RECORDING_GUIDE.md, "VC-2 captures";
research/VC2_MODE_PLAN.md, step 5).  vc2capture.py writes it as vc2probe.wav; fxplay plays it at each take's cue.

Its first 16.8 s are fxcapture.probe() sample for sample (the mark, the left-only and right-only clicks, the sweep, the
1 kHz steps, the noise), so the cutters' shared parts (capbase.py and the rest) read a VC-2 take as they read an effects
take.  After them comes a voice for the vocoder and the pitch tracker, made by additive synthesis: each harmonic of a
glottal source at its formant envelope's gain, so nothing here depends on a filter's or a library's rounding.

  probe()          the whole signal, stereo float at 44.1 kHz, full scale 1
  SECTIONS         where each part starts, for the cutter's report
  STEPS_F0         the pitch staircase's fundamentals, for the tracker's and Auto Note's checks
Every part is deterministic, so the file can be made again exactly."""
import math
import struct
import numpy as np

import fxcapture

FS = fxcapture.FS
FX_END = 16.8                       # where fxcapture.probe() ends: an effects take plays this much
END = 32.5                          # the whole signal: a take with the voice plays this much
SIDES = (17.5, 19.5)                # the vowel at 220 Hz on the left, at 330 Hz on the right: which side each path hears
VOWEL_A, VOWEL_I = (20.0, 22.0), (22.0, 23.0)
STEPS = (23.5, 26.7)                # the pitch staircase, 0.4 s a step
STEPS_F0 = [110.0, 146.83, 220.0, 220.0 * 2 ** (30 / 1200), 220.0 * 2 ** (50 / 1200), 329.63, 440.0, 880.0]
GLIDE = (27.0, 28.5)                # 80 Hz to 1 kHz, log: the tracker's range (65.4 to 1480 Hz, VC2_VOCAL_DESIGNER.md)
UNVOICED_S, UNVOICED_SH = (29.0, 29.4), (29.6, 30.0)
LADDER = (30.5, 31.7)               # the vowel at -42 to -12 dBFS rms in 6 dB steps of 0.2 s: the detectors' levels
LADDER_DB = [-42, -36, -30, -24, -18, -12]
VOICE_DB = -18.0                    # the vowel's rms everywhere else
SECTIONS = list(fxcapture.SECTIONS[:-1]) + [
    ('sides', SIDES[0]), ('vowel_a', VOWEL_A[0]), ('vowel_i', VOWEL_I[0]), ('pitch_steps', STEPS[0]),
    ('glide', GLIDE[0]), ('unvoiced', UNVOICED_S[0]), ('ladder', LADDER[0]), ('end', END)]

FORMANTS = {                        # (frequency, bandwidth) in Hz: an adult male's, the textbook's round values
    'a': [(730.0, 90.0), (1090.0, 110.0), (2440.0, 160.0), (3400.0, 250.0)],
    'i': [(270.0, 60.0), (2290.0, 100.0), (3010.0, 170.0), (3700.0, 250.0)]}
TOP_HZ = 7000.0                     # no harmonic above this, so nothing nears the input's or the vocoder's band edge


def at(t):
    return int(round(t * FS))


def envelope(f, vowel):
    """The formant envelope's gain at each frequency in f: second-order resonances, each 1 at its peak, summed."""
    g = np.zeros_like(f)
    for fc, bw in FORMANTS[vowel]:
        r = f / fc
        g += 1.0 / np.sqrt((1.0 - r * r) ** 2 + (r * bw / fc) ** 2) * (bw / fc)
    return g


def voiced(f0, vowel):
    """A vowel on a fundamental per sample (an array): each harmonic below TOP_HZ at the envelope's gain, with a source
    falling 12 dB an octave, the phase running from 0.  Returns the samples at VOICE_DB rms."""
    phase = 2.0 * math.pi * np.cumsum(f0) / FS
    out = np.zeros(len(f0))
    for k in range(1, int(TOP_HZ / float(f0.min())) + 1):
        fk = k * f0
        out += np.where(fk < TOP_HZ, envelope(fk, vowel) / (k * k), 0.0) * np.sin(k * phase)
    return out * (10 ** (VOICE_DB / 20.0) / math.sqrt(float(np.mean(out * out))))


def faded(x, ms=10.0):
    """x with a raised-cosine fade in and out, so a part starts and stops without a click of its own."""
    n = min(len(x) // 2, int(ms * FS / 1000.0))
    w = 0.5 - 0.5 * np.cos(math.pi * np.arange(n) / n)
    x = x.copy()
    x[:n] *= w
    x[len(x) - n:] *= w[::-1]
    return x


def steady(f0, span, vowel='a'):
    return faded(voiced(np.full(at(span[1]) - at(span[0]), f0), vowel))


def noise(n, seed):
    """xorshift32, as fxcapture's noise, uniform in [-1, 1)."""
    x, out = seed, np.empty(n)
    for i in range(n):
        x ^= (x << 13) & 0xffffffff
        x ^= x >> 17
        x ^= (x << 5) & 0xffffffff
        out[i] = x / 2147483648.0 - 1.0
    return out


def unvoiced(span, seed, diffs):
    """A fricative: noise differenced `diffs` times (each +6 dB an octave), at VOICE_DB rms."""
    x = noise(at(span[1]) - at(span[0]) + diffs, seed)
    for _ in range(diffs):
        x = x[1:] - x[:-1]
    return faded(x * (10 ** (VOICE_DB / 20.0) / math.sqrt(float(np.mean(x * x)))))


def voice():
    """The voice part, both channels, from FX_END to END."""
    L, R = np.zeros(at(END)), np.zeros(at(END))
    def put(span, left, right=None):
        a = at(span[0])
        L[a:a + len(left)] += left
        R[a:a + len(left)] += left if right is None else right
    put(SIDES, steady(220.0, SIDES), steady(330.0, SIDES))
    put(VOWEL_A, steady(220.0, VOWEL_A))
    put(VOWEL_I, steady(220.0, VOWEL_I, 'i'))
    step = 0.4
    for k, f0 in enumerate(STEPS_F0):
        s = STEPS[0] + step * k
        put((s, s + step), steady(f0, (s, s + step)))
    n = at(GLIDE[1]) - at(GLIDE[0])
    put(GLIDE, faded(voiced(80.0 * (1000.0 / 80.0) ** (np.arange(n) / float(n)), 'a')))
    put(UNVOICED_S, unvoiced(UNVOICED_S, 2463534242, 2))
    put(UNVOICED_SH, unvoiced(UNVOICED_SH, 88172645, 1))
    for k, d in enumerate(LADDER_DB):
        s = LADDER[0] + 0.2 * k
        put((s, s + 0.2), steady(220.0, (s, s + 0.2)) * 10 ** ((d - VOICE_DB) / 20.0))
    return np.stack([L, R], axis=1)[at(FX_END):]


def probe():
    """The whole signal, stereo, full scale 1: fxcapture.probe()'s samples, then the voice."""
    frames, _ = fxcapture.probe()
    fx = np.frombuffer(frames, dtype='<i2').reshape(-1, 2) / 32768.0
    return np.concatenate([fx, voice()])


def wav24(path, x):
    """x (stereo float, full scale 1) as a 24-bit WAV at 44.1 kHz; fxprobe's int16 samples land exactly (<< 8)."""
    v = np.clip(np.round(x * 8388608.0), -8388608, 8388607).astype('<i4')
    body = np.frombuffer(v.tobytes(), dtype=np.uint8).reshape(-1, 4)[:, :3].tobytes()
    fmt = struct.pack('<HHIIHH', 1, 2, FS, FS * 6, 6, 24)
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 4 + 24 + 8 + len(body)) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(body)) + body)
